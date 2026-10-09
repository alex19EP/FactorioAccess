#include "audio.h"

#include "log.h"
#include "speech.h"

#include <miniaudio.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <numbers>
#include <thread>
#include <unordered_map>
#include <utility>

namespace fa::audio {

double Parameter::at(double seconds) const {
   if (points.empty()) return constant;
   if (seconds <= points.front().time) return points.front().value;
   if (seconds >= points.back().time) return points.back().value;
   for (size_t i = 1; i < points.size(); ++i) {
      const Point& from = points[i - 1];
      const Point& to = points[i];
      if (seconds >= to.time) continue;
      if (to.jump || to.time <= from.time) return from.value;
      return from.value + (seconds - from.time) / (to.time - from.time) * (to.value - from.value);
   }
   return points.back().value;
}

namespace {

// Volume and pan reach the audio thread through atomics and glide there over this many frames, so
// a change never clicks.
constexpr ma_uint32 kGlideFrames = 256;

struct Glide {
   float current = 0;
   float step = 0;
   ma_uint32 left = 0;
   float target = 0;

   void reset(float value) {
      current = target = value;
      left = 0;
   }
   void retarget(float value) {
      if (value == target) return;
      target = value;
      step = (value - current) / kGlideFrames;
      left = kGlideFrames;
   }
   float next() {
      if (left > 0 && --left == 0)
         current = target;
      else if (left > 0)
         current += step;
      return current;
   }
};

// Mono in, stereo out: the sound's volume, then equal-power panning (left cos, right sin of
// (pan + 1) * pi / 4).
struct PannerNode {
   ma_node_base base; // first: miniaudio treats the struct as its node
   std::atomic<float> gain{1};
   std::atomic<float> pan{0};
   Glide gainGlide; // audio thread only
   Glide panGlide;

   static void process(ma_node* node, const float** in, ma_uint32*, float** out, ma_uint32* frames) {
      auto* self = reinterpret_cast<PannerNode*>(node);
      self->gainGlide.retarget(self->gain.load(std::memory_order_relaxed));
      self->panGlide.retarget(self->pan.load(std::memory_order_relaxed));
      const float* mono = in[0];
      float* stereo = out[0];
      for (ma_uint32 i = 0; i < *frames; ++i) {
         const float gain = self->gainGlide.next();
         const float theta = (self->panGlide.next() + 1.0f) * 0.25f * std::numbers::pi_v<float>;
         stereo[i * 2] = mono[i] * gain * std::cos(theta);
         stereo[i * 2 + 1] = mono[i] * gain * std::sin(theta);
      }
   }

   inline static ma_node_vtable vtable{&process, nullptr, 1, 1, 0};

   ma_result init(ma_node_graph* graph, float initialGain, float initialPan) {
      gain = initialGain;
      pan = initialPan;
      gainGlide.reset(initialGain);
      panGlide.reset(initialPan);
      ma_uint32 inChannels = 1;
      ma_uint32 outChannels = 2;
      ma_node_config config = ma_node_config_init();
      config.vtable = &vtable;
      config.pInputChannels = &inChannels;
      config.pOutputChannels = &outChannels;
      return ma_node_init(graph, &config, nullptr, &base);
   }
};

float clampPan(double pan) { return static_cast<float>(std::clamp(pan, -1.0, 1.0)); }

// One playing sound and its graph: source -> panner -> endpoint, or with a filter
// source -> splitter -> (panner, lpf -> filtered panner) -> endpoint, blended by the panners'
// output volumes. Every miniaudio object here stays at its address, so a Sound never moves.
struct Sound {
   std::string id;
   std::shared_ptr<const std::string> bytes; // the decoder reads them in place
   ma_decoder decoder{};
   ma_waveform waveform{};
   bool hasDecoder = false;
   bool hasWaveform = false;
   ma_sound sound{};
   bool hasSound = false;
   PannerNode dry{};
   bool hasDry = false;
   ma_splitter_node splitter{};
   bool hasSplitter = false;
   ma_lpf_node lpf{};
   bool hasLpf = false;
   PannerNode wet{};
   bool hasWet = false;

   Parameter volume;
   Parameter pan;
   Parameter playbackRate;
   std::optional<Parameter> filterGain;
   bool looping = false;
   ma_uint64 startFrame = 0;
   std::optional<ma_uint64> stopFrame;

   ~Sound() {
      if (hasWet) ma_node_uninit(&wet.base, nullptr);
      if (hasDry) ma_node_uninit(&dry.base, nullptr);
      if (hasLpf) ma_lpf_node_uninit(&lpf, nullptr);
      if (hasSplitter) ma_splitter_node_uninit(&splitter, nullptr);
      if (hasSound) ma_sound_uninit(&sound);
      if (hasDecoder) ma_decoder_uninit(&decoder);
      if (hasWaveform) ma_waveform_uninit(&waveform);
   }
};

bool check(ma_result result, const char* what, const std::string& id) {
   if (result == MA_SUCCESS) return true;
   log::error("Audio: {} failed for sound {}: {}", what, id, ma_result_description(result));
   return false;
}

class Engine {
public:
   bool init() {
      ma_engine_config config = ma_engine_config_init();
      config.channels = 2; // the panners' stereo
      if (ma_result result = ma_engine_init(&config, &engine_); result != MA_SUCCESS) {
         log::error("Audio: no output device: {}", ma_result_description(result));
         return false;
      }
      rate_ = ma_engine_get_sample_rate(&engine_);
      log::info("Audio: {} Hz", rate_);
      return true;
   }

   void run(const Command& command) {
      std::visit([&](const auto& c) { apply(c); }, command);
   }

   // Moves every sound along its parameters and drops the finished ones.
   void update() {
      const ma_uint64 now = ma_engine_get_time_in_pcm_frames(&engine_);
      for (auto it = sounds_.begin(); it != sounds_.end();) {
         Sound& s = *it->second;
         if ((s.stopFrame && now >= *s.stopFrame) || (!s.looping && ma_sound_at_end(&s.sound))) {
            it = sounds_.erase(it);
            continue;
         }
         if (now >= s.startFrame) retune(s, static_cast<double>(now - s.startFrame) / rate_);
         ++it;
      }
   }

private:
   void apply(const Stop& stop) { sounds_.erase(stop.id); }

   void apply(const StopAll&) { sounds_.clear(); }

   void apply(const Patch& patch) {
      if (auto it = sounds_.find(patch.id); it != sounds_.end()) {
         Sound& s = *it->second;
         if (!patch.looping && s.looping) {
            s.looping = false;
            ma_sound_set_looping(&s.sound, MA_FALSE);
         }
         s.volume = patch.volume;
         s.pan = patch.pan;
         s.playbackRate = patch.playbackRate;
         if (patch.filterGain) s.filterGain = patch.filterGain;
         return;
      }
      if (auto sound = create(patch)) sounds_[patch.id] = std::move(sound);
   }

   void retune(Sound& s, double seconds) {
      const float pan = clampPan(s.pan.at(seconds));
      const float gain = static_cast<float>(s.volume.at(seconds));
      s.dry.gain = gain;
      s.dry.pan = pan;
      if (s.hasWet) {
         s.wet.gain = gain;
         s.wet.pan = pan;
         const float filtered = s.filterGain ? static_cast<float>(std::clamp(s.filterGain->at(seconds), 0.0, 1.0)) : 1.0f;
         ma_node_set_output_bus_volume(&s.dry.base, 0, 1.0f - filtered);
         ma_node_set_output_bus_volume(&s.wet.base, 0, filtered);
      }
      ma_sound_set_pitch(&s.sound, static_cast<float>(s.playbackRate.at(seconds)));
   }

   std::unique_ptr<Sound> create(const Patch& patch) {
      auto s = std::make_unique<Sound>();
      s->id = patch.id;
      const Source& source = patch.source;
      ma_data_source* data = nullptr;
      if (source.bytes) {
         s->bytes = source.bytes;
         ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 1, rate_);
         if (!check(ma_decoder_init_memory(s->bytes->data(), s->bytes->size(), &config, &s->decoder), "decoding",
                    patch.id)) {
            log::error("Audio: {} is not a sound file miniaudio reads", source.name);
            return nullptr;
         }
         s->hasDecoder = true;
         data = &s->decoder;
      } else {
         static constexpr ma_waveform_type kTypes[] = {ma_waveform_type_sine, ma_waveform_type_square,
                                                       ma_waveform_type_triangle, ma_waveform_type_sawtooth};
         // Phases where each wave crosses zero, so a tone starts without a click; a square wave
         // never crosses and starts where it is.
         static constexpr double kZeroPhase[] = {0.0, 0.0, 0.25, 0.5};
         const auto wave = static_cast<size_t>(source.wave);
         ma_waveform_config config = ma_waveform_config_init(ma_format_f32, 1, rate_, kTypes[wave], 1.0, source.frequency);
         if (!check(ma_waveform_init(&config, &s->waveform), "the tone", patch.id)) return nullptr;
         s->hasWaveform = true;
         if (kZeroPhase[wave] > 0 && source.frequency > 0)
            ma_waveform_seek_to_pcm_frame(&s->waveform,
                                          static_cast<ma_uint64>(kZeroPhase[wave] * rate_ / source.frequency));
         data = &s->waveform;
      }

      ma_sound_config config = ma_sound_config_init_2(&engine_);
      config.pDataSource = data;
      config.flags = MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT | MA_SOUND_FLAG_NO_SPATIALIZATION;
      config.channelsOut = MA_SOUND_SOURCE_CHANNEL_COUNT; // stays mono until the panner
      if (!check(ma_sound_init_ex(&engine_, &config, &s->sound), "the sound", patch.id)) return nullptr;
      s->hasSound = true;

      ma_node_graph* graph = ma_engine_get_node_graph(&engine_);
      ma_node* endpoint = ma_engine_get_endpoint(&engine_);
      const float gain = static_cast<float>(patch.volume.at(0));
      const float pan = clampPan(patch.pan.at(0));
      if (!check(s->dry.init(graph, gain, pan), "the panner", patch.id)) return nullptr;
      s->hasDry = true;
      if (patch.lpfCutoff) {
         ma_splitter_node_config splitter = ma_splitter_node_config_init(1);
         if (!check(ma_splitter_node_init(graph, &splitter, nullptr, &s->splitter), "the splitter", patch.id))
            return nullptr;
         s->hasSplitter = true;
         ma_lpf_node_config lpf = ma_lpf_node_config_init(1, rate_, *patch.lpfCutoff, 2);
         if (!check(ma_lpf_node_init(graph, &lpf, nullptr, &s->lpf), "the filter", patch.id)) return nullptr;
         s->hasLpf = true;
         if (!check(s->wet.init(graph, gain, pan), "the filtered panner", patch.id)) return nullptr;
         s->hasWet = true;
         if (!check(ma_node_attach_output_bus(&s->sound, 0, &s->splitter, 0), "wiring", patch.id) ||
             !check(ma_node_attach_output_bus(&s->splitter, 0, &s->dry.base, 0), "wiring", patch.id) ||
             !check(ma_node_attach_output_bus(&s->splitter, 1, &s->lpf, 0), "wiring", patch.id) ||
             !check(ma_node_attach_output_bus(&s->lpf, 0, &s->wet.base, 0), "wiring", patch.id) ||
             !check(ma_node_attach_output_bus(&s->wet.base, 0, endpoint, 0), "wiring", patch.id))
            return nullptr;
      } else if (!check(ma_node_attach_output_bus(&s->sound, 0, &s->dry.base, 0), "wiring", patch.id)) {
         return nullptr;
      }
      if (!check(ma_node_attach_output_bus(&s->dry.base, 0, endpoint, 0), "wiring", patch.id)) return nullptr;

      s->volume = patch.volume;
      s->pan = patch.pan;
      s->playbackRate = patch.playbackRate;
      s->filterGain = patch.filterGain;
      s->looping = patch.looping;
      ma_sound_set_looping(&s->sound, patch.looping ? MA_TRUE : MA_FALSE);

      const ma_uint64 now = ma_engine_get_time_in_pcm_frames(&engine_);
      s->startFrame = now + static_cast<ma_uint64>(std::max(0.0, patch.startTime) * rate_);
      if (patch.startTime > 0) ma_sound_set_start_time_in_pcm_frames(&s->sound, s->startFrame);
      if (!source.bytes && !patch.looping && source.duration) {
         s->stopFrame = s->startFrame + static_cast<ma_uint64>(*source.duration * rate_);
         const double fade = std::min(source.fadeOut.value_or(0.0), *source.duration);
         ma_sound_set_stop_time_with_fade_in_pcm_frames(&s->sound, *s->stopFrame, static_cast<ma_uint64>(fade * rate_));
      }
      retune(*s, 0);
      if (!check(ma_sound_start(&s->sound), "starting", patch.id)) return nullptr;
      return s;
   }

   ma_engine engine_{};
   ma_uint32 rate_ = 0;
   std::unordered_map<std::string, std::unique_ptr<Sound>> sounds_;
};

std::mutex g_mutex;
std::condition_variable g_wake;
std::deque<std::vector<Command>> g_queue;
std::atomic<bool> g_enabled = false;

void run() {
   // Never destroyed: the thread and the device live until the process exits.
   auto* engine = new Engine;
   if (!engine->init()) {
      g_enabled = false;
      return;
   }
   for (;;) {
      std::deque<std::vector<Command>> batches;
      {
         std::unique_lock lock(g_mutex);
         // Parameters move every 10 ms; a command wakes the thread at once.
         g_wake.wait_for(lock, std::chrono::milliseconds(10), [] { return !g_queue.empty(); });
         batches.swap(g_queue);
      }
      for (const auto& batch : batches)
         for (const Command& command : batch) engine->run(command);
      engine->update();
   }
}

} // namespace

void start() {
   if (!speech::hasPlayer()) return;
   g_enabled = true;
   std::thread(run).detach();
}

void submit(std::vector<Command> commands) {
   if (!g_enabled || commands.empty()) return;
   {
      std::scoped_lock lock(g_mutex);
      g_queue.push_back(std::move(commands));
   }
   g_wake.notify_one();
}

} // namespace fa::audio
