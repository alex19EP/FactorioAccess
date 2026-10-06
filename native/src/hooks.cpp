#include "hooks.h"

#include "game.h"
#include "gui_reader.h"
#include "log.h"

#include <MinHook.h>

namespace fa::hooks {

namespace {

// virtual void agui::Gui::logic(bool); on x64 `this` and the bool arrive as the first two
// arguments of a plain function.
using GuiLogic = void (*)(agui::Gui* gui, bool argument);
GuiLogic g_originalGuiLogic = nullptr;

void detourGuiLogic(agui::Gui* gui, bool argument) {
   g_originalGuiLogic(gui, argument);
   gui_reader::afterGuiLogic(gui);
}

bool check(MH_STATUS status, const char* what) {
   if (status == MH_OK) return true;
   log::error("{} failed: {}", what, MH_StatusToString(status));
   return false;
}

} // namespace

bool install() {
   if (!check(MH_Initialize(), "MH_Initialize")) return false;
   bool ok = check(MH_CreateHook(reinterpret_cast<void*>(game::layout.guiLogic), reinterpret_cast<void*>(&detourGuiLogic),
                                 reinterpret_cast<void**>(&g_originalGuiLogic)),
                   "Hooking agui::Gui::logic") &&
             check(MH_EnableHook(MH_ALL_HOOKS), "Enabling hooks");
   if (!ok) {
      MH_Uninitialize();
      return false;
   }
   log::info("Hooks installed");
   return true;
}

} // namespace fa::hooks
