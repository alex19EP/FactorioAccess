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

struct Utterance {
   std::string text;
   bool interrupt;
};

std::mutex g_mutex;
std::condition_variable g_wake;
std::deque<Utterance> g_queue;
std::atomic<bool> g_enabled = false;

void PRISM_CALL onPrismLog(void*, PrismLogLevel level, const char* source, const char* message) {
   // Prism's own logging thread; fa::log is thread-safe.
   const char* tag = level == PRISM_LOG_LEVEL_ERROR ? "prism error" : level == PRISM_LOG_LEVEL_WARN ? "prism warn" : "prism";
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
      Utterance next;
      {
         std::unique_lock lock(g_mutex);
         g_wake.wait(lock, [] { return !g_queue.empty(); });
         next = std::move(g_queue.front());
         g_queue.pop_front();
      }
      if (!backend) backend = rebind(context, nullptr);
      if (!backend) continue;
      PrismError error = prism_backend_output(backend, next.text.c_str(), next.interrupt);
      if (error == PRISM_OK) continue;
      log::warn("Speech failed on {}: {}", prism_backend_name(backend), prism_error_string(error));
      backend = rebind(context, backend);
      if (backend) {
         error = prism_backend_output(backend, next.text.c_str(), next.interrupt);
         if (error != PRISM_OK) log::error("Speech failed after rebinding: {}", prism_error_string(error));
      }
   }
}

} // namespace

bool hasPlayer() {
   const wchar_t* commandLine = GetCommandLineW();
   for (const wchar_t* flag : {L"--benchmark", L"--start-server", L"--create"})
      if (std::wcsstr(commandLine, flag)) return false;
   return true;
}

void start() {
   if (!hasPlayer()) {
      log::info("No player at this game, so speech only goes to the log");
      return;
   }
   g_enabled = true;
   // Never joined: the thread lives until the process exits.
   std::thread(run).detach();
}

void say(std::string text, bool interrupt) {
   if (text.empty()) return;
   log::info("say{}: {}", interrupt ? " (interrupt)" : "", text);
   dev::onSpeech(text, interrupt);
   if (!g_enabled) return;
   {
      std::scoped_lock lock(g_mutex);
      if (interrupt) g_queue.clear();
      g_queue.push_back({std::move(text), interrupt});
   }
   g_wake.notify_one();
}

void sayShown(std::string_view text) {
   while (!text.empty()) {
      size_t end = text.find('\n');
      say(text::speakable(text.substr(0, end)), false);
      if (end == std::string_view::npos) break;
      text.remove_prefix(end + 1);
   }
}

} // namespace fa::speech
