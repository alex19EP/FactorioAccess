#include "game.h"

#include "log.h"
#include "symbols.h"

namespace fa::game {

Layout layout;

bool resolve(pdb::SymbolTable& symbols) {
   bool ok = true;
   auto address = [&](uintptr_t& out, const char* name) {
      if (auto value = symbols.address(name))
         out = *value;
      else
         ok = false;
   };
   auto offset = [&](uint32_t& out, const char* type, const char* path) {
      if (auto value = symbols.offset(type, path))
         out = *value;
      else
         ok = false;
   };
   auto size = [&](uint32_t& out, const char* type) {
      if (auto value = symbols.size(type))
         out = *value;
      else
         ok = false;
   };
   auto classSlot = [&](uint32_t& out, const char* type, const char* method) {
      if (auto value = symbols.virtualSlot(type, method))
         out = *value;
      else
         ok = false;
   };
   auto slot = [&](uint32_t& out, const char* method) { classSlot(out, "agui::Widget", method); };

   address(layout.guiLogic, "?logic@Gui@agui@@UEAAX_N@Z");
   address(layout.guiInstance, "?instance@Gui@agui@@2PEAV12@EA");
   address(layout.globalContext, "?global@@3PEAVGlobalContext@@EA");
   address(layout.sdlPollEvent, "SDL_PollEvent_REAL");
   address(layout.scrollToVisible, "?scrollToMakeWidgetVisible@ScrollPane@agui@@QEAAXPEAVWidget@2@W4ScrollMode@@@Z");
   address(layout.dispatchMouseEnter, "?dispatchMouseEnter@Widget@agui@@QEAAXAEBVMouseEvent@2@@Z");
   address(layout.dispatchMouseDown, "?dispatchMouseDown@Widget@agui@@QEAAXAEBVMouseEvent@2@@Z");
   address(layout.dispatchClick, "?dispatchClick@Widget@agui@@QEAAXAEBVMouseEvent@2@@Z");
   address(layout.dispatchMouseUp, "?dispatchMouseUp@Widget@agui@@QEAAXAEBVMouseEvent@2@@Z");
   address(layout.dispatchMouseLeave, "?dispatchMouseLeave@Widget@agui@@QEAAXAEBVMouseEvent@2@@Z");
   address(layout.processNextDialog, "?processNextDialog@PlayerInputSource@@QEAA_NXZ");
   address(layout.playerCursorPosition,
           "?getCursorMapPosition@Player@@QEBA?AV?$Optional@VMapPosition@@U?$OptionalEmptyValue@VMapPosition@@@@@@XZ");
   address(layout.sourceCursorPosition, "?getCursorMapPosition@PlayerInputSource@@QEBA?AVMapPosition@@XZ");
   address(layout.initLuaState, "?initLuaState@LuaHelper@@YAXPEAUlua_State@@@Z");
   address(layout.luaCreateTable, "lua_createtable");
   address(layout.luaPushCClosure, "lua_pushcclosure");
   address(layout.luaSetField, "lua_setfield");
   address(layout.luaSetGlobal, "lua_setglobal");
   address(layout.luaCheckInteger, "luaL_checkinteger");
   address(layout.luaCheckNumber, "luaL_checknumber");
   address(layout.luaGetTop, "lua_gettop");
   address(layout.luaSetTop, "lua_settop");
   address(layout.luaPushLString, "lua_pushlstring");
   address(layout.luaRawSetI, "lua_rawseti");
   address(layout.parseLocalisedString, "?parseLocalisedString@LuaHelper@@YA?AVLocalisedString@@PEAUlua_State@@H_N@Z");
   address(layout.localisedStringDestroy, "??1LocalisedString@@QEAA@XZ");
   size(layout.localisedStringSize, "LocalisedString");

   offset(layout.globalGui, "GlobalContext", "gui");
   offset(layout.globalGame, "GlobalContext", "game");
   offset(layout.globalAppManager, "GlobalContext", "appManager");
   offset(layout.globalPlayerInputSource, "GlobalContext", "playerInputSource");
   offset(layout.globalInputState, "GlobalContext", "inputState.value");
   offset(layout.inputStateMouseButtons, "InputState", "mouseState.buttons");
   offset(layout.appManagerStates, "AppManager", "stateStack");
   offset(layout.appStateGui, "AppManagerStateWithGuiManualConstruction<GameMenuGui>", "gui");
   offset(layout.gameView, "Game", "gameView");
   offset(layout.gameLocalPlayer, "Game", "localPlayer");
   offset(layout.playerIndex, "Player", "index");
   offset(layout.gameViewMessage, "GameView", "scenarioMessageDialog");
   offset(layout.speechBubbleLabel, "SpeechBubbleGui", "messageLabel");

   offset(layout.guiBaseWidget, "agui::Gui", "baseWidget");
   offset(layout.guiFocusedWidget, "agui::Gui", "focusManager.focusedWidget");
   offset(layout.guiWidgetUnderMouse, "agui::Gui", "widgetUnderMouse");
   offset(layout.guiModals, "agui::Gui", "focusManager.modals");
   size(layout.modalSize, "agui::FocusManager::WidgetWithPriority");
   offset(layout.modalWidget, "agui::FocusManager::WidgetWithPriority", "widget");
   offset(layout.modalIsDropDown, "agui::FocusManager::WidgetWithPriority", "isDropDownListBox");
   offset(layout.targeterTarget, "agui::GenericTargeterBase", "target");

   offset(layout.widgetTargetable, "agui::Widget", "agui::GenericTargetable");
   offset(layout.widgetParent, "agui::Widget", "parentWidget");
   offset(layout.widgetChildren, "agui::Widget", "children");
   offset(layout.widgetPrivateChildren, "agui::Widget", "privateChildren");
   offset(layout.widgetText, "agui::Widget", "text");
   offset(layout.widgetUsageBits, "agui::Widget", "usageBitMask");
   offset(layout.widgetLocation, "agui::Widget", "location");
   offset(layout.widgetSize, "agui::Widget", "size");
   slot(layout.slotKeyDown, "keyDown");
   slot(layout.slotKeyUp, "keyUp");
   slot(layout.slotFocus, "focus");
   slot(layout.slotIsFocusable, "isFocusable");

   offset(layout.labelText, "agui::Label", "resizableText.data");
   offset(layout.frameTitle, "agui::Frame", "title");

   offset(layout.toggleChecked, "agui::ToggleButton", "checkedState");
   offset(layout.buttonToggled, "agui::Button", "toggled");
   offset(layout.sliderValue, "agui::Slider", "value");
   offset(layout.sliderMin, "agui::Slider", "min");
   offset(layout.sliderMax, "agui::Slider", "max");
   offset(layout.sliderStep, "agui::Slider", "valueStep");
   offset(layout.switchState, "agui::Switch", "state");
   offset(layout.textBoxReadOnly, "agui::TextBox", "readOnly");
   offset(layout.textBoxText, "agui::TextBox", "resizableText.data");
   offset(layout.tableColumns, "agui::Table", "columnCount");
   offset(layout.tabPane, "agui::Tab", "tabPane");
   offset(layout.tabbedPaneTabs, "agui::TabbedPane", "tabs");
   offset(layout.tabbedPaneSelected, "agui::TabbedPane", "selectedTab");
   offset(layout.tabEntryTab, "agui::TabbedPane::TabEntry", "tab");
   offset(layout.dropDownList, "agui::DropDown", "listBox");
   offset(layout.dropDownSelected, "agui::DropDown", "selectedIndex");

   size(layout.keyEventSize, "agui::KeyEvent");
   offset(layout.keyEventUnichar, "agui::KeyEvent", "unichar");
   offset(layout.keyEventKeyCode, "agui::KeyEvent", "keyCode");
   offset(layout.keyEventExtKey, "agui::KeyEvent", "extKey");
   offset(layout.keyEventKey, "agui::KeyEvent", "key");
   offset(layout.keyEventSource, "agui::KeyEvent", "source");

   size(layout.mouseEventSize, "agui::MouseEvent");
   offset(layout.mouseEventPosition, "agui::MouseEvent", "position");
   offset(layout.mouseEventButton, "agui::MouseEvent", "button");
   offset(layout.mouseEventType, "agui::MouseEvent", "eventType");
   offset(layout.mouseEventControl, "agui::MouseEvent", "isControl");
   offset(layout.mouseEventShift, "agui::MouseEvent", "isShift");
   offset(layout.mouseEventSource, "agui::MouseEvent", "source");

   offset(layout.widgetToolTipCreator, "agui::Widget", "toolTipCreator");
   offset(layout.widgetToolTip, "agui::Widget", "toolTip");
   address(layout.checkCreateTooltip, "?checkCreateTooltip@Widget@agui@@QEAAXXZ");
   address(layout.removeToolTipWidget, "?removeToolTipWidget@Widget@agui@@QEAA_N_N@Z");
   classSlot(layout.slotToolTipUpdateContent, "agui::ToolTip", "updateContent");
   offset(layout.plainToolTipTitle, "agui::PlainToolTipCreator", "title");
   offset(layout.plainToolTipText, "agui::PlainToolTipCreator", "text");

   offset(layout.listBoxItems, "agui::ListBox", "items");
   size(layout.listBoxItemSize, "agui::ListBoxItem");
   offset(layout.listBoxItemButton, "agui::ListBoxItem", "button");
   offset(layout.tableSelectedIndex, "agui::TableWithSelection", "selectedIndex");

   // Every Dialog<Result> instantiation lays its members out alike.
   offset(layout.dialogButtons, "GuiTemplate", "Dialog<enum ConfirmCancelResult>.bottomButtonsFlow");
   offset(layout.menuTop, "MainMenuGui", "MenuGui<enum MainMenuResult>.topButtonsFrame");
   offset(layout.menuMain, "MainMenuGui", "MenuGui<enum MainMenuResult>.mainButtonsFrame");
   offset(layout.menuBottom, "MainMenuGui", "MenuGui<enum MainMenuResult>.bottomPart");
   offset(layout.appVersionLabel, "AppManager", "backgroundVersionLabel");
   offset(layout.mainMenuLanguage, "MainMenuGui", "languageSelectionGui");
   offset(layout.mainMenuSimulation, "MainMenuGui", "simulationSelectionGui");
   offset(layout.mainMenuAdvert, "MainMenuGui", "spaceAgeAdvert");
   offset(layout.loadMapList, "LoadMapGui", "packageListGui");
   offset(layout.loadMapInfo, "LoadMapGui", "mapInfo");
   offset(layout.mapInfoDelete, "MapInfoGui", "deleteSaveButton");
   offset(layout.modsTabs, "ModsGui", "tabs");
   offset(layout.modsManageTab, "ModsGui", "manageTab");
   offset(layout.modsManagePane, "ModsGui", "manageTabContents");
   offset(layout.manageModsTable, "ManageModsPane", "modsTable");
   offset(layout.manageModsInfo, "ManageModsPane", "modInfoPane");
   offset(layout.manageModsSearch, "ManageModsPane", "searchBar");
   offset(layout.settingsContent, "SettingsGui", "contentFrame");
   offset(layout.settingsReset, "SettingsGui", "resetButton");
   offset(layout.modSettingsTabs, "ModSettingsGui", "tabs");
   offset(layout.tabbedPaneContent, "agui::TabbedPane", "contentFrame");
   offset(layout.controlsScrollPane, "ControlSettingsGui", "scrollPane");
   offset(layout.controlsSetting, "ControlSettingsGui", "currentlySetting");
   offset(layout.newGameMaps, "NewGameGui", "mapsListBox");
   offset(layout.newGameLevels, "NewGameGui", "levelsVerticalFlow");
   offset(layout.newGameDifficulty, "NewGameGui", "difficultyVerticalFlow");
   offset(layout.newGameName, "NewGameGui", "mapNameLabel");
   offset(layout.newGameReplay, "NewGameGui", "enableReplayCheckBox");
   offset(layout.newGameDelete, "NewGameGui", "deleteScenarioButton");
   offset(layout.newGameDescription, "NewGameGui", "descriptionLabel");
   offset(layout.mapGenPresets, "MapGeneratorGui", "mapGenSettingPresets");
   offset(layout.mapGenPresetReset, "MapGeneratorGui", "resetPresetButton");
   offset(layout.mapGenPresetDescription, "MapGeneratorGui", "mapGeneratorPresetDescription");
   offset(layout.mapGenSeed, "MapGeneratorGui", "mapSeedField");
   offset(layout.mapGenRandomSeed, "MapGeneratorGui", "randomizeSeedButton");
   offset(layout.mapGenTabs, "MapGeneratorGui", "tabbedPane");
   offset(layout.mapGenPages[0], "MapGeneratorGui", "resourceSettingsScrollPane");
   offset(layout.mapGenPages[1], "MapGeneratorGui", "terrainSettingsScrollPane");
   offset(layout.mapGenPages[2], "MapGeneratorGui", "enemySettingsScrollPane");
   offset(layout.mapGenPages[3], "MapGeneratorGui", "advancedSettingsScrollPane");
   offset(layout.mapGenImport, "MapGeneratorGui", "exchangeStringImportButton");
   offset(layout.mapGenExport, "MapGeneratorGui", "exchangeStringExportButton");
   offset(layout.mapGenButtons, "MapGeneratorGui", "mainButtonHFlow");

   offset(layout.iconButtonSprite, "IconButton", "icon.sprite");
   offset(layout.spriteOwner, "Sprite", "owner");
   offset(layout.prototypeLocalisedName, "PrototypeBase", "localisedName");
   address(layout.localisedStringStr,
           "?str@LocalisedString@@QEBAAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@PEBVLocaleProvider@@@Z");
   offset(layout.prototypeName, "PrototypeBase", "name");

   offset(layout.slotInventory, "InventoryGuiSlot", "inventory");
   offset(layout.slotIndex, "InventoryGuiSlot", "targetSpecification.slotIndex");
   offset(layout.slotItemStack, "InventoryGuiSlot", "itemStack");
   offset(layout.inventoryData, "Inventory", "data");
   offset(layout.inventorySize, "Inventory", "dataSize");
   offset(layout.inventoryBar, "Inventory", "bar");
   offset(layout.inventoryGuiInventory, "InventoryGui", "inventory");
   offset(layout.barGuiButton, "InventoryWithBarGui", "setBarSlot");
   offset(layout.barGuiMode, "InventoryWithBarGui", "mode");
   size(layout.itemStackSize, "ItemStack");
   offset(layout.itemStackCount, "ItemStack", "count");
   offset(layout.itemStackItem, "ItemStack", "itemID");
   offset(layout.itemStackQuality, "ItemStack", "qualityID");
   offset(layout.recipeSlotCount, "RecipeSlot", "count");
   address(layout.itemPrototypes,
           "?indexToPrototype@?$PrototypeList@VItemPrototype@@@@2V?$vector@PEAVItemPrototype@@V?$allocator@"
           "PEAVItemPrototype@@@std@@@std@@A");
   offset(layout.recipeListSlots, "SelectListGui<ID<RecipePrototype,unsigned short> >", "slots");
   address(layout.qualityPrototypes,
           "?indexToPrototype@?$PrototypeList@VQualityPrototype@@@@2V?$vector@PEAVQualityPrototype@@V?$allocator@"
           "PEAVQualityPrototype@@@std@@@std@@A");
   address(layout.recipePrototypes,
           "?indexToPrototype@?$PrototypeList@VRecipePrototype@@@@2V?$vector@PEAVRecipePrototype@@V?$allocator@"
           "PEAVRecipePrototype@@@std@@@std@@A");

   classSlot(layout.providerBasePrototype, "PrototypeProvider", "getBasePrototype");
   classSlot(layout.providerQualityPrototype, "PrototypeProvider", "getQualityPrototype");
   classSlot(layout.buttonNumberCount, "ButtonNumber", "getCount");
   offset(layout.progressBarValue, "agui::ProgressBar", "value");

   offset(layout.entityMainWindow, "GameGuiWithControllerInventory", "mainWindow");
   offset(layout.entityInventoryHolder, "GameGuiWithControllerInventory", "controllerInventory");
   offset(layout.holderInventory, "GameControllerInventoryHolder", "inventoryGui");
   offset(layout.holderTitle, "GameControllerInventoryHolder", "titleLabel");
   offset(layout.frameHeader, "agui::Frame", "headerFlow");
   offset(layout.assemblerProgressBar, "AssemblingMachineGui", "productionProgressBar");
   offset(layout.assemblerBonusBar, "AssemblingMachineGui", "bonusProgressBar");
   offset(layout.furnaceProgressBar, "FurnaceGui", "productionProgressBar");
   offset(layout.furnaceBonusBar, "FurnaceGui", "bonusProgressBar");
   offset(layout.drillProgressBar, "MiningDrillGui", "miningProgressBar");
   offset(layout.drillBonusBar, "MiningDrillGui", "bonusProgressBar");
   offset(layout.assemblerRecipe, "AssemblingMachineGui", "recipeInfoWidget");
   offset(layout.furnaceRecipe, "FurnaceGui", "recipeInfoWidget");
   offset(layout.assemblerInputs, "AssemblingMachineGui", "ingredientsTable");
   offset(layout.furnaceInputs, "FurnaceGui", "ingredientsTable");
   offset(layout.assemblerOutputs, "AssemblingMachineGui", "outputsTable");
   offset(layout.furnaceOutputs, "FurnaceGui", "outputsTable");
   offset(layout.assemblerModules, "AssemblingMachineGui", "slotInventory");
   offset(layout.furnaceModules, "FurnaceGui", "slotInventory");
   offset(layout.drillModules, "MiningDrillGui", "moduleSlotsGui");
   offset(layout.assemblerChangeRecipe, "AssemblingMachineGui", "changeRecipeButton");
   offset(layout.burnerSlots, "BurnerInfo", "burnerSlotsTable");
   offset(layout.burntResultSlots, "BurnerInfo", "burntResultSlotsTable");
   offset(layout.burnerProgressBar, "BurnerInfo", "burningProgressBar");

   offset(layout.gameViewControllerView, "GameView", "controllerView");
   classSlot(layout.controllerViewQuickBar, "ControllerView", "getQuickBar");
   offset(layout.quickBarMainRows, "QuickBarGui", "mainWindowRows");
   offset(layout.quickBarPickerRows, "QuickBarGui", "pageSelectorRows");
   offset(layout.quickBarPicker, "QuickBarGui", "pageSelectorFrame");
   offset(layout.quickBarPickingFor, "QuickBarGui", "selectingNewPageForRow");
   offset(layout.rowPage, "QuickBarGui::RowWidgets", "pageIndex");
   offset(layout.rowButton, "QuickBarGui::RowWidgets", "button");
   offset(layout.rowSlots, "QuickBarGui::RowWidgets", "slots");

   address(layout.loggingLog, "?log@Logging@@SAXPEBDIW4LogLevel@@0ZZ");
   address(layout.stringAppend, "?append@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAAEAV12@QEBD_K@Z");
   address(layout.versionForDisplay,
           "?strDetailedNoBuildMode@ApplicationVersion@@QEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@"
           "2@@std@@XZ");
   address(layout.labelSetText,
           "?setText@Label@agui@@UEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z");
   address(layout.widgetSetToolTip,
           "?setToolTip@Widget@agui@@QEAAAEAV12@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z");
   slot(layout.slotSetEnabled, "setEnabled");
   offset(layout.globalOtherSettings, "GlobalContext", "otherSettings.value");
   offset(layout.crashLogItem, "OtherSettings", "enableCrashLogUploading");
   offset(layout.configBoolValue, "SimpleConfigItem<bool>", "value");
   offset(layout.otherSettingsBools, "OtherSettingsGui", "boolOtherSettings");
   offset(layout.boolSettingItem, "BoolGuiSetting", "setting");
   offset(layout.boolSettingWidget, "BoolGuiSetting", "widget");

   if (ok) {
      log::info("Layout: Gui baseWidget {:#x} focused {:#x} modals {:#x} (entry {} bytes); Widget parent {:#x} "
                "children {:#x} privateChildren {:#x} text {:#x} usage {:#x}; Label text {:#x}; vtable slots keyDown "
                "{} keyUp {} focus {} isFocusable {}; KeyEvent {} bytes",
                layout.guiBaseWidget, layout.guiFocusedWidget, layout.guiModals, layout.modalSize, layout.widgetParent,
                layout.widgetChildren, layout.widgetPrivateChildren, layout.widgetText, layout.widgetUsageBits,
                layout.labelText, layout.slotKeyDown, layout.slotKeyUp, layout.slotFocus, layout.slotIsFocusable,
                layout.keyEventSize);
   }
   return ok;
}

} // namespace fa::game
