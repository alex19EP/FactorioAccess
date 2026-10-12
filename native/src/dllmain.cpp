// FactorioAccess native core: fa_native.dll, loaded by the winmm.dll stub (src/stub/stub.cpp),
// which may unload it and load a rebuilt one while the game runs.

#include "audio.h"
#include "devserver.h"
#include "game.h"
#include "hooks.h"
#include "input.h"
#include "log.h"
#include "luabridge.h"
#include "scanner.h"
#include "speech.h"
#include "stub/stub-api.h"
#include "symbols.h"
#include "ui.h"

#include <windows.h>

#include <chrono>
#include <filesystem>

namespace {

std::filesystem::path modulePath(HMODULE module) {
   wchar_t buffer[MAX_PATH];
   DWORD length = GetModuleFileNameW(module, buffer, MAX_PATH);
   return std::filesystem::path(std::wstring(buffer, length));
}

} // namespace

extern "C" __declspec(dllexport) bool fa_core_start(const fa::stub::Api* api) {
   using namespace fa;
   const std::filesystem::path directory(api->directory);
   // A reload keeps the log of the run so far.
   log::open(directory / "factorio-access-native.log", api->generation > 0);
   if (api->version != stub::kApiVersion) {
      log::error("winmm.dll speaks stub API {}, this core {}; deploy both", api->version, stub::kApiVersion);
      speech::say("FactorioAccess native does not match its winmm.dll", false);
      return false;
   }
   log::info("Loaded into {} (generation {})", modulePath(nullptr).string(), api->generation);
   speech::start(directory);
   audio::start();
   dev::start(directory);

   auto started = std::chrono::steady_clock::now();
   pdb::SymbolTable symbols(GetModuleHandleW(nullptr), modulePath(nullptr),
                            directory / "factorio-access-native.symbols");
   bool resolved = game::resolve(symbols);
   symbols.finish();
   log::info("Symbol resolution took {} ms",
             std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());

   // Fail closed: with any name or layout missing, nothing touches the game.
   if (!resolved) {
      speech::say("FactorioAccess native does not support this Factorio version", false);
      return false;
   }
   ui::start();
   input::setKeyObserver(&scanner::observeKey);
   luabridge::bind(api->bindLua);
   if (!hooks::install()) {
      speech::say("FactorioAccess native could not hook the game", false);
      return false;
   }
   speech::say(api->generation > 0 ? "FactorioAccess native reloaded" : "FactorioAccess native ready", false);
   return true;
}

// Everything the core started ends here, before the stub unloads it: no hook may lead into it and no
// thread of its own may still run.
extern "C" __declspec(dllexport) void fa_core_stop() {
   using namespace fa;
   log::info("Stopping for a reload");
   hooks::uninstall();
   scanner::stop();
   dev::stop();
   audio::stop();
   speech::stop();
   log::info("Stopped");
}
