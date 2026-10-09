// FactorioAccess native: loaded into factorio.exe as a winmm.dll proxy (see CMakeLists.txt).

#include "audio.h"
#include "devserver.h"
#include "disclosure.h"
#include "game.h"
#include "hooks.h"
#include "log.h"
#include "speech.h"
#include "symbols.h"
#include "ui.h"

#include <windows.h>

#include <chrono>
#include <cwchar>
#include <filesystem>

namespace {

HMODULE g_module = nullptr;

std::filesystem::path modulePath(HMODULE module) {
   wchar_t buffer[MAX_PATH];
   DWORD length = GetModuleFileNameW(module, buffer, MAX_PATH);
   return std::filesystem::path(std::wstring(buffer, length));
}

// Runs under the loader lock, so it sticks to plain Win32 calls.
bool hostIsFactorio() {
   wchar_t buffer[MAX_PATH];
   DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
   if (length == 0 || length == MAX_PATH) return false;
   const wchar_t* name = std::wcsrchr(buffer, L'\\');
   return _wcsicmp(name ? name + 1 : buffer, L"factorio.exe") == 0;
}

DWORD WINAPI initialize(void*) {
   using namespace fa;
   auto directory = modulePath(g_module).parent_path();
   log::open(directory / "factorio-access-native.log");
   log::info("Loaded into {}", modulePath(nullptr).string());
   speech::start();
   audio::start();
   dev::start(directory);

   auto started = std::chrono::steady_clock::now();
   pdb::SymbolTable symbols(GetModuleHandleW(nullptr), modulePath(nullptr), directory / "factorio-access-native.symbols");
   bool resolved = game::resolve(symbols);
   symbols.finish();
   log::info("Symbol resolution took {} ms",
             std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());

   // Fail closed: with any name or layout missing, nothing touches the game.
   if (!resolved) {
      speech::say("FactorioAccess native does not support this Factorio version", false);
      return 0;
   }
   ui::start();
   if (!hooks::install()) {
      speech::say("FactorioAccess native could not hook the game", false);
      return 0;
   }
   speech::say("FactorioAccess native ready", false);
   return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
   if (reason != DLL_PROCESS_ATTACH) return TRUE;
   DisableThreadLibraryCalls(module);
   g_module = module;
   // Any other process that picks up this winmm.dll only gets the forwarded exports.
   if (!hostIsFactorio()) return TRUE;
   fa::disclosure::blockLogUploader();
   // The thread starts once the loader lock is released, while the game carries on starting up.
   if (HANDLE thread = CreateThread(nullptr, 0, &initialize, nullptr, 0, nullptr)) CloseHandle(thread);
   return TRUE;
}
