#include "hooks.h"

#include "disclosure.h"
#include "flyingtext.h"
#include "game.h"
#include "input.h"
#include "log.h"
#include "luabridge.h"
#include "ui.h"
#include "world.h"

#include <MinHook.h>

namespace fa::hooks {

namespace {

// virtual void agui::Gui::logic(bool); on x64 `this` and the bool arrive as the first two
// arguments of a plain function.
using GuiLogic = void (*)(agui::Gui* gui, bool argument);
GuiLogic g_originalGuiLogic = nullptr;

void detourGuiLogic(agui::Gui* gui, bool argument) {
   g_originalGuiLogic(gui, argument);
   ui::afterGuiLogic(gui);
}

bool check(MH_STATUS status, const char* what) {
   if (status == MH_OK) return true;
   log::error("{} failed: {}", what, MH_StatusToString(status));
   return false;
}

bool hook(uintptr_t target, void* detour, void** original, const char* what) {
   return check(MH_CreateHook(reinterpret_cast<void*>(target), detour, original), what);
}

} // namespace

bool install() {
   using game::layout;
   if (!check(MH_Initialize(), "MH_Initialize")) return false;
   bool ok = hook(layout.guiLogic, reinterpret_cast<void*>(&detourGuiLogic),
                  reinterpret_cast<void**>(&g_originalGuiLogic), "Hooking agui::Gui::logic") &&
             hook(layout.sdlPollEvent, input::pollEventDetour(), input::pollEventOriginal(), "Hooking SDL_PollEvent") &&
             hook(layout.playerCursorPosition, world::playerCursorDetour(), world::playerCursorOriginal(),
                  "Hooking Player::getCursorMapPosition") &&
             hook(layout.sourceCursorPosition, world::sourceCursorDetour(), world::sourceCursorOriginal(),
                  "Hooking PlayerInputSource::getCursorMapPosition") &&
             hook(layout.initLuaState, luabridge::initLuaStateDetour(), luabridge::initLuaStateOriginal(),
                  "Hooking LuaHelper::initLuaState") &&
             hook(layout.versionForDisplay, disclosure::versionDetour(), disclosure::versionOriginal(),
                  "Hooking ApplicationVersion::strDetailedNoBuildMode") &&
             hook(layout.addLocalFlyingText, flyingtext::mapDetour(), flyingtext::mapOriginal(),
                  "Hooking Map::addLocalFlyingText") &&
             hook(layout.constructGuiFlyingText, flyingtext::guiDetour(), flyingtext::guiOriginal(),
                  "Hooking the GuiFlyingText construct") &&
             check(MH_EnableHook(MH_ALL_HOOKS), "Enabling hooks");
   if (!ok) {
      MH_Uninitialize();
      return false;
   }
   log::info("Hooks installed");
   return true;
}

} // namespace fa::hooks
