#pragma once

#include <cstddef>
#include <cstdint>

namespace fa::pdb {
class SymbolTable;
}

namespace fa::game {

// The prototype types rich text names by tag ([item=iron-plate]) or sprite path (item/iron-plate):
// that word and the class whose PrototypeList holds them.
struct NamedPrototype {
   const char* tag;
   const char* type;
};
inline constexpr NamedPrototype kNamedPrototypes[] = {
    {"item", "ItemPrototype"},
    {"entity", "EntityPrototype"},
    {"fluid", "FluidPrototype"},
    {"recipe", "RecipePrototype"},
    {"technology", "TechnologyPrototype"},
    {"tile", "TilePrototype"},
    {"virtual-signal", "VirtualSignalPrototype"},
    {"quality", "QualityPrototype"},
    {"item-group", "ItemGroup"},
    {"space-location", "SpaceLocationPrototype"},
    {"planet", "SpaceLocationPrototype"},
    {"achievement", "AchievementPrototype"},
    {"asteroid-chunk", "AsteroidChunkPrototype"},
    {"shortcut", "ShortcutPrototype"},
    {"equipment", "EquipmentPrototype"},
    {"airborne-pollutant", "AirbornePollutantPrototype"},
};
inline constexpr size_t kNamedPrototypeCount = sizeof(kNamedPrototypes) / sizeof(kNamedPrototypes[0]);

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
   // Pair<bool, agui::Widget*> determineWidgetUnderMouse(): whether the real mouse is over a GUI
   // rather than the game view. A GUI control bound like a world control (craft and build on the
   // left button, craft-5 and mine on the right) is held only while it says so.
   uintptr_t determineWidgetUnderMouse = 0;
   // bool PlayerInputSource::processNextDialog(): what the Confirm message control runs, closing
   // the scenario message dialog shown over the game.
   uintptr_t processNextDialog = 0;

   // Where the game world's cursor is: hover selection, building and opening an entity read the
   // first, the selection tools and the rest of PlayerInputSource the second. Both fall back to
   // the mouse.
   uintptr_t playerCursorPosition = 0; // Optional<MapPosition> Player::getCursorMapPosition() const
   uintptr_t sourceCursorPosition = 0; // MapPosition PlayerInputSource::getCursorMapPosition() const
   // Drag building. void ClientDragBuildingContext::update(PlayerInputSource&) runs once a frame from
   // PlayerInputSource::sendStateChanges: while a build control is held it builds at the cursor and
   // along the cursor's path, in a straight line. Rotate during a belt drag
   // (ClientDragBuildingContext::smartDirectionChange from PlayerInputSource::processRotate) sets
   // belt.applySmartDirectionChangeWhenPossible instead of turning the belt in hand; the update
   // turns the line once the cursor is off it and clears the flag. Releasing the control runs
   // ClientDragBuildingContext::stopped, which empties startPosition.
   uintptr_t dragBuildingUpdate = 0;
   uint32_t inputSourcePlayer = 0;      // PlayerInputSource::player, Player*
   uint32_t inputSourceDragContext = 0; // PlayerInputSource::manualBuilder.dragBuildingContext
   uint32_t dragStartPosition = 0;      // ClientDragBuildingContext::startPosition, Optional<MapPosition>
   uint32_t dragTurnPending = 0;        // ClientDragBuildingContext::belt.applySmartDirectionChangeWhenPossible
   // bool ControlInput::isActive(bool, NamedBool<GuiCheckTag>, bool, NamedBool<CheckModifiersTag>)
   // const: whether a control is held, as every held control is read.
   uintptr_t controlInputIsActive = 0;
   // Zoom. bool PlayerInputSource::processZoom(Event const&) takes a zoom control triggered while
   // not held, a wheel notch, and calls void PlayerInputSource::zoom(ZoomDirection, double steps),
   // which makes the ZoomAroundPoint action. A held key zooms instead from
   // PlayerInputSource::sendStateChanges, a little every tick it reads the control as held.
   uintptr_t processZoom = 0;
   uintptr_t playerInputSourceZoom = 0;
   uint32_t controlSettingsZoomIn = 0;  // ControlSettings::zoomIn, the embedded ControlInput
   uint32_t controlSettingsZoomOut = 0; // ControlSettings::zoomOut
   // InterfaceSettings::zoomTowardsCursor.value, bool: zoom around the mouse rather than the middle
   // of the screen, where the camera can move (remote view, the map).
   uint32_t zoomTowardsCursor = 0;
   // The current controller's zoom: slot of Zoomer* GameAdapter::getZoomer(), and Zoomer::config.
   // zoomRate, double, the factor of one wheel notch (2^(1/7) for the character, 2^(1/3) for the
   // remote view).
   uint32_t adapterGetZoomer = 0;
   uint32_t zoomerRate = 0;
   // Where this client builds. SimpleBuildInput Player::getSimpleBuildInput(ClientDragBuildingContext
   // const*) const reads the cursor, takes the build direction and snaps the position to the grid,
   // for the build control, drag building and Player::buildFromCursor. ItemToBuildDrawnType
   // BuildingRenderer::prepareBuildingInGame(Player const*, MapPosition const&, DrawQueue&) draws
   // the item in hand at a position its caller takes from the cursor. bool
   // Player::buildFromCursor(Optional<MapPosition> const&, NamedBool<GhostModeTag>,
   // ClientDragBuildingContext const*) runs only from LuaPlayer::luaRawBuildFromCursor, in the game
   // state on every client.
   uintptr_t simpleBuildInput = 0;
   uintptr_t prepareBuildingInGame = 0;
   uintptr_t playerBuildFromCursor = 0;
   uint32_t simpleBuildInputSize = 0;      // sizeof(SimpleBuildInput)
   uint32_t simpleBuildInputEntity = 0;    // SimpleBuildInput's BuildID::entityID, ID<EntityPrototype> first
   uint32_t simpleBuildInputRailPlanner = 0; // SimpleBuildInput's BuildID::isRailPlanner, bool
   uint32_t simpleBuildInputClick = 0;     // SimpleBuildInput::originalClickPosition, the cursor before snapping
   uint32_t simpleBuildInputDirection = 0; // SimpleBuildInput::direction, Direction
   uint32_t simpleBuildInputPosition = 0;  // SimpleBuildInput::position, Optional<MapPosition>
   uint32_t simpleBuildInputTile = 0;      // SimpleBuildInput's BuildID::placeAsTile, PlaceAsTile const*
   uint32_t entityPrototypeFlags = 0;      // EntityPrototype::flags, EntityPrototypeFlags (uint32)
   // TilePosition EntityPrototype::tileGridSize(Direction) const, a virtual slot: the tiles the
   // entity covers when built facing that direction, width and height swapped for east and west.
   uint32_t entityTileGridSize = 0;
   // The blueprint in hand. std::unique_ptr<ReadAdapter> GameAdapter::getCursorAdapter() const, a
   // virtual slot, reads the player's cursor; Blueprint const* ReadAdapter::getBuildableBlueprint()
   // const finds the blueprint there (an item's or a library record's). The ReadAdapter is deleted
   // through its virtual destructor.
   uint32_t adapterCursorAdapter = 0;
   uint32_t readAdapterDestructor = 0;
   uintptr_t buildableBlueprint = 0;
   // Where a blueprint goes. BuildingModifier Blueprint::getBuildingModifier(MapPosition, Direction,
   // Flip, Optional<MapPosition> const&) const centres the blueprint's tile box on the grid nearest
   // the cursor: its afterRotationShift is the centre. A blueprint of only off-grid entities is
   // centred on the cursor itself. TileBox Blueprint::getTileBoxIgnoreSnapGrid() const is that box,
   // before the blueprint is rotated. Blueprints with a snapping grid are placed by other rules.
   uintptr_t blueprintBuildingModifier = 0;
   uintptr_t blueprintTileBox = 0;
   uint32_t buildingModifierSize = 0;
   uint32_t buildingModifierCentre = 0;    // BuildingModifier::afterRotationShift, MapPosition
   uint32_t blueprintRotation = 0;         // Blueprint::rotation, Direction
   uint32_t blueprintFlip = 0;             // Blueprint::flip, Flip (one byte)
   uint32_t blueprintSnapToGrid = 0;       // Blueprint::snapToGrid's has-value flag, bool
   // What is in hand to build. BuildID Player::getBuildID() const: the entity an item places, or the
   // tile, or neither for a blueprint or anything else. The game turns an entity in hand with
   // GameView::buildDirection and, for entities that flip by mirroring, GameView::entityMirrored;
   // EntityPrototype::flipping says how the entity flips, if at all.
   uintptr_t playerBuildId = 0;
   uint32_t buildIdSize = 0;
   uint32_t buildIdEntity = 0;             // BuildID::entityID, ID<EntityPrototype> first
   uint32_t buildIdTile = 0;               // BuildID::placeAsTile, PlaceAsTile const*
   uint32_t buildIdRailPlanner = 0;        // BuildID::isRailPlanner, bool
   uint32_t gameViewEntityMirrored = 0;    // GameView::entityMirrored, bool
   uint32_t entityFlipping = 0;            // EntityPrototype::flipping, EntityFlipping (one byte)
   // The build preview's colour. void EntityToBeBuiltSettings::draw(DrawQueue&, Entity const*) const
   // draws one preview entity, the one in hand or one of a blueprint's (EntityToBeBuiltSettings::
   // blueprint set), tinted by BuildCheckResult GameAdapter::entityBuildabilityCheck(Entity const&,
   // BuildCheckData const&) const, a virtual slot, on BuildCheckData
   // EntityToBeBuiltSettings::getBuildCheckData() const. Buildable is green, or the out-of-reach
   // tint with EntityToBeBuiltSettings::tooFar; Ignorable has a tint of its own; anything else is
   // red. LocalisedString BuildCheckResult::getMessage() const is the game's reason, empty for those
   // three.
   uintptr_t settingsDraw = 0;
   uintptr_t settingsBuildCheckData = 0;
   uintptr_t buildCheckMessage = 0;
   uint32_t adapterBuildabilityCheck = 0;
   uint32_t buildCheckDataSize = 0;
   uint32_t buildCheckResultSize = 0;
   uint32_t buildCheckResultType = 0;      // BuildCheckResult::type, BuildCheckResult::Type (uint32)
   uint32_t buildCheckResultEntity = 0;    // BuildCheckResult::entity, Entity*: what is in the way
   uint32_t entityPosition = 0;            // Entity::position, MapPosition
   uint32_t entityPrototypeOf = 0;         // Entity::prototype, EntityPrototype*
   uint32_t settingsTooFar = 0;            // EntityToBeBuiltSettings::tooFar, bool
   uint32_t settingsBlueprint = 0;         // EntityToBeBuiltSettings::blueprint, Blueprint const*
   uint32_t settingsPlayer = 0;            // EntityToBeBuiltSettings::player, Player const*
   // What the preview highlights, drawn while the preview draws. Every highlight box is drawn by
   // void RenderUtil::renderCursorBox(CursorBoxType, BoundingBox, DrawQueue&, RenderLayer::Enum,
   // char, double, Color), or RenderUtil::renderDoubleCursorBox(CursorBoxType, BoundingBox const&,
   // BoundingBox const&, DrawQueue&, RenderLayer::Enum, Color) for an entity with a second box; the
   // box is the entity's selection box. The preview's trial build marks each entity it would change
   // through void DrawAdapter::renderCursorBox(Entity const&, NamedBool<SkipSurfaceCheckTag>,
   // CursorBoxType) const: DrawAdapter::destroy for one it would replace,
   // DrawAdapter::setDirectionAndMirroring for one it would turn. static void
   // ElectricEnergySource::drawPoleConnections(DrawQueue&, Surface const&, BoundingBox const&) boxes
   // the poles that would power an electric preview.
   uintptr_t renderCursorBox = 0;
   uintptr_t renderDoubleCursorBox = 0;
   uintptr_t adapterRenderCursorBox = 0;
   uintptr_t adapterDestroy = 0;
   uintptr_t adapterSetDirection = 0;
   uintptr_t drawPoleConnections = 0;
   // The poles a pole preview would wire to: EntityToBeBuiltSettings::wiresInPreview.addedWires,
   // std::vector<Wire>, each Wire two PointerWireEnd (source and target) of an Entity* and a
   // connector.
   uint32_t settingsAddedWires = 0;
   uint32_t wireSize = 0;
   uint32_t wireSource = 0;                // Wire::source.entity, Entity*
   uint32_t wireTarget = 0;                // Wire::target.entity, Entity*
   // A roboport preview adds a RoboportInfoDrawHelper to its DrawQueue; later in the frame void
   // RoboportInfoRenderer::postPrepare(std::vector<DrawHelper*> const&) draws, through
   // LogisticNetwork::drawCellConnections, a line from it to each roboport it would link to with
   // void RenderUtil::drawOnTilesBetween(DrawQueue&, Sprite const&, MapPosition const& from,
   // MapPosition const& to, RealOrientation const&, RenderLayer::Enum, Color), between the two
   // roboports' positions.
   uintptr_t roboportPostPrepare = 0;
   uintptr_t drawOnTilesBetween = 0;
   // Naming what a box is on. BoundingBox Entity::getSelectionBox(SelectionContext const&) const
   // calls the entity's own getSelectionBox(), and ignores the context. Entities on a surface are
   // walked with HeuristicEntityIterator<Surface const>, over AdvancedTilePositions (two tiles a
   // side, a MapPosition >> 9) from leftTop to rightBottom inclusive: startAdvancedTile() opens
   // the first, moveUntilEntityFound() sets currentEntity to each entity in turn whose position
   // lies in the area, once, then to null.
   uintptr_t entitySelectionBox = 0;
   uintptr_t iteratorStartTile = 0;
   uintptr_t iteratorMove = 0;
   uint32_t iteratorSize = 0;
   uint32_t iteratorSurface = 0;           // Surface const*
   uint32_t iteratorLeftTop = 0;           // AdvancedTilePosition, two ints
   uint32_t iteratorRightBottom = 0;
   uint32_t iteratorCurrentTile = 0;
   uint32_t iteratorCurrentEntity = 0;     // Entity*
   uint32_t entitySurface = 0;             // Entity::surface, Surface*
   // ElectricPolePrototype const* EntityPrototype::asElectricPole() const, a virtual slot: null but
   // for poles.
   uint32_t entityPrototypeAsPole = 0;
   // A logistic container's preview draws the network it would join: LogisticNetwork*
   // LogisticManager::findMatchingNetworkByPosition(MapPosition const&), the network whose logistic
   // area holds the position, or null; then it highlights that network's roboports.
   uintptr_t findMatchingNetwork = 0;
   uint32_t logisticNetworkId = 0;         // LogisticNetwork::networkID, uint32
   uint32_t logisticNetworkName = 0;       // LogisticNetwork::networkName.value, std::string, empty unless named
   // void LuaHelper::initLuaState(lua_State*): sets up the globals of every Lua state the game
   // creates (log, localised_print, ...).
   uintptr_t initLuaState = 0;
   // Flying text. Every text over the map lands in void Map::addLocalFlyingText(LocalMapFlyingText&&);
   // every text over the GUI is built in place by the allocator's
   // construct<agui::GuiFlyingText, Point&, std::string const&, Color&, Font const*&, int&, int&>.
   uintptr_t addLocalFlyingText = 0;
   uintptr_t constructGuiFlyingText = 0;
   uint32_t localMapFlyingTextText = 0; // LocalMapFlyingText::text (agui::FlyingText), std::string
   // Console lines: void OutputConsole::add(std::string const& playerName, Color, LocalisedString
   // const& body, Player const*, PrintSettings const&, std::vector<SavedSpecialItemReference>&&).
   uintptr_t outputConsoleAdd = 0;
   uint32_t outputConsoleOwner = 0;         // OutputConsole::owner, the Player* whose console it is
   uint32_t outputConsoleItems = 0;         // OutputConsole::items, std::list<Item>, newest first
   uint32_t outputConsoleItemsNotSaved = 0; // OutputConsole::itemsNotPartOfGameState, the same
   // Pop-ups. TipsAndTricksNotificationButton::TipsAndTricksNotificationButton(TipsAndTricksItem
   // const&, GuiContext) builds the "New tip" button; SpeechBubbleGui::SpeechBubbleGui(SpeechBubble&,
   // std::string const& text, GameView&, agui::FlowStyle const*, SpeechBubbleStyle*) shows a speech
   // bubble entity's text over the map; void InfoBoxManager::update() lays out the saving and
   // multiplayer boxes once a frame, after their connectors were added or removed.
   uintptr_t tipNotificationButton = 0;
   uintptr_t speechBubbleGui = 0;
   uintptr_t infoBoxManagerUpdate = 0;
   uint32_t infoBoxManagerFrame = 0;      // InfoBoxManager::frame, an embedded agui::Window holding the boxes
   uint32_t infoBoxManagerRebuild = 0;    // InfoBoxManager::rebuildConnectors, bool
   uint32_t infoBoxManagerConnectors = 0; // InfoBoxManager::connectors, std::vector<ConnectorAndPosition>
   // Walking. virtual bool Character::changePosition(Vector const&) is a character's step of the
   // tick in the game state, true only when it went the whole way.
   uintptr_t characterChangePosition = 0;
   uint32_t characterPosition = 0;   // Entity::position, MapPosition
   uint32_t characterMap = 0;        // Entity::map, Map*
   uint32_t characterController = 0; // Character::controller, CharacterController*
   uint32_t controllerPlayer = 0;    // Controller::player, Player*
   uint32_t mapUpdateTick = 0;       // Map::updateTick, uint64
   uint32_t playerMap = 0;           // Player::map, Map*
   // Label colours: a Label's style.parent is the GuiStyle style the game gave it, e.g. the bold red
   // and bold orange of a recipe tooltip's short ingredient counts.
   uint32_t labelStyleParent = 0;        // agui::Label::style.parent, agui::Style*
   uint32_t globalStyle = 0;             // GlobalContext::style.value, GuiStyle*
   uint32_t guiStyleBoldRedLabel = 0;    // GuiStyle::_boldRedLabel, IntegratedStyle<LabelStyleSpecification>*
   uint32_t guiStyleBoldOrangeLabel = 0; // GuiStyle::_boldOrangeLabel, the same
   uint32_t integratedLabelStyleAgui = 0; // IntegratedStyle<LabelStyleSpecification>::agui, agui::LabelStyle

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
   // void lua_pushnumber<unsigned char>(lua_State*, unsigned char): the game pushes numbers through
   // templates like this one; there is no plain lua_pushnumber to call.
   uintptr_t luaPushByte = 0;
   uintptr_t luaPushInt = 0;     // void lua_pushnumber<int>(lua_State*, int)
   uintptr_t luaPushBoolean = 0; // void lua_pushboolean(lua_State*, int)
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
   uint32_t globalControlSettings = 0;   // ControlSettings*
   // InputState::mouseState.buttons: the held mouse buttons, bit n-1 for the game's button n (left
   // 1, right 2, middle 3).
   uint32_t inputStateMouseButtons = 0;
   // InputState::mouseBlocks, one InputState::MouseBlock per button in the same order: the control a
   // press was used by, which blocks every other control on the button until the real release.
   uint32_t inputStateMouseBlocks = 0;
   uint32_t mouseBlockSize = 0;

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
   // GameView::buildDirection, a Direction (one byte; kDirectionCount means none): what building and
   // the rotate keys use for the item in hand.
   uint32_t gameViewBuildDirection = 0;
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
   // A chest's slot limit: the slots from `bar` on take nothing from machines. Its window part
   // (InventoryWithBarGui, an InventoryGui) sets it with the red X button after the slots.
   uint32_t inventoryBar = 0;         // Inventory::bar, the first locked slot
   uint32_t inventoryGuiInventory = 0; // InventoryGui::inventory, Inventory&
   uint32_t barGuiButton = 0;         // InventoryWithBarGui::setBarSlot, IconButton
   uint32_t barGuiMode = 0;           // InventoryWithBarGui::mode: NotSet 0, Setting 1 (choosing a slot), Set 2
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
   uintptr_t entityPrototypes = 0;
   uintptr_t qualityPrototypes = 0;
   uintptr_t recipePrototypes = 0;
   // PrototypeList<T>::nameToPrototype, the std::map<std::string, T*, std::less<>> of a type's
   // prototypes by internal name, for each of kNamedPrototypes.
   uintptr_t prototypeNames[kNamedPrototypeCount] = {};

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
   // The windows with circuit and logistic network buttons in the title bar (GuiWithSideButtons):
   // a button opens its panel beside the window, inside the side panel container.
   uint32_t sidePanelContainer = 0;   // GuiWithSideButtons::sidePanelContainer, agui::VerticalFlow
   // The small window of a transport belt, a lamp, an accumulator and the like
   // (GenericOnOffEntityGui): the window titled with the entity's name.
   uint32_t onOffEntityWindow = 0;    // GenericOnOffEntityGui::entityWindow, agui::Window
   // A pipe's or a storage tank's window: the fluid part, and in it the fluid's icon and the bar
   // of how full the entity is, both beside a label that says the same.
   uint32_t singleFluidBoxGui = 0;    // SingleFluidBoxEntityGui::fluidBoxGui, FluidBoxGui
   uint32_t fluidBoxIcon = 0;         // FluidBoxGui::fluidIcon, SimpleSlot
   uint32_t fluidBoxBar = 0;          // FluidBoxGui::fluidPercentageBar, agui::ProgressBar
   // The electric network window a pole opens (ElectricNetworkGuiWindow<ElectricPole>), and the
   // surface's like it (<Surface>), the same template over another object: the bars of how well the
   // network is supplied, and the columns of what consumes, produces and stores its energy, each a
   // FlowDataFrame with a graph above its table.
   uint32_t electricNetworkBars = 0;        // ::satisfactionFlow, agui::HorizontalFlow
   uint32_t electricNetworkFlows = 0;        // ::gui, FlowGui, agui::HorizontalFlow
   uint32_t electricNetworkConsumption = 0;  // ::gui.inputFrame, FlowDataFrame
   uint32_t electricNetworkProduction = 0;   // ::gui.outputFrame, FlowDataFrame
   uint32_t electricNetworkStorage = 0;      // ::gui.storageFrame, FlowDataFrame
   uint32_t flowFrameGraph = 0;              // FlowDataFrame::graph, Graph
   // A game window a mod attached relative GUI elements to (LuaGuiElement anchor) is taken off the
   // root into an invisible CustomGuiGameGuiWrapper: a 3 by 3 table with the window in the middle
   // and a flow on each side holding the mod's elements for that side.
   uint32_t relativeWrapperTable = 0;        // CustomGuiGameGuiWrapper::table, agui::Table
   uint32_t relativeWrapperTop = 0;          // ::topFlow, agui::HorizontalFlow
   uint32_t relativeWrapperLeft = 0;         // ::leftFlow, agui::VerticalFlow
   uint32_t relativeWrapperRight = 0;        // ::rightFlow, agui::VerticalFlow
   uint32_t relativeWrapperBottom = 0;       // ::bottomFlow, agui::HorizontalFlow

   // The quickbar along the bottom of the screen (QuickBarGui), reached the way the game's own
   // quickbar keys reach it: GameView::controllerView->getQuickBar().
   uint32_t gameViewControllerView = 0;  // GameView::controllerView, std::unique_ptr<ControllerView>
   uint32_t controllerViewQuickBar = 0;  // QuickBarGui* ControllerView::getQuickBar(), a vtable slot
   uint32_t quickBarMainRows = 0;        // QuickBarGui::mainWindowRows, std::vector<std::unique_ptr<RowWidgets>>
   uint32_t quickBarPickerRows = 0;      // QuickBarGui::pageSelectorRows, the same for the page picker
   uint32_t quickBarPicker = 0;          // QuickBarGui::pageSelectorFrame, an embedded agui::Frame
   // QuickBarGui::selectingNewPageForRow, std::optional<unsigned char>: the bar whose page the picker
   // is choosing while it is open.
   uint32_t quickBarPickingFor = 0;
   uint32_t rowPage = 0;                 // QuickBarGui::RowWidgets::pageIndex, unsigned char
   uint32_t rowButton = 0;               // QuickBarGui::RowWidgets::button, std::unique_ptr<agui::Button>
   uint32_t rowSlots = 0;                // QuickBarGui::RowWidgets::slots, std::vector<std::unique_ptr<ChooseButton>>

   // The shortcut bar beside the quickbar (ShortcutBarGui), reached as the quickbar is. Its buttons
   // stand in columns, each an agui::VerticalFlow; a button with no behavior is an empty place.
   uint32_t controllerViewShortcutBar = 0; // ShortcutBarGui* ControllerView::getShortcutBar(), a vtable slot
   uint32_t shortcutBarColumns = 0;      // ShortcutBarGui::columns, std::vector<std::unique_ptr<agui::VerticalFlow>>
   // The toggle button that opens and closes the list of every shortcut, where the player chooses
   // those on the bar; the list's frame, whether it is open, and its rows.
   uint32_t shortcutBarListButton = 0;   // ShortcutBarGui::expandButton, IconButton
   uint32_t shortcutBarList = 0;         // ShortcutBarGui::shortcutSelectionFrame, agui::Frame
   uint32_t shortcutBarListOpen = 0;     // ShortcutBarGui::shortcutSelectionFrameVisible, bool
   uint32_t shortcutBarListRows = 0;     // ShortcutBarGui::shortcutRows, std::vector<std::unique_ptr<ShortcutRow>>
   uint32_t shortcutRowCheckBox = 0;     // ShortcutBarGui::ShortcutRow::dockCheckbox: checked while on the bar
   uint32_t shortcutButtonBehavior = 0;  // ShortcutButton::behavior, ShortcutBehavior*
   uint32_t shortcutBehaviorPrototype = 0; // ShortcutBehavior::prototype, ShortcutPrototype* (a PrototypeBase)
   uint32_t buttonIsToggle = 0;          // agui::Button::isButtonToggleButton

   // The side menu at the top right (SideMenu): a table of buttons that open the game's windows
   // (production statistics, trains, Factoriopedia, ...), and the master mute button, which acts in
   // place.
   uint32_t gameViewSideMenu = 0;        // GameView::sideMenu, std::unique_ptr<SideMenu>
   uint32_t sideMenuMuteButton = 0;      // SideMenu::masterMutedButton, IconButton*

   // The crafting queue at the bottom left (CraftingQueueGui, an agui::Flow), reached as the
   // quickbar is; CharacterView and GodView have one, the remote view none. Its slots are rebuilt on
   // every change to the queue (CraftingQueueGui::update).
   uint32_t controllerViewCraftingQueue = 0; // CraftingQueueGui* ControllerView::getCraftingQueue(), a vtable slot
   uint32_t craftingQueueSlots = 0;      // CraftingQueueGui::slots, std::vector<std::unique_ptr<agui::Widget>>
   // The same queue in the character window's left pane, under its label.
   uint32_t characterInfoQueueLabel = 0; // CharacterInfoGui::craftingQueueLabel, agui::Label
   uint32_t characterInfoQueue = 0;      // CharacterInfoGui::craftingQueueGui, CraftingQueueTableGui (an agui::Table)

   // The HUD's status: the research box at the top right, the alert buttons, the scenario's goal and
   // the bars over the quickbar.
   uint32_t gameViewResearch = 0;        // GameView::currentResearchInfo, std::unique_ptr<CurrentResearchInfo>
   uint32_t researchTitle = 0;           // CurrentResearchInfo::title, agui::Label: the technology, or "not researching"
   uint32_t researchProgressFlow = 0;    // CurrentResearchInfo::progressBarFlow, hidden while nothing is researched
   uint32_t researchProgressLabel = 0;   // CurrentResearchInfo::researchProgressLabel, the formatted percent
   // One AlertGui per AlertCategory, each shown while its category has alerts; its button opens the
   // category's AlertsOverview.
   uint32_t gameViewAlerts = 0;          // GameView::alertGuis, std::vector<std::unique_ptr<AlertGui>>
   uint32_t alertGuiCategory = 0;        // AlertGui::category, AlertCategory (unsigned char)
   uint32_t alertGuiButton = 0;          // AlertGui::warningSlot, IconButtonWithNumber
   // The alerts window an alert button opens, one category's alerts. It stacks over whatever window
   // is open; E or Escape closes it.
   uint32_t gameViewAlertsOverview = 0;  // GameView::alertsOverview, std::unique_ptr<AlertsOverview>
   uint32_t alertsOverviewCategory = 0;  // AlertsOverview::category, AlertCategory
   // AlertsOverview::alertGroupsList, agui::ListBox: per surface (headed by its name when there are
   // several) a row per group of alerts. A click opens remote view on the group and closes the window.
   uint32_t alertsOverviewList = 0;
   // AlertsOverview::pinButtonFlow, agui::VerticalFlow: beside each row of the list, its pin button
   // (an IconButton, InputAction PinAlertGroup), or for a surface's heading a blank TextButton.
   uint32_t alertsOverviewPins = 0;
   // IconButtonWithNumber::count. The alert button blinks by setting it to 0 every other half second.
   uint32_t iconButtonCount = 0;
   uint32_t gameViewGoal = 0;            // GameView::goalDescription, std::unique_ptr<GoalDescription>
   uint32_t goalLabel = 0;               // GoalDescription::label, agui::Label
   uint32_t gameViewBottom = 0;          // GameView::bottomContainer, std::unique_ptr<BottomContainer>
   // BottomContainer's bars, each a GenericTargeter<ControllerProgressBar>.
   uint32_t bottomHealthBar = 0;
   uint32_t bottomShieldBar = 0;
   uint32_t bottomVehicleHealthBar = 0;
   uint32_t bottomVehicleShieldBar = 0;
   uint32_t bottomMiningBar = 0;

   // Factoriopedia, which stacks over whatever window is open: the entries list on the left, the
   // chosen entry's page on the right.
   uint32_t gameViewFactoriopedia = 0;   // GameView::factoriopedia, std::unique_ptr<Factoriopedia>
   uint32_t factoriopediaList = 0;       // Factoriopedia::selectList, SelectListGui<FactoriopediaID>
   uint32_t factoriopediaSubheader = 0;  // Factoriopedia::insideFrame.subheader: the entry's title label
   uint32_t factoriopediaPage = 0;       // Factoriopedia::scrollPane, the entry's description and sections
   uint32_t factoriopediaUnresearched = 0; // Factoriopedia::showUnresearchedButton, a toggle IconButton
   uint32_t factoriopediaPinned = 0;     // Factoriopedia::pinned: kept open, without modal focus

   // Rich text icons that a label makes hoverable (LabelWithHoverableRichText, as in descriptions):
   // hovering one shows its tooltip, clicking it opens its Factoriopedia entry or technology. The
   // label lays its text out in sections, an icon each and the plain runs between them.
   uint32_t labelRichText = 0;           // agui::Label::resizableText.richTextData, std::unique_ptr to TextDrawSections
   uint32_t richTextSectionsBegin = 0;   // TextDrawSections::sections.begin_, TextDrawSection*
   uint32_t richTextSectionsEnd = 0;     // TextDrawSections::sections.end_
   uint32_t richTextSectionSize = 0;     // sizeof(TextDrawSection)
   uint32_t richTextSectionType = 0;     // TextDrawSection::type, TagType
   uint32_t richTextSectionTag = 0;      // TextDrawSection::tagText, std::string_view: "item=iron-plate"
   uint32_t hoverableLabelManager = 0;   // LabelWithHoverableRichText::hoverManger, LabelRichTextHoverManager
   uint32_t hoverManagerTooltip = 0;     // RichTextHoverManager::hoverTooltip, GenericTargeter<agui::ToolTip>
   // void RichTextHoverManager::handleHover(TextDrawSection const&, OutputConsole::Item const*, bool
   // clicked): what the label runs for the section under the mouse, on a move and on a click.
   uintptr_t richTextHandleHover = 0;
   uintptr_t richTextClearTooltip = 0;   // void RichTextHoverManager::clearTooltip()

   // The technology window (T), which also stacks over whatever window is open: the research queue,
   // the selected technology and the list of every technology on the left, the selected
   // technology's graph of prerequisites and unlocks on the right.
   uint32_t gameViewTechnology = 0;      // GameView::technologyGui, TechnologyGui*
   uint32_t technologyQueue = 0;         // TechnologyGui::researchQueueGui, ResearchQueueGui
   uint32_t technologyTitle = 0;         // TechnologyGui::featuredTechnologyTitle, agui::Label
   uint32_t technologyStatus = 0;        // TechnologyGui::featuredTechnologyStatus, agui::Label: "(Available)"
   uint32_t technologyFeatured = 0;      // TechnologyGui::featuredTechnologyGui, FeaturedTechnologyGui
   uint32_t technologyList = 0;          // TechnologyGui::technologiesGui, TechnologyListGui
   uint32_t technologyGraphTitle = 0;    // TechnologyGui::technologyGraphTitleFrame: history arrows, close
   uint32_t technologyGraphHolder = 0;   // TechnologyGui::technologyGraphHolder: "show only essential"
   uint32_t technologyGraph = 0;         // TechnologyGui::technologyGraph, TechnologyGraphGui
   uint32_t technologyListTable = 0;     // TechnologyListGui::table, the grid of TechnologySlot
   // ResearchQueueGui::queueTable, a table of TechnologyQueueElement, filled up to seven places with
   // empty ones.
   uint32_t queueTable = 0;
   uint32_t queueElementSlot = 0;        // TechnologyQueueElement::technologySlot, TechnologySlot
   uint32_t queueElementCancel = 0;      // TechnologyQueueElement::cancelButton, IconButton
   // A technology's button, in the list, the queue and the graph.
   uint32_t techSlotTechnology = 0;      // TechnologySlot::technology, TechnologyReference
   uint32_t techSlotResearchQueue = 0;   // TechnologySlot::researchQueue, ResearchQueue* (may be null)
   uint32_t techSlotResearchManager = 0; // TechnologySlot::researchManager, ResearchManager* (may be null)
   uint32_t techSlotIndicateProgress = 0; // TechnologySlot::indicateProgress: 0 none, else a bar is drawn
   uint32_t techReferenceId = 0;         // TechnologyReference::technologyID, ID<TechnologyPrototype,u16>
   uint32_t technologyPrototype = 0;     // Technology::prototype
   uint32_t researchQueueMap = 0;        // ResearchQueue::queue, std::deque<ID<TechnologyPrototype,u16>>: _Map
   uint32_t researchQueueMapSize = 0;    // ... _Mapsize
   uint32_t researchQueueOffset = 0;     // ... _Myoff
   uint32_t researchQueueSize = 0;       // ... _Mysize
   uintptr_t getTechnology = 0;          // Technology const& TechnologyReference::getTechnology() const
   uintptr_t technologyState = 0;        // ResearchState Technology::getState(ResearchQueue const*) const
   uintptr_t techSlotLevel = 0;          // unsigned TechnologySlot::getLevel() const: the level band's number
   // LocalisedString TechnologyPrototype::getLocalisedNameWithLevel(unsigned) const, as the window
   // titles the selected technology: "Steel axe", "Mining productivity 3".
   uintptr_t technologyNameWithLevel = 0;
   uintptr_t researchProgress = 0;       // double ResearchManager::getProgress(Technology const&) const
   uintptr_t localisedStringFromKey = 0; // LocalisedString::LocalisedString(char const* key)
   // The graph, laid out in layers top to bottom, prerequisites above what they unlock. An edge that
   // spans layers runs through dummy vertices, one per layer it crosses.
   uint32_t graphVertices = 0;           // TechnologyGraphGui::graph, std::vector<std::unique_ptr<Vertex>>
   uint32_t graphCentral = 0;            // TechnologyGraphGui::central, Vertex*: the selected technology
   uint32_t vertexTechnology = 0;        // TechnologyGraphGui::Vertex::technology, TechnologyReference
   uint32_t vertexSlot = 0;              // ::slot, TechnologyGraphVertex*, a base of the button drawn
   uint32_t vertexSuccessors = 0;        // ::successors, std::vector<Vertex*>: one layer down
   uint32_t vertexPredecessors = 0;      // ::predecessors, std::vector<Vertex*>: one layer up
   uint32_t vertexLayer = 0;             // ::layer, unsigned
   uint32_t vertexType = 0;              // ::type, Vertex::Type
   uint32_t vertexNumOmitted = 0;        // ::numOmitted, unsigned
   uint32_t vertexX = 0;                 // ::position.x, int: left to right within the layer

   // The info panel the game shows for what the player points at, built every frame by
   // SelectedInfoRenderer::update from GameView::update: beside the mouse after a delay, or with the
   // interface setting "entity tooltip on the side" at the right of the screen. It is a window
   // (SelectedInfo<Entity const*,EntityButton> for an entity, SelectedInfo<Tile,ObjectButton<...>>
   // for a tile) that the entity's own addToDescription fills through a Description.
   // SelectedInfo(std::optional<GuiContext>, Entity const* const&, bool onTheSide): empty until updated.
   uintptr_t entityInfoConstruct = 0;
   uintptr_t entityInfoUpdate = 0;       // void update(Entity const* const&, bool): fills it anew when changed
   uintptr_t entityInfoDestroy = 0;      // its scalar deleting destructor
   uint32_t entityInfoSize = 0;
   // SelectedInfo(std::optional<GuiContext>, Tile const&, bool onTheSide)
   uintptr_t tileInfoConstruct = 0;
   uintptr_t tileInfoChange = 0;         // void change(Tile const&, bool)
   uintptr_t tileInfoDestroy = 0;
   uint32_t tileInfoSize = 0;
   uint32_t globalInterfaceSettings = 0; // GlobalContext::interfaceSettings.value
   uint32_t tooltipOnTheSide = 0;        // InterfaceSettings::entityToolTipOnTheSide.value, bool
   // What the player points at, as GameView::update finds it: the entity of the selector the
   // latency adapter (or, without latency hiding, the player's own adapter) gives, else the tile the
   // controller deduces at the cursor.
   uint32_t playerLatencyAdapter = 0;    // Player::latencyStateAdapter, LatencyStateAdapter* (may be null)
   uint32_t playerGameStateAdapter = 0;  // Player::gameStateAdapter, by value
   uint32_t adapterEntitySelector = 0;   // slot of EntitySelector* GameAdapter::getEntitySelector() const
   uint32_t selectorEntity = 0;          // EntitySelector::selectedEntity.target, the Entity*
   uint32_t playerController = 0;        // Player::controllerManager.controller, Controller*
   uint32_t controllerSelectedTile = 0;  // slot of Tile const* Controller::deduceSelectedTile(MapPosition const&) const
   uint32_t gameViewActiveWindow = 0;    // GameView::activeWindow, std::unique_ptr<GameGui>: inventory, an entity's

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

   // The full map (see chart.h).
   uint32_t playerRenderMode = 0;     // Player::renderMode, GameRenderMode
   // ChartSelection PlayerInputSource::getChartSelection() const: what the map selects at the
   // cursor, nothing off the map.
   uintptr_t chartSelection = 0;
   uint32_t chartSelectionSize = 0;
   uint32_t chartSelectionTarget = 0; // ChartSelection::target, EntityWithOwner*: a vehicle or display panel
   uint32_t chartSelectionTag = 0;    // ChartSelection::customTagTarget, CustomChartTag*
   uint32_t chartSelectionPatch = 0;  // ChartSelection::resourcePatch, ResourceEntity*: one resource of the patch
   uint32_t chartTagText = 0;         // CustomChartTag::text, std::string
   // ResourcePatchInfo, which finds a whole resource patch from one resource as the map does to
   // outline and label it.
   uint32_t patchInfoSize = 0;
   uintptr_t patchInfoConstruct = 0; // ResourcePatchInfo::ResourcePatchInfo(bool useClockLimiter)
   uintptr_t patchInfoDestroy = 0;   // ResourcePatchInfo::~ResourcePatchInfo()
   // bool ResourcePatchInfo::update(ResourceEntity const*, ForceData const&, bool keepIfUnchanged)
   uintptr_t patchInfoUpdate = 0;
   // static std::string ResourcePatchInfo::getFormattedNameFor(MaterialID const&, double amount,
   // ResourceEntityPrototype const*): a line of the map's label, "[item=iron-ore] 1.2M".
   uintptr_t patchFormattedName = 0;
   uint32_t patchInfoCounts = 0;    // ResourcePatchInfo::expectedMiningAmount.counts, std::map<MaterialID, double>
   uint32_t patchInfoPrototype = 0; // ResourcePatchInfo::resourcePrototype
   uint32_t materialIdSize = 0;     // sizeof(MaterialID)
   uint32_t playerForce = 0;        // Player::forceID.index, uint8_t
   // Map::forceManager.sortedForceDataList.begin_, ForceData**, indexed by ForceID.
   uint32_t mapForceData = 0;
   uintptr_t gameOperatorDelete = 0; // the game's operator delete(void*), which frees what its strings hold
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

// Direction holds 16 directions, north 0 clockwise, as defines.direction does; 16 is none (the game
// tests for it before handing the direction to Lua as CustomInputEvent::cursor_direction).
inline constexpr uint8_t kDirectionCount = 16;

// MapPosition coordinates are fixed point with 8 fractional bits.
inline constexpr int32_t kMapPositionScale = 256;

// Bits of EntityPrototypeFlags, read from the game's entityPrototypeFlagMapping table in 2.1.21:
// code constants, like the widget bits above.
inline constexpr uint32_t kEntityNotRotatable = 0x1;              // "not-rotatable"
inline constexpr uint32_t kEntityPlaceableOffGrid = 0x10;         // "placeable-off-grid"
inline constexpr uint32_t kEntitySnapToRailSupportSpot = 0x10000000; // "snap-to-rail-support-spot"

// EntityFlipping, code constants: NotAvailable 0, Simple 1, DirectionTransform 2,
// DirectionAndMirroring 3. Only the last uses GameView::entityMirrored.
inline constexpr uint8_t kEntityFlippingNotAvailable = 0;
inline constexpr uint8_t kEntityFlippingMirroring = 3;
// Flip, a one-byte bitfield: horizontal, then vertical, in the blueprint's own unrotated frame.
inline constexpr uint8_t kFlipHorizontal = 0x1;
inline constexpr uint8_t kFlipVertical = 0x2;

// BuildCheckResult::Type, code constants.
inline constexpr uint32_t kBuildCheckBuildable = 0;
inline constexpr uint32_t kBuildCheckIgnorable = 2;
inline constexpr uint32_t kBuildCheckCollidesWithEntity = 3;

// RenderUtil::CursorBoxType, code constants: the kinds of highlight box.
inline constexpr uint8_t kCursorBoxEntity = 0;
inline constexpr uint8_t kCursorBoxElectricity = 1;
inline constexpr uint8_t kCursorBoxCopy = 2;
inline constexpr uint8_t kCursorBoxNotAllowed = 3;
inline constexpr uint8_t kCursorBoxPair = 4;
inline constexpr uint8_t kCursorBoxLogistics = 5;
// Also what Entity::drawPotentialInteractionIndications draws on the inserters, drills and
// machines that would put into or take from an entity.
inline constexpr uint8_t kCursorBoxTrainVisualization = 6;

// GameRenderMode, code constants: nothing 0, game 1, chart 2 (the full map), chart zoomed in 3
// (remote view drawing the world).
inline constexpr uint8_t kRenderModeChart = 2;

// Stack room the building hooks keep for a SimpleBuildInput, a BuildingModifier, a BuildID, a
// BuildCheckData and a BuildCheckResult; resolve fails when the game's outgrow them.
inline constexpr uint32_t kSimpleBuildInputCapacity = 128;
inline constexpr uint32_t kBuildingModifierCapacity = 64;
inline constexpr uint32_t kBuildIdCapacity = 64;
inline constexpr uint32_t kBuildCheckDataCapacity = 192;
inline constexpr uint32_t kBuildCheckResultCapacity = 96;
// Room for a HeuristicEntityIterator<Surface const>.
inline constexpr uint32_t kEntityIteratorCapacity = 64;
// Room for a ChartSelection, and for the ResourcePatchInfo the chart keeps (see chart.h).
inline constexpr uint32_t kChartSelectionCapacity = 64;
inline constexpr uint32_t kPatchInfoCapacity = 256;

extern Layout layout;

// Fills `layout`; logs every missing name and returns false if anything could not be resolved.
bool resolve(pdb::SymbolTable& symbols);

} // namespace fa::game
