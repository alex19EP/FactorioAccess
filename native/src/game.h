#pragma once

#include <cstdint>

namespace fa::pdb {
class SymbolTable;
}

namespace fa::game {

// Addresses and class layouts read from factorio.pdb, so nothing here is tied to one build.
struct Layout {
   // Functions and globals.
   uintptr_t guiLogic = 0;         // virtual void agui::Gui::logic(bool)
   uintptr_t guiInstance = 0;      // static agui::Gui* agui::Gui::instance
   uintptr_t globalContext = 0;    // GlobalContext* global
   uintptr_t sdlPollEvent = 0;     // SDL_PollEvent_REAL, the statically linked SDL3
   uintptr_t scrollToVisible = 0;  // void agui::ScrollPane::scrollToMakeWidgetVisible(Widget*, ScrollMode)
   // void agui::Widget::dispatch*(MouseEvent const&): what the Gui calls on the widget under the
   // mouse, each running the widget's own handler and then its listeners.
   uintptr_t dispatchMouseEnter = 0;
   uintptr_t dispatchMouseDown = 0;
   uintptr_t dispatchClick = 0;
   uintptr_t dispatchMouseUp = 0;
   uintptr_t dispatchMouseLeave = 0;
   // bool PlayerInputSource::processNextDialog(): what the Confirm message control runs, closing
   // the scenario message dialog shown over the game.
   uintptr_t processNextDialog = 0;

   // GlobalContext
   uint32_t globalGui = 0;  // agui::Gui* of the application, the one the menus live in
   uint32_t globalGame = 0; // Game*, null outside a game
   uint32_t globalAppManager = 0;       // AppManager*
   uint32_t globalPlayerInputSource = 0; // PlayerInputSource*

   // AppManager: the stack of app states (InGame, InGameMenu, InSettingsMenu, ...), the top last.
   uint32_t appManagerStates = 0; // std::vector<std::unique_ptr<AppManagerState>>
   // AppManagerStateWithGuiManualConstruction<T>::gui, the window a menu state owns; every
   // instantiation lays it out alike.
   uint32_t appStateGui = 0;

   // The loaded game's view and the scenario message dialog it shows.
   uint32_t gameView = 0;            // Game::gameView
   uint32_t gameViewMessage = 0;     // GameView::scenarioMessageDialog, std::unique_ptr<SpeechBubbleGui>
   uint32_t speechBubbleLabel = 0;   // SpeechBubbleGui::messageLabel, an embedded agui::Label

   // agui::Gui
   uint32_t guiBaseWidget = 0;      // agui::TopContainer* baseWidget
   uint32_t guiFocusedWidget = 0;   // targeter in focusManager; holds a GenericTargetable*
   uint32_t guiWidgetUnderMouse = 0; // targeter; holds a GenericTargetable*
   uint32_t guiModals = 0;          // std::vector<FocusManager::WidgetWithPriority> in focusManager
   uint32_t modalSize = 0;          // sizeof(FocusManager::WidgetWithPriority)
   uint32_t modalWidget = 0;        // its GenericTargeter<Widget>
   uint32_t modalIsDropDown = 0;    // its bool isDropDownListBox: the list of an open DropDown

   // agui::GenericTargeterBase
   uint32_t targeterTarget = 0;

   // agui::Widget
   uint32_t widgetTargetable = 0; // GenericTargetable base, what targeters point at
   uint32_t widgetParent = 0;
   uint32_t widgetChildren = 0;        // std::vector<agui::Widget*>
   uint32_t widgetPrivateChildren = 0; // std::vector<agui::Widget*>; holds e.g. a Frame's content
   uint32_t widgetText = 0;            // std::string, already translated
   uint32_t widgetUsageBits = 0;
   uint32_t widgetLocation = 0;        // agui::Point {int x, y}, relative to the parent
   uint32_t widgetSize = 0;            // agui::Dimension {int width, height}

   // Virtual methods of agui::Widget, as vtable slots.
   uint32_t slotKeyDown = 0;     // bool keyDown(KeyEvent const&)
   uint32_t slotKeyUp = 0;       // bool keyUp(KeyEvent const&)
   uint32_t slotFocus = 0;       // void focus(NamedBool<TabbedInTag>)
   uint32_t slotIsFocusable = 0; // bool isFocusable() const

   // agui::Label keeps its caption here instead of in Widget::text.
   uint32_t labelText = 0; // std::string

   // Embedded agui::Label of an agui::Frame (and so of every Window).
   uint32_t frameTitle = 0;

   // Control state.
   uint32_t toggleChecked = 0; // agui::ToggleButton::checkedState
   uint32_t buttonToggled = 0; // agui::Button::toggled
   uint32_t sliderValue = 0;   // double
   uint32_t sliderMin = 0;
   uint32_t sliderMax = 0;
   uint32_t sliderStep = 0;    // double
   uint32_t switchState = 0;   // agui::SwitchState
   uint32_t textBoxReadOnly = 0;
   uint32_t textBoxText = 0;   // std::string
   uint32_t tableColumns = 0;  // agui::Table::columnCount
   uint32_t tabPane = 0;       // agui::Tab::tabPane
   uint32_t tabbedPaneTabs = 0;     // std::vector<TabbedPane::TabEntry>
   uint32_t tabbedPaneSelected = 0; // iterator into tabs: points at a TabEntry
   uint32_t tabEntryTab = 0;   // GenericTargeter<Tab> in TabbedPane::TabEntry
   uint32_t dropDownList = 0;     // agui::ListBox listBox, embedded in the DropDown
   uint32_t dropDownSelected = 0; // int selectedIndex

   // agui::KeyEvent, which the game's widgets take in keyDown/keyUp.
   uint32_t keyEventSize = 0;
   uint32_t keyEventUnichar = 0;
   uint32_t keyEventKeyCode = 0;
   uint32_t keyEventExtKey = 0;
   uint32_t keyEventKey = 0;
   uint32_t keyEventSource = 0;

   // agui::MouseEvent, for clicks delivered straight to a widget.
   uint32_t mouseEventSize = 0;
   uint32_t mouseEventPosition = 0; // agui::Point {int x, y}, relative to the widget
   uint32_t mouseEventButton = 0;
   uint32_t mouseEventType = 0;
   uint32_t mouseEventControl = 0;
   uint32_t mouseEventShift = 0;
   uint32_t mouseEventSource = 0;

   // Tooltips: Widget::toolTipCreator, and the title and text of the plain kind most buttons use.
   uint32_t widgetToolTipCreator = 0;
   uint32_t plainToolTipTitle = 0;
   uint32_t plainToolTipText = 0;

   // agui::ListBox and agui::TableWithSelection, the lists that keep a selection of their own.
   uint32_t listBoxItems = 0;         // std::vector<ListBoxItem>
   uint32_t listBoxItemSize = 0;
   uint32_t listBoxItemButton = 0;    // std::unique_ptr<TextButton>
   uint32_t tableSelectedIndex = 0;   // row, the header row being 0

   // Members of the game's windows the screens read by name.
   uint32_t dialogButtons = 0;        // GuiTemplate (Dialog<>) bottomButtonsFlow, the footer
   uint32_t loadMapList = 0;          // LoadMapGui::packageListGui
   uint32_t loadMapInfo = 0;          // LoadMapGui::mapInfo
   uint32_t mapInfoDelete = 0;        // MapInfoGui::deleteSaveButton
   uint32_t modsTabs = 0;             // ModsGui::tabs
   uint32_t modsManageTab = 0;        // ModsGui::manageTab
   uint32_t modsManagePane = 0;       // ModsGui::manageTabContents
   uint32_t manageModsTable = 0;      // ManageModsPane::modsTable
   uint32_t manageModsInfo = 0;       // ManageModsPane::modInfoPane
   uint32_t manageModsSearch = 0;     // ManageModsPane::searchBar
   uint32_t settingsContent = 0;      // SettingsGui::contentFrame
   uint32_t settingsReset = 0;        // SettingsGui::resetButton
   uint32_t controlsScrollPane = 0;   // ControlSettingsGui::scrollPane
   uint32_t controlsSetting = 0;      // ControlSettingsGui::currentlySetting, the button waiting for a key
};

// Bits of agui::Widget::usageBitMask, read from Widget::setVisible and Widget::isEnabled in
// 2.1.20. These are code constants, not PDB data, so they are the one part of the layout that a
// Factorio update can change silently.
inline constexpr uint32_t kUsageVisible = 0x4;
inline constexpr uint32_t kUsageHiddenMask = 0x20004; // visible only when (bits & mask) == visible
inline constexpr uint32_t kUsageEnabled = 0x8;
// Set on widgets that click as the button goes down; the Gui then sends no click on release.
// Read from Widget::dispatchMouseDown and Gui::handleMouseUp in 2.1.20.
inline constexpr uint32_t kUsageClickOnMouseDown = 0x800;

extern Layout layout;

// Fills `layout`; logs every missing name and returns false if anything could not be resolved.
bool resolve(pdb::SymbolTable& symbols);

} // namespace fa::game
