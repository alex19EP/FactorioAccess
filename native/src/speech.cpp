#include "speech.h"

#include "devserver.h"
#include "log.h"
#include "text.h"

#include <objbase.h>
#include <prism.h>

#include <atomic>
#include <condition_variable>
#include <cwchar>
#include <deque>
#include <format>
#include <mutex>
#include <thread>

namespace fa::speech {

namespace {

std::mutex g_mutex;
std::condition_variable g_wake;
std::deque<std::string> g_queue;
std::atomic<bool> g_enabled = false;
std::atomic<bool> g_muted = false;
bool g_stopping = false;
std::thread g_thread;

constexpr const wchar_t* kMuteMarker = L"factorio-access-mute.once";

std::string environment(const char* name) {
   char value[64];
   DWORD length = GetEnvironmentVariableA(name, value, sizeof(value));
   return length > 0 && length < sizeof(value) ? std::string(value, length) : std::string();
}

bool mutedAtStart(const std::filesystem::path& directory) {
   if (std::string mute = environment("FA_MUTE"); !mute.empty()) return mute == "1";
   std::error_code error;
   bool marker = std::filesystem::remove(directory / kMuteMarker, error);
   return marker || environment("AI_AGENT").ends_with("_agent");
}

void PRISM_CALL onPrismLog(void*, PrismLogLevel level, const char* source, const char* message) {
   // Prism's own logging thread; fa::log is thread-safe.
   const char* tag = level == PRISM_LOG_LEVEL_ERROR  ? "prism error"
                     : level == PRISM_LOG_LEVEL_WARN ? "prism warn"
                                                     : "prism";
   log::write(tag, std::format("[{}] {}", source ? source : "prism", message ? message : ""));
}

// Drops the current backend, if any, and takes the best one that initializes now. This is how a
// screen reader started or restarted after the game picks up.
PrismBackend* rebind(PrismContext* context, PrismBackend* current) {
   if (current) {
      (void)prism_backend_stop(current);
      prism_backend_free(current);
   }
   PrismBackend* backend = prism_registry_create_best(context);
   if (backend)
      log::info("Speech backend: {}", prism_backend_name(backend));
   else
      log::error("No speech backend available");
   return backend;
}

void run() {
   // SAPI and other COM-based backends need COM on the thread that calls them.
   (void)CoInitializeEx(nullptr, COINIT_MULTITHREADED);
   prism_set_log_handler(PrismLogHandler{&onPrismLog, nullptr});
   prism_set_log_level(PRISM_LOG_LEVEL_INFO);

   PrismConfig config = prism_config_init();
   PrismContext* context = prism_init(&config);
   if (!context) {
      log::error("prism_init failed; there will be no speech");
      return;
   }
   PrismBackend* backend = rebind(context, nullptr);

   for (;;) {
      std::string next;
      {
         std::unique_lock lock(g_mutex);
         g_wake.wait(lock, [] { return !g_queue.empty() || g_stopping; });
         if (g_stopping) break;
         next = std::move(g_queue.front());
         g_queue.pop_front();
      }
      if (!backend) backend = rebind(context, nullptr);
      if (!backend) continue;
      PrismError error = prism_backend_output(backend, next.c_str(), false);
      if (error == PRISM_OK) continue;
      log::warn("Speech failed on {}: {}", prism_backend_name(backend), prism_error_string(error));
      backend = rebind(context, backend);
      if (backend) {
         error = prism_backend_output(backend, next.c_str(), false);
         if (error != PRISM_OK) log::error("Speech failed after rebinding: {}", prism_error_string(error));
      }
   }
   if (backend) {
      (void)prism_backend_stop(backend);
      prism_backend_free(backend);
   }
   prism_shutdown(context);
   // Prism's log thread outlives its contexts; it holds the core loaded until it ends.
   prism_log_shutdown();
   CoUninitialize();
}

} // namespace

bool hasPlayer() {
   const wchar_t* commandLine = GetCommandLineW();
   for (const wchar_t* flag : {L"--benchmark", L"--start-server", L"--create"})
      if (std::wcsstr(commandLine, flag)) return false;
   return true;
}

void start(const std::filesystem::path& directory) {
   if (!hasPlayer()) {
      log::info("No player at this game, so speech only goes to the log");
      return;
   }
   setMuted(mutedAtStart(directory));
   g_enabled = true;
   g_thread = std::thread(run);
}

void stop() {
   if (!g_thread.joinable()) return;
   g_enabled = false;
   {
      std::scoped_lock lock(g_mutex);
      g_stopping = true;
      g_queue.clear();
   }
   g_wake.notify_one();
   g_thread.join();
}

void say(std::string text) {
   if (text.empty()) return;
   log::info("say: {}", text);
   dev::onSpeech(text);
   if (!g_enabled || g_muted) return;
   {
      std::scoped_lock lock(g_mutex);
      g_queue.push_back(std::move(text));
   }
   g_wake.notify_one();
}

void sayShown(std::string_view text) {
   while (!text.empty()) {
      size_t end = text.find('\n');
      say(text::speakable(text.substr(0, end)));
      if (end == std::string_view::npos) break;
      text.remove_prefix(end + 1);
   }
}

bool muted() { return g_muted; }

void setMuted(bool muted) {
   g_muted = muted;
   SetEnvironmentVariableA("FA_MUTE", muted ? "1" : "0");
   if (muted) {
      std::scoped_lock lock(g_mutex);
      g_queue.clear();
   }
   log::info("Speech {}", muted ? "muted: lines go to the log and the dev server only" : "unmuted");
}

} // namespace fa::speech
