// winmm.dll: what factorio.exe loads. It forwards every winmm export to the system DLL (the
// generated winmm_forwarders.cpp) and loads FactorioAccess's core, fa_native.dll, beside it. The
// game imports winmm, so this DLL stays until the process exits; the core does not have to. With
// the dev server's marker or FA_DEV=1, the stub loads a copy of the core and loads the new one
// whenever fa_native.dll changes, so a rebuilt core runs without restarting the game.

#include "stub-api.h"

#include <windows.h>

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <format>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

using fa::stub::LuaFunction;

constexpr const wchar_t* kCore = L"fa_native.dll";
constexpr const wchar_t* kLog = L"factorio-access-native.log";
constexpr const wchar_t* kDevMarker = L"factorio-access-dev.enable";

HMODULE g_module = nullptr;
std::filesystem::path g_directory;
std::wstring g_directoryText;

// One line in the core's log, which the core opens and the stub only appends to.
void note(std::string_view message) {
   HANDLE file = CreateFileW((g_directory / kLog).c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
   if (file == INVALID_HANDLE_VALUE) return;
   SYSTEMTIME now;
   GetLocalTime(&now);
   const std::string line =
      std::format("{:02}:{:02}:{:02}.{:03} [stub] {}\n", now.wHour, now.wMinute, now.wSecond, now.wMilliseconds, message);
   DWORD written = 0;
   WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
   CloseHandle(file);
}

// ---- the functions Lua holds ----

// Lua keeps the functions it was given for as long as its state lives, across reloads, so it gets
// these and never the core's own. Each slot is one fa_native function, found by name, so a reload
// that reorders or adds functions still calls the right ones.
constexpr size_t kSlots = 64;

struct Slot {
   std::string name;
   LuaFunction target = nullptr;
};

std::mutex g_mutex;
std::condition_variable g_changed;
std::array<Slot, kSlots> g_slots;
size_t g_slotCount = 0;
// While set, calls wait for the next core instead of reaching the one being unloaded.
bool g_reloading = false;
// Calls inside the core right now; the core is unloaded only once none is.
int g_active = 0;

// How long a Lua call waits for a reload before it gives up and returns nothing.
constexpr auto kReloadWait = std::chrono::seconds(15);

int call(size_t slot, lua_State* L) {
   LuaFunction target;
   {
      std::unique_lock lock(g_mutex);
      if (!g_changed.wait_for(lock, kReloadWait, [] { return !g_reloading; })) return 0;
      target = g_slots[slot].target;
      if (!target) return 0;
      ++g_active;
   }
   // Lua errors are C++ exceptions in Factorio (luaD_throw), so this runs when one leaves the core.
   struct Leave {
      ~Leave() {
         {
            std::scoped_lock lock(g_mutex);
            --g_active;
         }
         g_changed.notify_all();
      }
   } leave;
   return target(L);
}

template <size_t Slot>
int trampoline(lua_State* L) {
   return call(Slot, L);
}

template <size_t... Index>
constexpr std::array<LuaFunction, kSlots> makeTrampolines(std::index_sequence<Index...>) {
   return {&trampoline<Index>...};
}

constexpr std::array<LuaFunction, kSlots> kTrampolines = makeTrampolines(std::make_index_sequence<kSlots>{});

LuaFunction bindLua(const char* name, LuaFunction target) {
   std::scoped_lock lock(g_mutex);
   size_t slot = 0;
   while (slot < g_slotCount && g_slots[slot].name != name) ++slot;
   if (slot == g_slotCount) {
      if (g_slotCount == kSlots) {
         note(std::format("No Lua slot left for fa_native.{}; raise kSlots", name));
         return nullptr;
      }
      g_slots[g_slotCount++].name = name;
   }
   g_slots[slot].target = target;
   return kTrampolines[slot];
}

// ---- the core ----

struct Core {
   HMODULE module = nullptr;
   FaCoreStop stop = nullptr;
   // The copy that was loaded, removed once it is unloaded; empty when the core itself was.
   std::filesystem::path copy;
};

Core g_core;
fa::stub::Api g_api{};
bool g_dev = false;

bool devMode() {
   char value[8];
   const DWORD length = GetEnvironmentVariableA("FA_DEV", value, sizeof(value));
   if (length == 1 && value[0] == '1') return true;
   return GetFileAttributesW((g_directory / kDevMarker).c_str()) != INVALID_FILE_ATTRIBUTES;
}

// When and how big fa_native.dll is, to see that a build replaced it.
struct Stamp {
   FILETIME written;
   uint64_t size;
   bool operator==(const Stamp& other) const {
      return CompareFileTime(&written, &other.written) == 0 && size == other.size;
   }
};

std::optional<Stamp> stampOf(const std::filesystem::path& file) {
   WIN32_FILE_ATTRIBUTE_DATA data;
   if (!GetFileAttributesExW(file.c_str(), GetFileExInfoStandard, &data)) return std::nullopt;
   return Stamp{data.ftLastWriteTime, (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow};
}

std::optional<Stamp> g_loadedStamp;

// The copies earlier runs left behind, which a crash never got to remove.
void removeOldCopies() {
   WIN32_FIND_DATAW found;
   HANDLE search = FindFirstFileW((g_directory / L"fa_native.live-*.dll").c_str(), &found);
   if (search == INVALID_HANDLE_VALUE) return;
   do DeleteFileW((g_directory / found.cFileName).c_str());
   while (FindNextFileW(search, &found));
   FindClose(search);
}

void load(uint32_t generation) {
   const std::filesystem::path core = g_directory / kCore;
   std::filesystem::path file = core;
   if (g_dev) {
      // The build replaces fa_native.dll while the copy runs.
      g_loadedStamp = stampOf(core);
      file = g_directory / std::format(L"fa_native.live-{}.dll", generation);
      if (!CopyFileW(core.c_str(), file.c_str(), FALSE)) {
         note(std::format("Copying fa_native.dll failed ({})", GetLastError()));
         return;
      }
   }
   HMODULE module = LoadLibraryW(file.c_str());
   if (!module) {
      note(std::format("Loading {} failed ({})", file.filename().string(), GetLastError()));
      if (g_dev) DeleteFileW(file.c_str());
      return;
   }
   auto start = reinterpret_cast<FaCoreStart>(GetProcAddress(module, "fa_core_start"));
   auto stop = reinterpret_cast<FaCoreStop>(GetProcAddress(module, "fa_core_stop"));
   if (!start || !stop) {
      note("fa_native.dll lacks fa_core_start or fa_core_stop");
      FreeLibrary(module);
      if (g_dev) DeleteFileW(file.c_str());
      return;
   }
   g_core = {module, stop, g_dev ? file : std::filesystem::path()};
   g_api = {fa::stub::kApiVersion, g_directoryText.c_str(), generation, &bindLua};
   if (!start(&g_api)) note("The core stood down");
}

void unload() {
   {
      std::unique_lock lock(g_mutex);
      g_reloading = true;
      g_changed.wait(lock, [] { return g_active == 0; });
   }
   g_core.stop();
   {
      std::scoped_lock lock(g_mutex);
      for (Slot& slot : g_slots) slot.target = nullptr;
   }
   FreeLibrary(g_core.module);
   if (!g_core.copy.empty()) {
      // A thread made with the CRT holds its module until it ends, so one the core left running
      // keeps it loaded (and running) after this.
      if (GetModuleHandleW(g_core.copy.c_str()))
         note(std::format("{} is still loaded: a thread of it still runs", g_core.copy.filename().string()));
      else
         DeleteFileW(g_core.copy.c_str());
   }
   g_core = {};
}

void reopenLua() {
   {
      std::scoped_lock lock(g_mutex);
      g_reloading = false;
   }
   g_changed.notify_all();
}

// How often the dev loop looks at fa_native.dll. A change is loaded once the file has stayed the
// same for one more look, so a copy still being written is never loaded.
constexpr DWORD kWatchMs = 500;

DWORD WINAPI run(void*) {
   wchar_t buffer[MAX_PATH];
   const DWORD length = GetModuleFileNameW(g_module, buffer, MAX_PATH);
   g_directory = std::filesystem::path(std::wstring(buffer, length)).parent_path();
   g_directoryText = g_directory.wstring();
   g_dev = devMode();
   if (g_dev) removeOldCopies();
   load(0);
   if (!g_dev) return 0;

   const std::filesystem::path core = g_directory / kCore;
   std::optional<Stamp> pending;
   for (uint32_t generation = 1;;) {
      Sleep(kWatchMs);
      const std::optional<Stamp> now = stampOf(core);
      if (!now || now == g_loadedStamp) {
         pending.reset();
         continue;
      }
      if (now != pending) {
         pending = now;
         continue;
      }
      pending.reset();
      note(std::format("fa_native.dll changed; reloading as generation {}", generation));
      const auto started = std::chrono::steady_clock::now();
      if (g_core.module) unload();
      load(generation++);
      reopenLua();
      note(std::format("Reload took {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(
                                                std::chrono::steady_clock::now() - started)
                                                .count()));
   }
}

// Runs under the loader lock, so it sticks to plain Win32 calls.
bool hostIsFactorio() {
   wchar_t buffer[MAX_PATH];
   DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
   if (length == 0 || length == MAX_PATH) return false;
   const wchar_t* name = std::wcsrchr(buffer, L'\\');
   return _wcsicmp(name ? name + 1 : buffer, L"factorio.exe") == 0;
}

// The game uploads a crash log by starting factorio.exe --upload-log-file, which loads this DLL
// too. Ends that process before it sends anything (src/disclosure.h says why). Plain Win32, so
// safe from DllMain; it covers crashes before any core is loaded.
void blockLogUploader() {
   // The crashed game starts the uploader with ShellExecute and never reads its exit code, so this
   // one only tells whoever looks that the block worked.
   constexpr UINT kBlockedExitCode = 0xFA;
   if (std::wcsstr(GetCommandLineW(), L"--upload-log-file")) TerminateProcess(GetCurrentProcess(), kBlockedExitCode);
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
   if (reason != DLL_PROCESS_ATTACH) return TRUE;
   DisableThreadLibraryCalls(module);
   // Any other process that picks up this winmm.dll only gets the forwarded exports.
   if (!hostIsFactorio()) return TRUE;
   blockLogUploader();
   g_module = module;
   // The thread starts once the loader lock is released, while the game carries on starting up.
   if (HANDLE thread = CreateThread(nullptr, 0, &run, nullptr, 0, nullptr)) CloseHandle(thread);
   return TRUE;
}
