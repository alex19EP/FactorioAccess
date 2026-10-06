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
   auto slot = [&](uint32_t& out, const char* method) {
      if (auto value = symbols.virtualSlot("agui::Widget", method))
         out = *value;
      else
         ok = false;
   };

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

   offset(layout.globalGui, "GlobalContext", "gui");
   offset(layout.globalGame, "GlobalContext", "game");
   offset(layout.globalAppManager, "GlobalContext", "appManager");
   offset(layout.globalPlayerInputSource, "GlobalContext", "playerInputSource");
   offset(layout.appManagerStates, "AppManager", "stateStack");
   offset(layout.appStateGui, "AppManagerStateWithGuiManualConstruction<GameMenuGui>", "gui");
   offset(layout.gameView, "Game", "gameView");
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
   offset(layout.plainToolTipTitle, "agui::PlainToolTipCreator", "title");
   offset(layout.plainToolTipText, "agui::PlainToolTipCreator", "text");

   offset(layout.listBoxItems, "agui::ListBox", "items");
   size(layout.listBoxItemSize, "agui::ListBoxItem");
   offset(layout.listBoxItemButton, "agui::ListBoxItem", "button");
   offset(layout.tableSelectedIndex, "agui::TableWithSelection", "selectedIndex");

   // Every Dialog<Result> instantiation lays its members out alike.
   offset(layout.dialogButtons, "GuiTemplate", "Dialog<enum ConfirmCancelResult>.bottomButtonsFlow");
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
   offset(layout.controlsScrollPane, "ControlSettingsGui", "scrollPane");
   offset(layout.controlsSetting, "ControlSettingsGui", "currentlySetting");

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
