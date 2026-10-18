#include "hooks.h"

#include "agui.h"
#include "console.h"
#include "cursor.h"
#include "disclosure.h"
#include "editoractions.h"
#include "entityicons.h"
#include "flyingtext.h"
#include "game.h"
#include "highlights.h"
#include "hook-list.h"
#include "input.h"
#include "log.h"
#include "luabridge.h"
#include "movement.h"
#include "popups.h"
#include "railplanner.h"
#include "selection.h"
#include "tutorial.h"
#include "ui.h"
#include "world.h"
#include "zoom.h"

#include <MinHook.h>
#include <windows.h>

namespace fa::hooks {

namespace {

// virtual void agui::Gui::logic(bool); on x64 `this` and the bool arrive as the first two
// arguments of a plain function.
using GuiLogic = void (*)(agui::Gui* gui, bool argument);
GuiLogic g_originalGuiLogic = nullptr;

bool g_installed = false;
// How long uninstall waits for calls already inside a detour.
constexpr DWORD kDrainMs = 250;

void detourGuiLogic(agui::Gui* gui, bool argument) {
   g_originalGuiLogic(gui, argument);
   ui::afterGuiLogic(gui);
}

bool check(MH_STATUS status, const char* what) {
   if (status == MH_OK) return true;
   log::error("{} failed: {}", what, MH_StatusToString(status));
   return false;
}

bool hook(uintptr_t target, void* detour, void** original, const char* name) {
   MH_STATUS status = MH_CreateHook(reinterpret_cast<void*>(target), detour, original);
   if (status == MH_OK) return true;
   log::error("Hooking {} failed: {}", name, MH_StatusToString(status));
   return false;
}

} // namespace

bool install() {
   using game::layout;
   if (!check(MH_Initialize(), "MH_Initialize")) return false;
#define FA_INSTALL_HOOK(field, detour, original, name) hook(layout.field, detour, original, name) &&
   bool ok = FA_HOOKS(FA_INSTALL_HOOK) check(MH_EnableHook(MH_ALL_HOOKS), "Enabling hooks");
#undef FA_INSTALL_HOOK
   if (!ok) {
      MH_Uninitialize();
      return false;
   }
   g_installed = true;
   log::info("Hooks installed");
   return true;
}

void uninstall() {
   if (!g_installed) return;
   // MinHook suspends every other thread while it patches, and moves any thread it finds inside a
   // patched prologue out of it, so after this no new call reaches a detour.
   check(MH_DisableHook(MH_ALL_HOOKS), "Disabling hooks");
   // A call that entered a detour before that still runs our code. Detours never block (they copy
   // what they need and return), so they are all done well within this.
   Sleep(kDrainMs);
   check(MH_Uninitialize(), "MH_Uninitialize");
   g_installed = false;
   log::info("Hooks removed");
}

} // namespace fa::hooks
