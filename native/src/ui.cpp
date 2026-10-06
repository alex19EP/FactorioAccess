#include "ui.h"

#include "agui.h"
#include "devserver.h"
#include "input.h"
#include "log.h"
#include "navigator/ScreenManager.hpp"
#include "screens/CharacterScreen.hpp"
#include "screens/ControlSettingsScreen.hpp"
#include "screens/DropDownScreen.hpp"
#include "screens/GenericWindowScreen.hpp"
#include "screens/LoadGameScreen.hpp"
#include "screens/MachineScreen.hpp"
#include "screens/MapGeneratorScreen.hpp"
#include "screens/MenuScreen.hpp"
#include "screens/ModSettingsScreen.hpp"
#include "screens/ModsScreen.hpp"
#include "screens/NewGameScreen.hpp"
#include "screens/ScenarioMessageScreen.hpp"
#include "screens/SettingsScreen.hpp"
#include "speech.h"

#include <windows.h>

#include <memory>
#include <vector>

namespace fa::ui {

namespace {

bool g_disabled = false;

void tick(const agui::Gui* gui) {
   // Only the application Gui carries screens; the main menu's background simulation runs a Gui
   // of its own every frame, which is recreated at will.
   if (gui != agui::applicationGui()) return;
   screens::WindowScreen::SetGui(gui);
   nav::ScreenManager::Get().Update();
   dev::pump();
}

// A wrong offset or a widget freed under us shows up as an access violation. Stop rather than
// take the game down; nothing here may unwind C++ objects, so the work lives in tick().
bool guardedTick(const agui::Gui* gui) {
   __try {
      tick(gui);
      return true;
   } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER
                                                                 : EXCEPTION_CONTINUE_SEARCH) {
      return false;
   }
}

} // namespace

void start() {
   auto& manager = nav::ScreenManager::Get();
   std::vector<std::unique_ptr<screens::WindowScreen>> recipes;
   recipes.push_back(std::make_unique<screens::MenuScreen>());
   recipes.push_back(std::make_unique<screens::LoadGameScreen>());
   recipes.push_back(std::make_unique<screens::NewGameScreen>());
   recipes.push_back(std::make_unique<screens::MapGeneratorScreen>());
   recipes.push_back(std::make_unique<screens::ModsScreen>());
   recipes.push_back(std::make_unique<screens::ControlSettingsScreen>());
   recipes.push_back(std::make_unique<screens::ModSettingsScreen>());
   recipes.push_back(std::make_unique<screens::SettingsScreen>());

   std::vector<const screens::WindowScreen*> claimed;
   for (auto& recipe : recipes) {
      claimed.push_back(recipe.get());
      manager.Register(std::move(recipe));
   }
   // Whatever window no recipe reads.
   manager.Register(std::make_unique<screens::GenericWindowScreen>(std::move(claimed)));
   // Under them, the scenario's message dialog over a loaded game.
   manager.Register(std::make_unique<screens::ScenarioMessageScreen>());
   // The game's character screen (E) over a loaded game; the game menu takes over while it is open.
   manager.Register(std::make_unique<screens::CharacterScreen>());
   // An entity's window (a chest, a furnace, a drill, ...), opened by the game's own open-gui control.
   manager.Register(std::make_unique<screens::MachineScreen>());
   // Over any of them, an open dropdown's list.
   manager.Register(std::make_unique<screens::DropDownScreen>());
}

void afterGuiLogic(const agui::Gui* gui) {
   if (g_disabled) return;
   if (!guardedTick(gui)) {
      g_disabled = true;
      // Keys must go back to the game before anything else can go wrong.
      input::clearClaims();
      log::error("Access violation in the navigator; FactorioAccess native UI is off until restart");
      speech::say("FactorioAccess native UI failed and is now off", false);
   }
}

} // namespace fa::ui
