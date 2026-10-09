#include "devserver.h"

#include "agui.h"
#include "input.h"
#include "log.h"
#include "navigator/ScreenManager.hpp"
#include "screens/GuiDump.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace fa::dev {

namespace {

constexpr const wchar_t* kMarker = L"factorio-access-dev.enable";
// RTAccess has 8772 and WrathAccess 8771.
constexpr uint16_t kDefaultPort = 8773;
constexpr size_t kSpeechKept = 2000;
constexpr DWORD kGameWaitMs = 3000;

std::atomic<bool> g_running{false};
std::atomic<uint64_t> g_frames{0};

// ---- speech ring ----

struct Line {
   uint64_t seq;
   bool interrupt;
   std::string text;
};

std::mutex g_speechMutex;
std::deque<Line> g_speech;
uint64_t g_nextSeq = 0;

uint64_t speechCursor() {
   std::scoped_lock lock(g_speechMutex);
   return g_nextSeq;
}

std::vector<Line> speechSince(uint64_t since) {
   std::scoped_lock lock(g_speechMutex);
   std::vector<Line> lines;
   for (const Line& line : g_speech)
      if (line.seq >= since) lines.push_back(line);
   return lines;
}

// ---- requests run on the game thread ----

struct Job {
   std::function<std::string()> work;
   std::string result;
   bool done = false;
};

std::mutex g_jobMutex;
std::condition_variable g_jobDone;
std::deque<std::shared_ptr<Job>> g_jobs;

std::string onGameThread(std::function<std::string()> work) {
   auto job = std::make_shared<Job>();
   job->work = std::move(work);
   std::unique_lock lock(g_jobMutex);
   g_jobs.push_back(job);
   // The navigator off after a fault, or a game that is not ticking, never runs it.
   if (!g_jobDone.wait_for(lock, std::chrono::milliseconds(kGameWaitMs), [&] { return job->done; })) {
      std::erase(g_jobs, job);
      return "[timeout] the game thread did not run the request\n";
   }
   return job->result;
}

void waitFrames(uint64_t count) {
   uint64_t target = g_frames + count;
   ULONGLONG deadline = GetTickCount64() + std::max<ULONGLONG>(kGameWaitMs, count * 50);
   while (g_frames < target && GetTickCount64() < deadline) Sleep(5);
}

// ---- keys ----

struct Chord {
   uint32_t key;
   bool shift, ctrl, alt;
};

std::optional<uint32_t> keyNamed(std::string_view name) {
   using namespace input::keys;
   if (name.size() == 1 && ((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= '0' && name[0] <= '9')))
      return static_cast<uint32_t>(name[0]);
   static const std::pair<std::string_view, uint32_t> named[] = {
      {"tab", Tab},         {"enter", Return},  {"return", Return},     {"escape", Escape}, {"esc", Escape},
      {"space", Space},     {"backspace", Backspace}, {"delete", Delete}, {"home", Home}, {"end", End},
      {"pageup", PageUp},   {"pagedown", PageDown}, {"up", Up},          {"down", Down},     {"left", Left},
      {"right", Right},     {"f1", F1},         {"[", LeftBracket},     {"]", RightBracket},
      {"\\", Backslash},    {"backslash", Backslash}, {"=", Equals}, {"-", Minus},
      // The game's console keys.
      {"/", Slash},         {"slash", Slash},   {"`", Grave},           {"grave", Grave},
      {"lalt", LeftAlt}, // alone, as the game's alt mode key
   };
   for (const auto& [n, key] : named)
      if (n == name) return key;
   return std::nullopt;
}

std::optional<Chord> parseChord(std::string_view token) {
   Chord chord{0, false, false, false};
   for (;;) {
      size_t plus = token.find('+');
      // A lone "+" or a trailing one is the key itself, not a separator.
      if (plus == std::string_view::npos || plus + 1 == token.size()) break;
      std::string_view mod = token.substr(0, plus);
      if (mod == "shift") chord.shift = true;
      else if (mod == "ctrl") chord.ctrl = true;
      else if (mod == "alt") chord.alt = true;
      else return std::nullopt;
      token.remove_prefix(plus + 1);
   }
   auto key = keyNamed(token);
   if (!key) return std::nullopt;
   chord.key = *key;
   return chord;
}

std::vector<std::string_view> words(std::string_view text) {
   std::vector<std::string_view> out;
   size_t i = 0;
   while (i < text.size()) {
      while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
      size_t start = i;
      while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i]))) ++i;
      if (i > start) out.push_back(text.substr(start, i - start));
   }
   return out;
}

std::string lowered(std::string_view text) {
   std::string out(text);
   for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
   return out;
}

// Frames between two chords: enough for the game to act on one before the next, and for a
// window it opened to settle into a screen.
constexpr uint64_t kFramesBetweenKeys = 6;
// Frames after the last chord before reading what was said.
constexpr uint64_t kFramesAfterKeys = 20;
// Frames a chord's modifiers are held before and after its key.
constexpr uint64_t kFramesForModifiers = 3;
// Frames a key is held, long enough for the game's checks that it is held to see it (a quick
// press lasts about six).
constexpr uint64_t kFramesHeld = 4;

std::string runKeys(std::string_view spec) {
   struct Step {
      std::optional<Chord> chord;
      uint64_t wait = 0;
      std::string text;
   };
   std::vector<Step> steps;
   for (std::string_view word : words(spec)) {
      // "text=iron" types "iron" into the focused field, case kept; "_" stands for a space.
      if (word.starts_with("text=")) {
         std::string text(word.substr(5));
         std::ranges::replace(text, '_', ' ');
         if (text.empty()) return "[error] no text\n";
         steps.push_back({std::nullopt, 0, std::move(text)});
         continue;
      }
      std::string token = lowered(word);
      if (token == "wait") {
         steps.push_back({std::nullopt, 60, {}});
         continue;
      }
      if (token.starts_with("wait=")) {
         uint64_t frames = 0;
         auto [end, error] = std::from_chars(token.data() + 5, token.data() + token.size(), frames);
         if (error != std::errc() || end != token.data() + token.size())
            return std::format("[error] bad wait: {}\n", word);
         steps.push_back({std::nullopt, frames, {}});
         continue;
      }
      auto chord = parseChord(token);
      if (!chord) return std::format("[error] unknown key: {}\n", word);
      steps.push_back({chord, 0, {}});
   }
   if (steps.empty()) return "[error] no keys\n";

   uint64_t from = speechCursor();
   bool first = true;
   for (const Step& step : steps) {
      if (!step.text.empty()) {
         if (!first) waitFrames(kFramesBetweenKeys);
         first = false;
         input::injectText(step.text);
         continue;
      }
      if (!step.chord) {
         waitFrames(step.wait);
         continue;
      }
      if (!first) waitFrames(kFramesBetweenKeys);
      first = false;
      const Chord& chord = *step.chord;
      // Modifiers go down frames before the key and up after the game has acted on it, as a
      // person's do: the game reads them from its key state when it handles the key.
      bool modified = chord.shift || chord.ctrl || chord.alt;
      if (modified) {
         input::injectModifiers(chord.shift, chord.ctrl, chord.alt, true);
         waitFrames(kFramesForModifiers);
      }
      input::injectKey(chord.key, chord.shift, chord.ctrl, chord.alt, true);
      waitFrames(kFramesHeld);
      input::injectKey(chord.key, chord.shift, chord.ctrl, chord.alt, false);
      if (modified) {
         waitFrames(kFramesForModifiers);
         input::injectModifiers(chord.shift, chord.ctrl, chord.alt, false);
      }
   }
   waitFrames(kFramesAfterKeys);

   std::string out;
   for (const Line& line : speechSince(from)) out += std::format("{}{}\n", line.interrupt ? "! " : "  ", line.text);
   return out.empty() ? "(silence)\n" : out;
}

// Every visible window of the application Gui, the game's own over a loaded game included, which
// no screen reads and so none dumps.
std::string dumpWindows() {
   const agui::Gui* gui = agui::applicationGui();
   const agui::Widget* root = gui ? agui::baseWidget(gui) : nullptr;
   if (!root) return "(no gui)\n";
   std::string out;
   for (const agui::Widget* child : agui::children(root))
      if (agui::visible(child)) out += std::format("==== {} ====\n{}\n", agui::className(child), screens::DescribeTree(child));
   return out.empty() ? "(no visible window)\n" : out;
}

// ---- HTTP ----

struct Request {
   std::string method;
   std::string path;
   std::string query;
   std::string body;
};

std::string percentDecoded(std::string_view text) {
   std::string out;
   for (size_t i = 0; i < text.size(); ++i) {
      if (text[i] == '%' && i + 2 < text.size()) {
         unsigned value = 0;
         auto [end, error] = std::from_chars(text.data() + i + 1, text.data() + i + 3, value, 16);
         if (error == std::errc() && end == text.data() + i + 3) {
            out += static_cast<char>(value);
            i += 2;
            continue;
         }
      }
      out += text[i];
   }
   return out;
}

std::string queryParam(std::string_view query, std::string_view name) {
   while (!query.empty()) {
      size_t amp = query.find('&');
      std::string_view pair = query.substr(0, amp);
      size_t eq = pair.find('=');
      if (pair.substr(0, eq) == name) return eq == std::string_view::npos ? "" : percentDecoded(pair.substr(eq + 1));
      if (amp == std::string_view::npos) break;
      query.remove_prefix(amp + 1);
   }
   return {};
}

std::pair<int, std::string> route(const Request& request) {
   if (request.path == "/health") return {200, std::format("ok\nframes={}\n", g_frames.load())};

   if (request.path == "/speech") {
      uint64_t since = 0;
      std::string param = queryParam(request.query, "since");
      std::from_chars(param.data(), param.data() + param.size(), since);
      std::string out = std::format("next={}\n", speechCursor());
      for (const Line& line : speechSince(since))
         out += std::format("{} {}{}\n", line.seq, line.interrupt ? "! " : "  ", line.text);
      return {200, out};
   }

   if (request.path == "/screen")
      return {200, onGameThread([] { return nav::ScreenManager::Get().Describe(); })};

   if (request.path == "/key") {
      std::string spec = request.method == "POST" ? request.body : queryParam(request.query, "keys");
      return {200, runKeys(spec)};
   }

   if (request.path == "/dump") return {200, onGameThread(dumpWindows)};

   return {404, "endpoints: /health /speech?since=N /screen /key /dump\n"};
}

bool readRequest(SOCKET client, Request& request) {
   std::string data;
   char buffer[4096];
   size_t headerEnd;
   while ((headerEnd = data.find("\r\n\r\n")) == std::string::npos) {
      int got = recv(client, buffer, sizeof(buffer), 0);
      if (got <= 0 || data.size() > 65536) return false;
      data.append(buffer, got);
   }
   std::string_view head(data.data(), headerEnd);
   size_t lineEnd = head.find("\r\n");
   std::string_view requestLine = head.substr(0, lineEnd);
   auto parts = words(requestLine);
   if (parts.size() < 2) return false;
   request.method = std::string(parts[0]);
   std::string_view target = parts[1];
   size_t question = target.find('?');
   request.path = std::string(target.substr(0, question));
   if (question != std::string_view::npos) request.query = std::string(target.substr(question + 1));

   size_t length = 0;
   std::string lowerHead = lowered(head);
   if (size_t at = lowerHead.find("\r\ncontent-length:"); at != std::string::npos) {
      const char* p = lowerHead.data() + at + 17;
      while (*p == ' ') ++p;
      std::from_chars(p, lowerHead.data() + lowerHead.size(), length);
   }
   if (length > 1 << 20) return false;
   request.body = data.substr(headerEnd + 4);
   while (request.body.size() < length) {
      int got = recv(client, buffer, sizeof(buffer), 0);
      if (got <= 0) return false;
      request.body.append(buffer, got);
   }
   return true;
}

void sendAll(SOCKET client, std::string_view data) {
   while (!data.empty()) {
      int sent = send(client, data.data(), static_cast<int>(std::min<size_t>(data.size(), 1 << 16)), 0);
      if (sent <= 0) return;
      data.remove_prefix(sent);
   }
}

void handle(SOCKET client) {
   DWORD timeout = 5000;
   setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
   Request request;
   if (readRequest(client, request)) {
      auto [status, body] = route(request);
      std::string head = std::format("HTTP/1.1 {} {}\r\nContent-Type: text/plain; charset=utf-8\r\n"
                                     "Content-Length: {}\r\nConnection: close\r\n\r\n",
                                     status, status == 200 ? "OK" : "Not Found", body.size());
      sendAll(client, head);
      sendAll(client, body);
   }
   // Half-close and drain before closing: a loopback close with unread data turns into a reset,
   // and the client can lose the reply with it.
   shutdown(client, SD_SEND);
   timeout = 1000;
   setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
   char sink[512];
   while (recv(client, sink, sizeof(sink), 0) > 0) {}
   closesocket(client);
}

SOCKET listenOn(int family, uint16_t port) {
   SOCKET s = socket(family, SOCK_STREAM, IPPROTO_TCP);
   if (s == INVALID_SOCKET) return s;
   BOOL exclusive = TRUE;
   setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
   sockaddr_storage address{};
   int size;
   if (family == AF_INET) {
      auto* v4 = reinterpret_cast<sockaddr_in*>(&address);
      v4->sin_family = AF_INET;
      v4->sin_port = htons(port);
      v4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      size = sizeof(sockaddr_in);
   } else {
      auto* v6 = reinterpret_cast<sockaddr_in6*>(&address);
      v6->sin6_family = AF_INET6;
      v6->sin6_port = htons(port);
      v6->sin6_addr = in6addr_loopback;
      size = sizeof(sockaddr_in6);
   }
   if (bind(s, reinterpret_cast<sockaddr*>(&address), size) == SOCKET_ERROR || listen(s, 8) == SOCKET_ERROR) {
      closesocket(s);
      return INVALID_SOCKET;
   }
   return s;
}

void serve(std::vector<SOCKET> listeners) {
   for (;;) {
      fd_set ready;
      FD_ZERO(&ready);
      for (SOCKET s : listeners) FD_SET(s, &ready);
      if (select(0, &ready, nullptr, nullptr, nullptr) == SOCKET_ERROR) {
         log::error("Dev server stopped: select failed ({})", WSAGetLastError());
         return;
      }
      for (SOCKET s : listeners)
         if (FD_ISSET(s, &ready))
            if (SOCKET client = accept(s, nullptr, nullptr); client != INVALID_SOCKET) handle(client);
   }
}

std::string environment(const char* name) {
   char value[64];
   DWORD length = GetEnvironmentVariableA(name, value, sizeof(value));
   return length > 0 && length < sizeof(value) ? std::string(value, length) : std::string();
}

bool enabled(const std::filesystem::path& directory) {
   if (environment("FA_DEV") == "1") return true;
   std::error_code error;
   return std::filesystem::exists(directory / kMarker, error);
}

} // namespace

void start(const std::filesystem::path& directory) {
   if (!enabled(directory)) return;
   uint16_t port = kDefaultPort;
   if (std::string env = environment("FA_DEV_PORT"); !env.empty()) std::from_chars(env.data(), env.data() + env.size(), port);

   WSADATA wsa;
   if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
      log::error("Dev server: WSAStartup failed");
      return;
   }
   // Both loopbacks as two sockets, never a dual-mode wildcard that would listen on every
   // interface. `localhost` resolves to ::1 first.
   std::vector<SOCKET> listeners;
   for (int family : {AF_INET6, AF_INET})
      if (SOCKET s = listenOn(family, port); s != INVALID_SOCKET) listeners.push_back(s);
   if (listeners.empty()) {
      log::error("Dev server: could not listen on loopback port {} ({})", port, WSAGetLastError());
      return;
   }
   g_running = true;
   std::thread(serve, std::move(listeners)).detach();
   log::info("Dev server on http://localhost:{} (/health /speech /screen /key /dump)", port);
}

void pump() {
   ++g_frames;
   if (!g_running) return;
   std::deque<std::shared_ptr<Job>> jobs;
   {
      std::scoped_lock lock(g_jobMutex);
      jobs.swap(g_jobs);
   }
   if (jobs.empty()) return;
   for (auto& job : jobs) {
      std::string result;
      try {
         result = job->work();
      } catch (const std::exception& e) {
         result = std::format("[exception] {}\n", e.what());
      }
      std::scoped_lock lock(g_jobMutex);
      job->result = std::move(result);
      job->done = true;
   }
   g_jobDone.notify_all();
}

void onSpeech(std::string_view text, bool interrupt) {
   if (!g_running) return;
   std::scoped_lock lock(g_speechMutex);
   g_speech.push_back({g_nextSeq++, interrupt, std::string(text)});
   if (g_speech.size() > kSpeechKept) g_speech.pop_front();
}

} // namespace fa::dev
