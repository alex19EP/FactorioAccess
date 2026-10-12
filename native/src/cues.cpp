#include "cues.h"

#include "audio.h"
#include "log.h"
#include "modfiles.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace fa::cues {

namespace {

// A sound of data.lua: its file, its volume there, and the file's bytes once read.
struct Sound {
   const char* path;
   const char* name;
   double volume;
   std::shared_ptr<const std::string> bytes;
};

enum SoundIndex : size_t { kNextMachine, kAtRuler, kOnLine, kClose, kSoundCount };

// The "next-machine" sound and the "audio-ruler-*" sounds of data.lua.
Sound g_sounds[kSoundCount] = {
   {"__FactorioAccess__/audio/sonifiers/grid/crafting_machine/new_machine.ogg", "new_machine.ogg", 1.0, nullptr},
   {"__base__/sound/programmable-speaker/kit-07.ogg", "kit-07.ogg", 1.0, nullptr},
   {"__base__/sound/programmable-speaker/plucked-14.ogg", "plucked-14.ogg", 0.5, nullptr},
   {"__base__/sound/programmable-speaker/plucked-12.ogg", "plucked-12.ogg", 0.5, nullptr},
};

std::atomic<bool> g_nextMachine{false};
std::mutex g_mutex;
bool g_read = false;
// Each sound its own id, so a quick second step starts it again rather than retuning the first.
std::atomic<uint64_t> g_played{0};

void play(SoundIndex index) {
   const char* name;
   double volume;
   std::shared_ptr<const std::string> bytes;
   {
      std::scoped_lock lock(g_mutex);
      name = g_sounds[index].name;
      volume = g_sounds[index].volume;
      bytes = g_sounds[index].bytes;
   }
   if (!bytes) return;
   audio::Patch patch;
   patch.id = std::string("fa-cue-") + name + "-" + std::to_string(++g_played);
   patch.source.name = name;
   patch.source.bytes = std::move(bytes);
   patch.volume = audio::Parameter{volume};
   audio::submit({std::move(patch)});
}

} // namespace

void setNextMachine(bool enabled) {
   g_nextMachine = enabled;
   std::scoped_lock lock(g_mutex);
   if (std::exchange(g_read, true)) return;
   for (Sound& sound : g_sounds) {
      if (auto bytes = modfiles::read(sound.path)) {
         log::info("Cues: read {}, {} bytes", sound.path, bytes->size());
         sound.bytes = std::make_shared<const std::string>(std::move(*bytes));
      } else
         log::error("Cues: cannot read {}", sound.path);
   }
}

void nextMachine() {
   if (!g_nextMachine) return;
   log::info("Cues: next machine");
   play(kNextMachine);
}

void rulerTone(skip::Alignment alignment) {
   switch (alignment) {
   case skip::Alignment::AtRuler: play(kAtRuler); break;
   case skip::Alignment::OnLine: play(kOnLine); break;
   case skip::Alignment::Close: play(kClose); break;
   case skip::Alignment::None: break;
   }
}

} // namespace fa::cues
