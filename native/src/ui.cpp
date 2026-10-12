#include "ui.h"

#include "agui.h"
#include "chart.h"
#include "devserver.h"
#include "disclosure.h"
#include "input.h"
#include "log.h"
#include "navigator/ScreenManager.hpp"
#include "reader.h"
#include "screens/BeltScreen.hpp"
#include "screens/AlertsScreen.hpp"
#include "screens/CharacterScreen.hpp"
#include "screens/ConsoleScreen.hpp"
#include "screens/ControlSettingsScreen.hpp"
#include "screens/CraftingQueueScreen.hpp"
#include "screens/TrackedAchievementsScreen.hpp"
#include "screens/DropDownScreen.hpp"
#include "screens/FactoriopediaScreen.hpp"
#include "screens/FilterSelectScreen.hpp"
#include "screens/GenericWindowScreen.hpp"
#include "screens/LoadGameScreen.hpp"
#include "screens/DeconstructionPlannerScreen.hpp"
#include "screens/UpgradePlannerScreen.hpp"
#include "screens/BlueprintBookScreen.hpp"
#include "screens/BlueprintLibraryScreen.hpp"
#include "screens/AchievementsScreen.hpp"
#include "screens/TipsAndTricksScreen.hpp"
#include "screens/BlueprintSetupScreen.hpp"
#include "screens/GameDialogScreen.hpp"
#include "screens/MachineScreen.hpp"
#include "screens/MapGeneratorScreen.hpp"
#include "screens/MapSearchScreen.hpp"
#include "screens/ElectricNetworkScreen.hpp"
#include "screens/MenuScreen.hpp"
#include "screens/ModSettingsScreen.hpp"
#include "screens/ModsScreen.hpp"
#include "screens/NewGameScreen.hpp"
#include "screens/PipeScreen.hpp"
#include "screens/ModWindowsScreen.hpp"
#include "screens/QuickBarScreen.hpp"
#include "screens/ScenarioMessageScreen.hpp"
#include "screens/SelectedInfoScreen.hpp"
#include "screens/SettingsScreen.hpp"
#include "screens/ShortcutBarScreen.hpp"
#include "screens/SideMenuScreen.hpp"
#include "screens/MapViewOptionsScreen.hpp"
#include "screens/EditorPanelScreen.hpp"
#include "screens/SplitterScreen.hpp"
#include "screens/GoalScreen.hpp"
#include "screens/StatusScreen.hpp"
#include "screens/TechnologyScreen.hpp"
#include "screens/TooltipScreen.hpp"
#include "speech.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <format>
#include <memory>
#include <string>
#include <vector>

namespace fa::ui {

namespace {

bool g_disabled = false;

void tick(const agui::Gui* gui) {
   // Only the application Gui carries screens; the main menu's background simulation runs a Gui
   // of its own every frame, which is recreated at will.
   if (gui != agui::applicationGui()) return;
   disclosure::tick();
   screens::WindowScreen::SetGui(gui);
   nav::ScreenManager::Get().Update();
   screens::QuickBarScreen::WatchPage();
   reader::tick();
   chart::tick();
   dev::pump();
}

// A wrong offset or a widget freed under us shows up as an access violation. Stop rather than
// take the game down; nothing here may unwind C++ objects, so the work lives in tick().
// The faulting instruction and the address it touched, for the log.
uintptr_t g_faultAt = 0;
uintptr_t g_faultAddress = 0;

int accessViolation(const EXCEPTION_POINTERS* info) {
   const EXCEPTION_RECORD* record = info->ExceptionRecord;
   if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) return EXCEPTION_CONTINUE_SEARCH;
   g_faultAt = reinterpret_cast<uintptr_t>(record->ExceptionAddress);
   g_faultAddress = record->NumberParameters > 1 ? record->ExceptionInformation[1] : 0;
   return EXCEPTION_EXECUTE_HANDLER;
}

bool guardedTick(const agui::Gui* gui) {
   __try {
      tick(gui);
      return true;
   } __except (accessViolation(GetExceptionInformation())) {
      return false;
   }
}

// "fa_native.dll+0x1234": the module an address lies in and how far into it.
std::string whereIs(uintptr_t address) {
   HMODULE module = nullptr;
   if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(address), &module))
      return std::format("{:#x}", address);
   char path[MAX_PATH] = {};
   GetModuleFileNameA(module, path, MAX_PATH);
   const char* name = std::strrchr(path, '\\');
   return std::format("{}+{:#x}", name ? name + 1 : path, address - reinterpret_cast<uintptr_t>(module));
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
   // The scenario's message dialog over a loaded game, above its windows.
   manager.Register(std::make_unique<screens::ScenarioMessageScreen>());
   // The game's character screen (E) over a loaded game; the game menu takes over while it is open.
   manager.Register(std::make_unique<screens::CharacterScreen>());
   // An entity's window (a chest, a furnace, a drill, ...), opened by the game's own open-gui control.
   manager.Register(std::make_unique<screens::MachineScreen>());
   // The planners' windows, opened from the planner's slot.
   manager.Register(std::make_unique<screens::DeconstructionPlannerScreen>());
   manager.Register(std::make_unique<screens::UpgradePlannerScreen>());
   // A blueprint's setup, opened from its slot or by selecting an area with a blank one.
   manager.Register(std::make_unique<screens::BlueprintSetupScreen>());
   // A blueprint book's window, opened from its slot.
   manager.Register(std::make_unique<screens::BlueprintBookScreen>());
   // The blueprint library's window, from its key or the shortcut bar.
   manager.Register(std::make_unique<screens::BlueprintLibraryScreen>());
   // The achievements window, from the side menu.
   manager.Register(std::make_unique<screens::AchievementsScreen>());
   // The tips and tricks window, from the side menu or the "New tip" button.
   manager.Register(std::make_unique<screens::TipsAndTricksScreen>());
   // Any other dialog over a game: a planner's name, description and icons.
   manager.Register(std::make_unique<screens::GameDialogScreen>());
   // A transport belt's window, with the mod's views of what the belt carries.
   manager.Register(std::make_unique<screens::BeltScreen>());
   manager.Register(std::make_unique<screens::SplitterScreen>());
   // A pipe's or a storage tank's window, with the mod's views of where its pipeline goes.
   manager.Register(std::make_unique<screens::PipeScreen>());
   // An electric pole's network window, with the mod's views of its wires and supply area, and the
   // surface's like it.
   manager.Register(std::make_unique<screens::ElectricNetworkScreen>());
   // The windows mods and the scenario put on the screen, while Ctrl+Tab has moved to them; it
   // also announces one that shows up.
   manager.Register(std::make_unique<screens::ModWindowsScreen>());
   // The map editor's panel, the part after them.
   manager.Register(std::make_unique<screens::EditorPanelScreen>());
   // The scenario's goal, the part after them.
   manager.Register(std::make_unique<screens::GoalScreen>());
   // The quickbar, the part after it.
   manager.Register(std::make_unique<screens::QuickBarScreen>());
   // The shortcut bar, the part after it.
   manager.Register(std::make_unique<screens::ShortcutBarScreen>());
   // The side menu, the part after that.
   manager.Register(std::make_unique<screens::SideMenuScreen>());
   // Remote view's map view options, the part after the side menu.
   manager.Register(std::make_unique<screens::MapViewOptionsScreen>());
   // The HUD's status, the part after that.
   manager.Register(std::make_unique<screens::StatusScreen>());
   // The crafting queue, the part after the status.
   manager.Register(std::make_unique<screens::CraftingQueueScreen>());
   // The tracked achievements, the part of the HUD after it.
   manager.Register(std::make_unique<screens::TrackedAchievementsScreen>());
   // The game's info panel for what the cursor points at, which the Y key opens over the map.
   manager.Register(std::make_unique<screens::SelectedInfoScreen>());
   // Remote view's map search, while its field shows.
   manager.Register(std::make_unique<screens::MapSearchScreen>());
   // The technology window, over the window it stacks on.
   manager.Register(std::make_unique<screens::TechnologyScreen>());
   // The alerts window an alert button opens, over the window it stacks on.
   manager.Register(std::make_unique<screens::AlertsScreen>());
   // Factoriopedia, over the window it stacks on, the technology window's too.
   manager.Register(std::make_unique<screens::FactoriopediaScreen>());
   // The console, over the map, the HUD or the window it was opened over.
   manager.Register(std::make_unique<screens::ConsoleScreen>());
   // Over it or a window, the chooser of a filter one of their slots opened.
   manager.Register(std::make_unique<screens::FilterSelectScreen>());
   // Over any of them, an open dropdown's list.
   manager.Register(std::make_unique<screens::DropDownScreen>());
   // Over whichever screen it came from, a tooltip too long to say at once.
   manager.Register(std::make_unique<screens::TooltipScreen>());
}

void afterGuiLogic(const agui::Gui* gui) {
   if (g_disabled) return;
   if (!guardedTick(gui)) {
      g_disabled = true;
      // Keys must go back to the game before anything else can go wrong.
      input::clearClaims();
      log::error("Access violation in the navigator at {} touching {:#x}; FactorioAccess native UI is off until "
                 "restart",
                 whereIs(g_faultAt), g_faultAddress);
      speech::say("FactorioAccess native UI failed and is now off", false);
   }
}

} // namespace fa::ui
