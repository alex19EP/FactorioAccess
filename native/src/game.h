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

   // Where the game world's cursor is: hover selection, building and opening an entity read the
   // first, the selection tools and the rest of PlayerInputSource the second. Both fall back to
   // the mouse.
   uintptr_t playerCursorPosition = 0; // Optional<MapPosition> Player::getCursorMapPosition() const
   uintptr_t sourceCursorPosition = 0; // MapPosition PlayerInputSource::getCursorMapPosition() const
   // void LuaHelper::initLuaState(lua_State*): sets up the globals of every Lua state the game
   // creates (log, localised_print, ...).
   uintptr_t initLuaState = 0;

   // The Lua 5.2 C API, linked into the game.
   uintptr_t luaCreateTable = 0;
   uintptr_t luaPushCClosure = 0;
   uintptr_t luaSetField = 0;
   uintptr_t luaSetGlobal = 0;
   uintptr_t luaCheckInteger = 0;
   uintptr_t luaCheckNumber = 0;
   uintptr_t luaGetTop = 0;
   uintptr_t luaSetTop = 0;
   uintptr_t luaPushLString = 0;
   uintptr_t luaRawSetI = 0;
   // LocalisedString LuaHelper::parseLocalisedString(lua_State*, int index, bool strict): what
   // localised_print reads its argument with. It throws ScriptException, a Lua error to the caller,
   // on a malformed string.
   uintptr_t parseLocalisedString = 0;
   uintptr_t localisedStringDestroy = 0; // LocalisedString::~LocalisedString()
   uint32_t localisedStringSize = 0;     // sizeof(LocalisedString)

   // GlobalContext
   uint32_t globalGui = 0;  // agui::Gui* of the application, the one the menus live in
   uint32_t globalGame = 0; // Game*, null outside a game
   uint32_t globalAppManager = 0;       // AppManager*
   uint32_t globalPlayerInputSource = 0; // PlayerInputSource*
   uint32_t globalInputState = 0;        // InputState*
   // InputState::mouseState.buttons: the SDL mouse button mask, bit n-1 for button n.
   uint32_t inputStateMouseButtons = 0;

   // AppManager: the stack of app states (InGame, InGameMenu, InSettingsMenu, ...), the top last.
   uint32_t appManagerStates = 0; // std::vector<std::unique_ptr<AppManagerState>>
   // AppManagerStateWithGuiManualConstruction<T>::gui, the window a menu state owns; every
   // instantiation lays it out alike.
   uint32_t appStateGui = 0;

   // The loaded game's view and the scenario message dialog it shows.
   uint32_t gameView = 0;            // Game::gameView
   uint32_t gameLocalPlayer = 0;     // Game::localPlayer, the Player* of this client
   uint32_t playerIndex = 0;         // Player::index, LuaPlayer::index
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
   uint32_t widgetToolTip = 0; // Widget::toolTip, the GenericTargeter<ToolTip> of the one shown
   // void Widget::checkCreateTooltip(): makes and shows the widget's tooltip as hovering does.
   uintptr_t checkCreateTooltip = 0;
   // bool Widget::removeToolTipWidget(bool destroy): takes it down again.
   uintptr_t removeToolTipWidget = 0;
   // ToolTip::updateContent(): fills a tooltip, which the Gui otherwise does at the end of its logic.
   uint32_t slotToolTipUpdateContent = 0;
   uint32_t plainToolTipTitle = 0;
   uint32_t plainToolTipText = 0;

   // agui::ListBox and agui::TableWithSelection, the lists that keep a selection of their own.
   uint32_t listBoxItems = 0;         // std::vector<ListBoxItem>
   uint32_t listBoxItemSize = 0;
   uint32_t listBoxItemButton = 0;    // std::unique_ptr<TextButton>
   uint32_t tableSelectedIndex = 0;   // row, the header row being 0

   // Members of the game's windows the screens read by name.
   uint32_t dialogButtons = 0;        // GuiTemplate (Dialog<>) bottomButtonsFlow, the footer
   // MenuGui<Result> (the main menu, single player, the game menu, ...): the frame of highlighted
   // buttons on top (Continue), the main buttons, and the bottom row (Exit, Back). Every
   // instantiation lays them out alike.
   uint32_t menuTop = 0;
   uint32_t menuMain = 0;
   uint32_t menuBottom = 0;
   uint32_t appVersionLabel = 0;      // AppManager::backgroundVersionLabel, std::unique_ptr<agui::Label>
   // MainMenuGui's panels beside the menu, each a std::unique_ptr.
   uint32_t mainMenuLanguage = 0;     // languageSelectionGui
   uint32_t mainMenuSimulation = 0;   // simulationSelectionGui
   uint32_t mainMenuAdvert = 0;       // spaceAgeAdvert
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
   uint32_t modSettingsTabs = 0;      // ModSettingsGui::tabs
   uint32_t tabbedPaneContent = 0;    // agui::TabbedPane::contentFrame, the selected tab's page
   uint32_t controlsScrollPane = 0;   // ControlSettingsGui::scrollPane
   uint32_t controlsSetting = 0;      // ControlSettingsGui::currentlySetting, the button waiting for a key
   uint32_t newGameMaps = 0;          // NewGameGui::mapsListBox, the scenarios
   uint32_t newGameLevels = 0;        // NewGameGui::levelsVerticalFlow, a campaign's levels
   uint32_t newGameDifficulty = 0;    // NewGameGui::difficultyVerticalFlow
   uint32_t newGameName = 0;          // NewGameGui::mapNameLabel
   uint32_t newGameReplay = 0;        // NewGameGui::enableReplayCheckBox
   uint32_t newGameDelete = 0;        // NewGameGui::deleteScenarioButton
   uint32_t newGameDescription = 0;   // NewGameGui::descriptionLabel
   uint32_t mapGenPresets = 0;        // MapGeneratorGui::mapGenSettingPresets
   uint32_t mapGenPresetReset = 0;    // MapGeneratorGui::resetPresetButton
   uint32_t mapGenPresetDescription = 0; // MapGeneratorGui::mapGeneratorPresetDescription
   uint32_t mapGenSeed = 0;           // MapGeneratorGui::mapSeedField
   uint32_t mapGenRandomSeed = 0;     // MapGeneratorGui::randomizeSeedButton
   uint32_t mapGenTabs = 0;           // MapGeneratorGui::tabbedPane
   uint32_t mapGenPages[4] = {};      // MapGeneratorGui::{resource,terrain,enemy,advanced}SettingsScrollPane
   uint32_t mapGenImport = 0;         // MapGeneratorGui::exchangeStringImportButton
   uint32_t mapGenExport = 0;         // MapGeneratorGui::exchangeStringExportButton
   uint32_t mapGenButtons = 0;        // MapGeneratorGui::mainButtonHFlow, its footer

   // Icons: IconButton::icon.sprite, the Sprite's owner (the prototype it depicts) and that
   // prototype's localised name.
   uint32_t iconButtonSprite = 0;     // Sprite*
   uint32_t spriteOwner = 0;          // PrototypeBase*
   uint32_t prototypeLocalisedName = 0; // LocalisedString
   // std::string const& LocalisedString::str(LocaleProvider const*) const: translates through the
   // game's own locale (null provider: the current one) and caches the result.
   uintptr_t localisedStringStr = 0;
   uint32_t prototypeName = 0;        // PrototypeBase::name, the internal name ("normal")

   // The item and recipe buttons of the character screen. A slot shows stack slotIndex of its
   // inventory, or without one the loose stack it points at (InventoryGuiSlot::getStack).
   uint32_t slotInventory = 0;        // InventoryGuiSlot::inventory, Inventory*
   uint32_t slotIndex = 0;            // InventoryGuiSlot::targetSpecification.slotIndex
   uint32_t slotItemStack = 0;        // InventoryGuiSlot::itemStack, ItemStack*
   uint32_t inventoryData = 0;        // Inventory::data, ItemStack[]
   uint32_t inventorySize = 0;        // Inventory::dataSize
   uint32_t itemStackSize = 0;        // sizeof(ItemStack)
   uint32_t itemStackCount = 0;       // ItemStack::count
   uint32_t itemStackItem = 0;        // ItemStack::itemID, an index into the item prototypes
   uint32_t itemStackQuality = 0;     // ItemStack::qualityID, an index into the quality prototypes
   uint32_t recipeSlotCount = 0;      // RecipeSlot::count, how many the player can craft now
   // SelectListGui<ID<RecipePrototype>>::slots, std::map<recipe ID, unique_ptr<agui::Button>>: the
   // crafting list's buttons by recipe.
   uint32_t recipeListSlots = 0;
   // PrototypeList<T>::indexToPrototype, the std::vector<T*> an ID indexes.
   uintptr_t itemPrototypes = 0;
   uintptr_t qualityPrototypes = 0;
   uintptr_t recipePrototypes = 0;

   // Slot buttons of every kind (SlotButtonBase: item, fluid, recipe and filter slots). The getters
   // are introduced by secondary bases, so they are called through those subobjects; slot numbers
   // are vtable slots of PrototypeProvider and ButtonNumber.
   uint32_t providerBasePrototype = 0;    // PrototypeBase const* PrototypeProvider::getBasePrototype() const
   uint32_t providerQualityPrototype = 0; // QualityPrototype const* PrototypeProvider::getQualityPrototype() const
   uint32_t buttonNumberCount = 0;        // double ButtonNumber::getCount() const, the number drawn on it

   uint32_t progressBarValue = 0; // agui::ProgressBar::value, 0 to 1

   // The windows of entities (GameGuiWithControllerInventory): the entity's window, which holds
   // the player's inventory beside the entity's own part.
   uint32_t entityMainWindow = 0;     // GameGuiWithControllerInventory::mainWindow, agui::Window
   uint32_t entityInventoryHolder = 0; // GameGuiWithControllerInventory::controllerInventory, ControllerInventoryHolder*
   uint32_t holderInventory = 0;      // GameControllerInventoryHolder::inventoryGui, InventoryGui
   uint32_t holderTitle = 0;          // GameControllerInventoryHolder::titleLabel ("Character")
   uint32_t frameHeader = 0;          // agui::Frame::headerFlow: the title bar's search and close buttons
   // The progress bars of crafting machines and drills, with the productivity bar beside each.
   uint32_t assemblerProgressBar = 0; // AssemblingMachineGui::productionProgressBar
   uint32_t assemblerBonusBar = 0;    // AssemblingMachineGui::bonusProgressBar
   uint32_t furnaceProgressBar = 0;   // FurnaceGui::productionProgressBar
   uint32_t furnaceBonusBar = 0;      // FurnaceGui::bonusProgressBar
   uint32_t drillProgressBar = 0;     // MiningDrillGui::miningProgressBar
   uint32_t drillBonusBar = 0;        // MiningDrillGui::bonusProgressBar
   // The recipe a crafting machine shows; a click on it opens Factoriopedia.
   uint32_t assemblerRecipe = 0;      // AssemblingMachineGui::recipeInfoWidget, RecipeInfoWidget
   uint32_t furnaceRecipe = 0;        // FurnaceGui::recipeInfoWidget
   // The slots of what a crafting machine takes and makes, agui::Table.
   uint32_t assemblerInputs = 0;      // AssemblingMachineGui::ingredientsTable
   uint32_t furnaceInputs = 0;        // FurnaceGui::ingredientsTable
   uint32_t assemblerOutputs = 0;     // AssemblingMachineGui::outputsTable
   uint32_t furnaceOutputs = 0;       // FurnaceGui::outputsTable
   // The module slots, an InventoryGui owned through a pointer; null where the entity takes none.
   uint32_t assemblerModules = 0;     // AssemblingMachineGui::slotInventory
   uint32_t furnaceModules = 0;       // FurnaceGui::slotInventory
   uint32_t drillModules = 0;         // MiningDrillGui::moduleSlotsGui
   // Beside an assembler's recipe unless the recipe is fixed: back to the recipe chooser.
   uint32_t assemblerChangeRecipe = 0; // AssemblingMachineGui::changeRecipeButton, IconButton
   // The fuel part of a burner-powered entity's window (BurnerInfo).
   uint32_t burnerSlots = 0;          // BurnerInfo::burnerSlotsTable, agui::Table
   uint32_t burntResultSlots = 0;     // BurnerInfo::burntResultSlotsTable, agui::Table
   uint32_t burnerProgressBar = 0;    // BurnerInfo::burningProgressBar: what is left of the fuel burning

   // What the game says about itself while the DLL runs (see disclosure.h).
   // static void Logging::log(char const* file, unsigned line, LogLevel, char const* format, ...)
   uintptr_t loggingLog = 0;
   // std::string& std::string::append(char const*, size_t), the game's own, so that the game's
   // allocator owns what it grows.
   uintptr_t stringAppend = 0;
   // std::string ApplicationVersion::strDetailedNoBuildMode() const: the version the main menu's
   // corner label and the About dialog show. Saves and multiplayer compare other strings.
   uintptr_t versionForDisplay = 0;
   uintptr_t labelSetText = 0;        // void agui::Label::setText(std::string const&)
   uintptr_t widgetSetToolTip = 0;    // agui::Widget& agui::Widget::setToolTip(std::string const&)
   uint32_t slotSetEnabled = 0;       // agui::Widget& agui::Widget::setEnabled(bool)
   uint32_t globalOtherSettings = 0;  // GlobalContext::otherSettings, OtherSettings*
   uint32_t crashLogItem = 0;         // OtherSettings::enableCrashLogUploading, SimpleConfigItem<bool>
   uint32_t configBoolValue = 0;      // SimpleConfigItem<bool>::value
   // OtherSettingsGui::boolOtherSettings, std::vector<std::unique_ptr<BoolGuiSetting>>: the
   // checkboxes of Settings > Other, each with the config item it edits.
   uint32_t otherSettingsBools = 0;
   uint32_t boolSettingItem = 0;      // BoolGuiSetting::setting, SimpleConfigItem<bool>*
   uint32_t boolSettingWidget = 0;    // BoolGuiSetting::widget, an embedded agui::CheckBox
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

// MapPosition coordinates are fixed point with 8 fractional bits.
inline constexpr int32_t kMapPositionScale = 256;

extern Layout layout;

// Fills `layout`; logs every missing name and returns false if anything could not be resolved.
bool resolve(pdb::SymbolTable& symbols);

} // namespace fa::game
