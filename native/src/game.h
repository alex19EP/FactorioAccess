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
   // Selection tools (planners, copy and cut, a blueprint's area). A press of a select control
   // reaches bool PlayerInputSource::processSelectionToolCommon(SelectionMode, SelectionMode), which
   // starts a selection at the cursor (GameView::startSelection). Every frame
   // PlayerInputSource::sendSelectionChanges asks SelectionMode
   // PlayerInputSource::expectedSelectionModeFromInputs(bool) const which select control is held;
   // with the mouse none held finishes the selection, with a gamepad it stays open until the next
   // select press, which processSelectionToolCommon answers with void
   // PlayerInputSource::finishSelection(): the selection's action, sent like any other input
   // action. bool PlayerInputSource::processActions(Event const&, bool paused) runs the game's
   // controls for one input event in a fixed order; ControlInputValue const*
   // ControlInput::triggeredBy(Event const&, ControlContext const*, unsigned) const tells whether
   // the event triggers a control.
   uintptr_t processSelectionToolCommon = 0;
   uintptr_t expectedSelectionMode = 0;
   uintptr_t finishSelection = 0;
   uintptr_t processActions = 0;
   uintptr_t controlTriggeredBy = 0;
   uint32_t controlSettingsToggleMenu = 0; // ControlSettings::toggleMenu, Escape
   // The open selection, on GameView: SelectionMode (Nothing is 0) at the start and now, the
   // surface it started on (SurfaceIndex), its start corner (Optional<MapPosition>) and start time
   // (std::chrono::steady_clock::time_point).
   uint32_t gameViewStartSelectionMode = 0;
   uint32_t gameViewSelectionMode = 0;
   uint32_t gameViewSelectionSurface = 0;
   uint32_t gameViewSelectionPosition = 0;
   uint32_t gameViewSelectionStartTime = 0;
   // What the open selection takes in. Every frame CursorRenderer::renderSelection (the world) and
   // SelectionToolChartRenderer::prepare (the map, but not for a deconstruction planner) build a
   // SelectionToolRenderer for the box between the start corner and the cursor, count what the
   // selection would act on, and draw the counts with void SelectionToolRenderer::
   // drawSelectionCounts(DrawQueue&, Color const&): the items a copy or blueprint would build or the
   // upgrades an upgrade planner would make. A deconstruction planner draws its counts while
   // selecting (RenderUtil::drawDeconstructionCounts: the entities, then the items they would give)
   // and returns from drawSelectionCounts at once.
   uintptr_t drawSelectionCounts = 0;
   uint32_t selectionRendererCursor = 0;        // SelectionToolRenderer::cursorPosition, MapPosition
   uint32_t selectionRendererStart = 0;         // SelectionToolRenderer::selectionStart, MapPosition
   uint32_t selectionRendererDeconstruction = 0; // SelectionToolRenderer::isDeconstructionPlanner, bool
   uint32_t selectionRendererCounts = 0;        // SelectionToolRenderer::selectionCounts, SelectionCounts
   // SelectionCounts: std::maps from IDWithQuality to a count (items, entities), or from a pair of
   // them, what is upgraded and what to, to a count.
   uint32_t countsItemsNotToBuild = 0; // itemCountsNotUsedToBuild
   uint32_t countsItemsToBuild = 0;    // itemCountsToBuild
   uint32_t countsEntities = 0;        // entityCounts
   uint32_t countsEntityUpgrades = 0;  // entityUpgradeCounts
   uint32_t countsItemUpgrades = 0;    // itemUpgradeCounts
   // The stored pair in those maps' nodes (std::_Tree_node): the ID or the pair of IDs, and the count.
   // Item and entity maps lay their nodes out alike, which game.cpp checks.
   uint32_t countNodeId = 0;
   uint32_t countNodeCount = 0;
   uint32_t upgradeNodeFrom = 0;
   uint32_t upgradeNodeTo = 0;
   uint32_t upgradeNodeCount = 0;
   // IDWithQuality: baseID (an index into the prototypes) and qualityID.
   uint32_t idWithQualityBase = 0;
   uint32_t idWithQualityQuality = 0;
   // MapPosition GameView::getMapPosition(PixelPosition) const: the map position at a pixel of the
   // game view. The game asks it for the mouse pixel (InputState::mouseState.x and .y) wherever it
   // wants the world under the mouse: the cursor position the renderers draw at, the selection box
   // and its counts, the sound a finished selection plays, the tile GameView::update tracks.
   uintptr_t gameViewMapPosition = 0;
   uint32_t inputStateMouseX = 0;
   uint32_t inputStateMouseY = 0;
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
   // The scanner lists a surface's entities chunk by chunk, with the iterator above over each
   // chunk's 16 by 16 advanced tiles, where the player's force has charted it.
   uint32_t mapSurfaces = 0;               // Map::surfaces, std::vector<Surface*>
   uint32_t surfaceIndex = 0;              // Surface::index, SurfaceIndex (uint32), LuaSurface::index
   uint32_t surfaceChunks = 0;             // Surface::chunks, std::vector<Chunk*>
   uint32_t chunkPosition = 0;             // Chunk::position, ChunkPosition (two ints)
   // Chunk const* Surface::getChunkSafe(ChunkPosition const&) const: the chunk there, or null.
   // A refresh spread over ticks finds its chunks again by this, as the game may delete chunks.
   uintptr_t surfaceChunkAt = 0;
   // Water and ice are tiles: Chunk::tiles is Tile[32][32] by x then y, each a tileID
   // (ID<TilePrototype>, uint16) and a variation byte. Tile const* Surface::getTileOptional(
   // TilePosition const&) const reads one, null where no chunk is.
   uint32_t chunkTiles = 0;                // Chunk::tiles
   uint32_t tileSize = 0;                  // sizeof(Tile)
   uintptr_t surfaceTileAt = 0;
   // Resource patches as the map finds them (ResourcePatchInfo::addPatch and scanPatch): a grid of
   // cells twice the prototype's resourcePatchSearchRadius a side (at most 32, shrunk until it
   // divides 32; 0 makes every resource a patch of its own), joined to their 8 neighbours while one
   // of the prototype's resources is in each, in charted chunks.
   uint32_t resourceSearchRadius = 0;      // ResourceEntityPrototype::resourcePatchSearchRadius, uint32
   uint32_t resourceInfinite = 0;          // ResourceEntityPrototype::infiniteType, bool
   uint32_t prototypeGetType = 0;          // virtual slot of char const* PrototypeBase::getType() const
   // Entries keep their entities through the game's own weak references, as its GUIs do: a
   // TargeterBase {vfptr, target, next, previous} linked first into the Targetable's (the entity's,
   // at its start) Targeters::firstTargeter. The game unlinks nothing itself; when the entity goes,
   // Targeters::clear calls each targeter's clearAsReactionToNotification, which nulls target.
   uintptr_t entityTargeterVtable = 0;     // Targeter<Entity,0,0>'s vftable: generic type, no flags
   uint32_t targetableTargeters = 0;       // Targetable::targetingMe.firstTargeter, TargeterBase*
   // Entity::usageBitMask (uint16). The scanner skips what LuaEntity's constructor refuses: 0x4,
   // the entity inside a ghost, which Entity::getOuterEntity trades for its ghost, and 0x10.
   uint32_t entityUsageBits = 0;
   // What sets an entity apart from others of its prototype in the scanner's subcategories, read
   // from the class each prototype type makes; each member is found through that class's bases.
   uint32_t scanAssemblerRecipe = 0;       // AssemblingMachine::recipeID, IDWithQuality<RecipeID>
   uint32_t scanFurnaceRecipe = 0;         // Furnace::recipeID
   uint32_t scanFurnaceResult = 0;         // Furnace::resultInventory, an Inventory
   // MiningDrill::resourcesToMine, std::vector<Targeter<ResourceEntity,33,0>>: what it mines
   uint32_t scanDrillResources = 0;
   uint32_t resourceTargeterSize = 0;      // sizeof(Targeter<ResourceEntity,33,0>)
   uint32_t scanLocomotiveTrain = 0;       // Locomotive::train, Train*
   uint32_t scanCargoWagonTrain = 0;       // CargoWagon::train
   uint32_t scanFluidWagonTrain = 0;       // FluidWagon::train
   uint32_t scanArtilleryWagonTrain = 0;   // ArtilleryWagon::train
   uint32_t trainId = 0;                   // Train::id, uint32: LuaTrain::id
   uint32_t scanGhostInner = 0;            // EntityGhost::innerEntity, Entity*
   uint32_t scanSpawnerPollution = 0;      // EnemySpawner::absorbedPollution, double
   uint32_t scanContainerInventory = 0;    // ContainerEntity::inventory, Inventory*
   uint32_t scanLogisticInventory = 0;     // LogisticContainer::inventory
   uint32_t scanInfinityInventory = 0;     // InfinityContainer::inventory
   uint32_t scanRoboportName = 0;          // Roboport::backerName, std::string
   uint32_t scanPipeFluidBox = 0;          // Pipe::fluidBox, a FluidBox
   uint32_t scanInfinityPipeFluidBox = 0;  // InfinityPipe::fluidBox
   uint32_t scanUndergroundFluidBox = 0;   // PipeToGround::fluidBox
   uint32_t scanTankFluidBox = 0;          // StorageTank::fluidBox
   // A fluid box's fluid is its FluidSegment's while it has one (Entity::getFluidAmount): the
   // fluid's ID (ID<FluidPrototype>, uint16, 0 for none) and amount (fixed point int64).
   uint32_t fluidBoxSegment = 0;           // FluidBox::fluidSegment, FluidSegment*
   uint32_t fluidBoxFluid = 0;             // FluidBox::buffer.fluid.fluidID
   uint32_t fluidBoxAmount = 0;            // FluidBox::buffer.fluid.amount
   uint32_t segmentFluid = 0;              // FluidSegment::buffer.fluid.fluidID
   uint32_t segmentAmount = 0;             // FluidSegment::buffer.fluid.amount
   // FluidBox::connections, SmallVector<FluidBoxConnection,4>: one per pipe connection, its target
   // (FluidBox*) null while nothing is connected there.
   uint32_t fluidBoxConnectionsBegin = 0;  // FluidBox::connections.begin_
   uint32_t fluidBoxConnectionsEnd = 0;    // FluidBox::connections.end_
   uint32_t fluidConnectionSize = 0;       // sizeof(FluidBoxConnection)
   uint32_t fluidConnectionTarget = 0;     // FluidBoxConnection::target
   // bool ForceData::isChunkCharted(SurfaceIndex, MapPosition const&) const
   uintptr_t forceIsChunkCharted = 0;
   // Fog of war: Chart const* ForceData::getChart(SurfaceIndex) const, the force's chart of a
   // surface or null, and bool Chart::isChunkCoveredByFogOfWar(ChunkPosition const&) const, true
   // where the chunk is uncharted or was last charted 600 or more ticks ago (never on a platform).
   // Both only read.
   uintptr_t forceChart = 0;
   uintptr_t chartChunkCovered = 0;
   // ForceID Entity::getForceID() const, a virtual slot: neutral for entities of no force. Returned
   // through a hidden pointer, as member functions return classes.
   uint32_t entityGetForceId = 0;
   uint32_t displayPanelShowInChart = 0; // DisplayPanel::showInChart, bool
   // ElectricPolePrototype const* EntityPrototype::asElectricPole() const, a virtual slot: null but
   // for poles.
   uint32_t entityPrototypeAsPole = 0;
   // A logistic container's preview draws the network it would join: LogisticNetwork*
   // LogisticManager::findMatchingNetworkByPosition(MapPosition const&), the network whose logistic
   // area holds the position, or null; then it highlights that network's roboports.
   uintptr_t findMatchingNetwork = 0;
   uint32_t logisticNetworkId = 0;         // LogisticNetwork::networkID, uint32
   uint32_t logisticNetworkName = 0;       // LogisticNetwork::networkName.value, std::string, empty unless named
   // What an entity shows on the map, read by drawing it again into a DrawQueue of our own with the
   // main view's render parameters: GameView::renderer (std::unique_ptr<GameRenderer>) keeps the
   // last frame's in GameRenderer::renderParameters, whose flags say whether alt mode
   // (ShowEntityInfo) and status icons (ShowStatusIcons) are on. DrawQueue::DrawQueue(RenderParameters
   // const&) and clear(); the entity draws itself through the virtual Entity::draw(DrawQueue&) const.
   uint32_t gameViewRenderer = 0;          // GameView::renderer, GameRenderer*
   uint32_t gameRendererParameters = 0;    // GameRenderer::renderParameters, RenderParameters
   uint32_t renderParametersFlags = 0;     // RenderParameters::flags, RenderParameters::Flags (uint32)
   uint32_t drawQueueSize = 0;
   uint32_t drawQueueRenderParameters = 0; // DrawQueue::renderParameters, RenderParameters const*
   uintptr_t drawQueueConstruct = 0;
   uintptr_t drawQueueClear = 0;
   uint32_t entityDraw = 0;                // virtual slot of Entity::draw
   // Status icons: void Entity::drawAlert(DrawQueue&, Sprite const&, MapPosition const&, bool
   // blinking) const draws each (no power, no fuel, no ammo ...), when ShowStatusIcons is on; the
   // overload without a position calls it. Sprites are UtilitySprites members, named by
   // UtilitySprites::spritesMapping (std::map<std::string, Sprite*, std::less<void>>), the
   // utility sprite names of the prototype ("electricity_icon").
   uintptr_t entityDrawAlert = 0;
   uint32_t globalUtilitySprites = 0;      // GlobalContext::utilitySprites.value, UtilitySprites*
   uint32_t utilitySpritesSize = 0;
   uint32_t utilitySpritesMapping = 0;
   // Alt mode icons: void DrawQueue::drawInfoIcon(Sprite const*, QualityCondition, MapPosition const&,
   // double scale, DrawingFlags, RenderLayer::Enum, Vector const&, signed char, Color) draws every
   // one (recipe, contents, filters, modules, fluid, ammo, signals ...), its Sprite owned by the
   // prototype it depicts (Sprite::owner). A pass with kDrawingFlagIconBackground draws only the
   // icon's dark backing. Then drawQualityPartOfInfoIcon draws the quality badge: none for no
   // quality (unless the flags say a filter or any quality), none for a quality that is not drawn
   // by default compared with Equals; otherwise the quality's icon, after Comparison::str() unless
   // Equals, or UtilitySprites::anyQuality without a quality.
   uintptr_t drawInfoIcon = 0;
   uint32_t qualityDrawByDefault = 0;      // QualityPrototype::drawSpriteByDefault, bool
   uintptr_t comparisonStr = 0;            // char const* Comparison::str() const
   // Entity* LuaHelper::getParamOrDefault<Entity*>(lua_State*, int index, char const* name, Entity*
   // default): the entity of a LuaEntity argument.
   uintptr_t luaParamEntity = 0;
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
   // void lua_pushnumber<unsigned char>(lua_State*, unsigned char): the game pushes its own numbers
   // through templates like this one. The plain lua_pushnumber is exported too.
   uintptr_t luaPushByte = 0;
   uintptr_t luaPushInt = 0;     // void lua_pushnumber<int>(lua_State*, int)
   uintptr_t luaPushNumber = 0;  // void lua_pushnumber(lua_State*, lua_Number)
   uintptr_t luaPushBoolean = 0; // void lua_pushboolean(lua_State*, int)
   // Reading a table argument (fa_native.audio).
   uintptr_t luaGetField = 0;   // void lua_getfield(lua_State*, int index, char const*)
   uintptr_t luaRawGetI = 0;    // void lua_rawgeti(lua_State*, int index, int n)
   uintptr_t luaType = 0;       // int lua_type(lua_State*, int index)
   uintptr_t luaToNumberX = 0;  // double lua_tonumberx(lua_State*, int index, int* isnum)
   uintptr_t luaToBoolean = 0;  // int lua_toboolean(lua_State*, int index)
   uintptr_t luaToLString = 0;  // char const* lua_tolstring(lua_State*, int index, size_t*)
   uintptr_t luaRawLen = 0;     // size_t lua_rawlen(lua_State*, int index)
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
   // GameView::player, the Player* it shows. Null for a moment while a save loads, when asking
   // PlayerInputSource for anything of the view aborts the game ("No game view").
   uint32_t gameViewPlayer = 0;
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
   // LabeledSwitch, a HorizontalFlow: the switch between a label for each side ("Whitelist",
   // "Blacklist").
   uint32_t labeledSwitchSwitch = 0; // switchWidget, agui::Switch
   uint32_t labeledSwitchLeft = 0;   // leftValueLabel, agui::Label
   uint32_t labeledSwitchRight = 0;  // rightValueLabel, agui::Label
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
   // What robots are to bring to an entity's slot or take from it (its insert and removal plans),
   // as InventoryGuiSlot::paintComponent draws them: the requested item's ghost and count, and a
   // deconstruction mark.
   uint32_t slotGhostItem = 0;        // InventoryGuiSlot::ghostItem.id.baseID, 0 for none
   uint32_t slotGhostQuality = 0;     // InventoryGuiSlot::ghostItem.id.qualityID
   uint32_t slotGhostCount = 0;       // InventoryGuiSlot::ghostItem.count
   uint32_t slotGhostRemoval = 0;     // InventoryGuiSlot::ghostRemoval, bool
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
   // GameGuiWithControllerInventory::controllerInventory, ControllerInventoryHolder*
   uint32_t entityInventoryHolder = 0;
   uint32_t holderInventory = 0;      // GameControllerInventoryHolder::inventoryGui, InventoryGui
   uint32_t holderTitle = 0;          // GameControllerInventoryHolder::titleLabel ("Character")
   // RemoteControllerInventoryHolder::selectGui, IDWithQualityIDSelectListGui<IDWithQuality<ID<ItemPrototype>>>:
   // remote view's "Ghost cursor selection" in an entity's window, where the player's inventory
   // would be. A choice puts its item's ghost in hand.
   uint32_t remoteHolderSelect = 0;
   // SelectListGui<ID<ItemPrototype>>::subheader, agui::Frame, the base at offset 0 of that list:
   // holds its title label ("Ghost cursor selection").
   uint32_t itemSelectListSubheader = 0;
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
   // The deconstruction planner's window (DeconstructionItemGui), beside the player's inventory.
   // Its item part (DeconstructionItemGui::ItemPartHelper, a FrameWithSubheader) names the planner
   // and holds the buttons for it: rename, copy, export to a string, delete.
   uint32_t deconItemPart = 0;        // itemPart
   uint32_t deconItemPartName = 0;    // ItemPartHelper::nameLabel, agui::Label
   // The bar under a FrameWithSubheader's title, which holds the name and the buttons; the rest
   // of the window is the frame's body.
   uint32_t frameSubheader = 0;       // FrameWithSubheader::subheader, agui::Frame
   uint32_t deconDescription = 0;     // descriptionLabel, agui::Label
   uint32_t deconTreesAndRocks = 0;   // treesAndRocksOnlyCheckbox, agui::CheckBox
   uint32_t deconEntityTab = 0;       // entityTab, agui::Tab
   uint32_t deconTileTab = 0;         // tileTab, agui::Tab
   // Each tab's content: the whitelist/blacklist switch (LabeledSwitch) and the filter slots, a
   // DragPaneWidget<agui::Table> of ChooseButtons; the tiles tab also the tile mode dropdown.
   uint32_t deconEntityMode = 0;      // entityFiltersWidgets.modeSwitch
   uint32_t deconEntityFilters = 0;   // entityFiltersWidgets.table
   uint32_t deconTileMode = 0;        // tileFiltersWidgets.modeSwitch
   uint32_t deconTileSelection = 0;   // tileFiltersWidgets.tileModeDropdown, agui::DropDown
   uint32_t deconTileFilters = 0;     // tileFiltersWidgets.table
   // The upgrade planner's window (UpgradeItemGui), beside the player's inventory. Its item frame
   // (UpgradeItemGui::UpgradeItemFrame, a FrameWithSubheader) names the planner and holds the
   // buttons for it, as the deconstruction planner's does.
   uint32_t upgradeItemFrame = 0;     // upgradeItemFrame
   uint32_t upgradeItemName = 0;      // UpgradeItemFrame::nameLabel, agui::Label
   uint32_t upgradeDescription = 0;   // descriptionLabel, agui::Label
   // The rules, a DragPaneWidget<agui::Table> four to a row: first a "From To" pair of header
   // labels per column, then a HorizontalFlow per rule holding its From and To ChooseButtons.
   uint32_t upgradeRules = 0;         // mappersWidgets.table
   // A blueprint's setup window (BlueprintSetupGui, a Dialog): its settings (BlueprintSettingsGui,
   // a FrameWithSubheader) beside the preview picture (BlueprintWidget) in a frame of its own.
   uint32_t blueprintSettings = 0;    // BlueprintSetupGui::blueprintSettingsGui
   uint32_t blueprintPreview = 0;     // BlueprintSetupGui::blueprintPreview
   // The settings' subheader holds the name (BlueprintLabelEdit, an EditableLabel) and the
   // buttons for the blueprint; the rest are frames in a scroll pane.
   uint32_t blueprintName = 0;        // BlueprintSettingsGui::labelEdit
   uint32_t blueprintScroll = 0;      // verticalScroll, agui::VerticalScrollPane
   uint32_t blueprintDescription = 0; // descriptionEdit, TextBoxWithChatIconSelector
   uint32_t blueprintSnapCheckbox = 0; // snapToGridCheckbox, agui::CheckBox
   // The first field of the snapping table, a Table six to a row: grid size, grid position, then
   // the absolute and relative snapping radio buttons.
   uint32_t blueprintGridWidth = 0;   // snapToGridX, agui::TextField
   uint32_t blueprintComponents = 0;  // componentsTable, agui::Table of SimpleSlots
   // The include checkboxes (entities, modules, tiles, station names, trains, fuel, vehicles); the
   // game adds only those that apply to the blueprint to its filters frame.
   uint32_t blueprintInclude[7] = {};
   // An EditableLabel shows its text in a label beside a pencil button; while editing, a text field
   // it creates takes the label's place.
   uint32_t editableLabelText = 0;    // EditableLabel::label, agui::Label
   uint32_t editableLabelField = 0;   // EditableLabel::labelEdit, unique_ptr<TextFieldWithChatIconSelector>
   uint32_t editableLabelButton = 0;  // EditableLabel::switchEditLabelMode, IconButton

   // The blueprint's picture (BlueprintWidget). Every frame its paint asks BlueprintSelectionResult
   // Blueprint::selectionFromPosition(MapPosition const&, SetupBlueprintParameters const&) const
   // what lies under the mouse (the entity the game's hover would pick, honouring the include
   // checkboxes, else the tile) and keeps it in blueprintSelection, emptied while the mouse is off
   // the picture. A left click restores that entity or tile (remove = false), a right click
   // removes it, through the build and mine controls' buttons; Shift with the left button sets
   // blueprintShiftSelected, the grid position, from the click's place. editEnabled is off in
   // the previews of a book or the library. Map positions in the picture: the pixel less
   // PixelPosition getPixelShift(agui::Point const& absolute) const, over renderParameters.scale
   // times 32, from renderParameters.boundingBox.leftTop. Those are the blueprint's own
   // coordinates, which getTileBoxIgnoreSnapGrid (blueprintTileBox) measures it in.
   uint32_t pictureBlueprint = 0;      // BlueprintWidget::blueprint, Blueprint*
   uint32_t pictureParameters = 0;     // ::blueprintParameters, SetupBlueprintParameters
   uint32_t pictureSelection = 0;      // ::blueprintSelection, BlueprintSelectionResult
   uint32_t pictureEditEnabled = 0;    // ::editEnabled, bool
   uint32_t pictureScale = 0;          // ::renderParameters.scale, double
   uint32_t pictureViewLeftTop = 0;    // ::renderParameters.boundingBox.leftTop, MapPosition
   // Alt mode, as the paint reads it: bool GameAdapter::getShowEntityInfo() const, a virtual slot,
   // on the player's latency adapter or else its game state adapter. (The picture's own render
   // flags have it forced on between a click and the next paint.)
   uint32_t picturePlayer = 0;         // ::context.player, Player*
   uint32_t adapterShowEntityInfo = 0;
   uintptr_t blueprintSelectionAt = 0;
   uintptr_t picturePixelShift = 0;
   uint32_t selectionResultSize = 0;   // sizeof(BlueprintSelectionResult)
   uint32_t selectionEntity = 0;       // BlueprintSelectionResult::entity, Entity*
   uint32_t selectionTile = 0;         // ::tileID, ID<TilePrototype>
   uint32_t selectionIndex = 0;        // ::index into the blueprint's entities or tiles
   // Which entities and tiles are removed: std::vector<ConfigureBlueprintEntityItem> and
   // <ConfigureBlueprintTileItem>, a bool remove each, by index; shorter than the blueprint when
   // the last ones were never touched.
   uint32_t parametersEntities = 0;    // SetupBlueprintParameters::entitiesData
   uint32_t parametersTiles = 0;       // ::tilesData
   // The blueprint's entities, std::vector<BlueprintEntities::EntityData>: each an Entity* the
   // picture draws and the items to be delivered to it (InsertPlan, a FlatMap from an item
   // IDWithQuality to its ItemInventoryPositions: a grid count and the stacks it goes into, each
   // with a count). Its tiles, std::vector<TileWithPosition>.
   uint32_t blueprintEntityList = 0;   // Blueprint::entities.data
   uint32_t entityDataSize = 0;
   uint32_t entityDataInsertPlan = 0;  // EntityData::insertPlan.data.data, the pairs' vector
   uint32_t insertPairSize = 0;        // Pair<IDWithQuality<ItemID>, ItemInventoryPositions>
   uint32_t insertPairPositions = 0;   // ::second
   uint32_t positionsGridCount = 0;    // ItemInventoryPositions::gridCount
   uint32_t positionsStacks = 0;       // ::inventoryPositions, std::vector<ItemStackLocationWithCount>
   uint32_t stackLocationSize = 0;
   uint32_t stackLocationCount = 0;
   // What the picture shows of an entity. Direction Entity::getDirection() const and bool
   // Entity::hasDirection() const, virtual slots; the quality badge (EntityWithOwner::qualityID)
   // and the details only while alt mode is on, as the game draws them.
   uint32_t entityGetDirection = 0;
   uint32_t entityHasDirection = 0;
   uint32_t entityQuality = 0;         // EntityWithOwner::qualityID, ID<QualityPrototype>
   uint32_t craftingRecipe = 0;        // CraftingMachine::recipeID, IDWithQuality<RecipeID>
   // A blueprint's recipe the player's force has not unlocked is drawn crossed out
   // (CraftingMachine::draw while rendering a blueprint), except in the map editor (the controller,
   // or the one before a pause, an EditorController): the force's Recipes::indexToInstance by
   // recipe ID, each a Recipe with its enabled flag. The player's force and map: playerForce,
   // playerMap.
   uint32_t playerControllerBeforePause = 0; // Player::controllerManager.controllerBeforePause, Controller*
   uint32_t mapForces = 0;             // Map::forceManager.sortedForceDataList.begin_, ForceData** by ForceID
   uint32_t forceRecipes = 0;          // ForceData::recipes, unique_ptr<Recipes>
   uint32_t recipeInstances = 0;       // Recipes::indexToInstance, std::vector<Recipe>
   uint32_t recipeSize = 0;
   uint32_t recipeEnabled = 0;         // Recipe::enabled, bool
   // Item filters, IDWithQualityFilter: an item ID and a QualityCondition (quality, comparison).
   uint32_t itemFilterSize = 0;
   uint32_t itemFilterId = 0;          // ::baseID, ID<ItemPrototype>
   uint32_t itemFilterQuality = 0;     // ::qualityCondition.qualityID, 0 for any quality
   uint32_t inserterFilters = 0;       // Inserter::filter, std::array of 5
   uint32_t inserterFlags = 0;         // Inserter::flags, InserterFlags
   uint32_t splitterLogic = 0;         // Splitter::leftLogic, SplitterLogic
   uint32_t laneSplitterLogic = 0;     // LaneSplitter::logic
   uint32_t splitterInputLocked = 0;   // SplitterLogic::inputLocked, bool
   uint32_t splitterOutputLocked = 0;  // ::outputLocked, bool
   uint32_t splitterTakeFrom = 0;      // ::takeNextItemFrom, SplitterDirection
   uint32_t splitterGoesTo = 0;        // ::nextItemGoesTo, SplitterDirection
   uint32_t splitterFilter = 0;        // ::filter
   uint32_t splitterRight = 0;         // SplitterDirection Right
   uint32_t loaderFilters = 0;         // Loader::filter, std::array of 5
   uint32_t loaderFilterMode = 0;      // Loader::filterMode, Loader::FilterMode
   uint32_t loaderType = 0;            // Loader::type, LoaderType
   uint32_t loaderPerLane = 0;         // LoaderPrototype::perLaneFilters, bool
   uint32_t loaderWhitelist = 0;       // Loader::FilterMode Whitelist
   uint32_t loaderBlacklist = 0;       // Loader::FilterMode Blacklist
   uint32_t loaderOutput = 0;          // LoaderType Output
   uint32_t undergroundType = 0;       // UndergroundBelt::type, UndergroundBeltType
   uint32_t undergroundOutput = 0;     // UndergroundBeltType Output
   // Combinators. Their operation shows on their display always; the signals only with alt mode
   // and the interface setting "show combinator settings" on.
   uint32_t showCombinatorSettings = 0; // InterfaceSettings::showCombinatorSettingsWhenDetailedInfoIsOn.value
   uint32_t arithmeticParameters = 0;  // ArithmeticCombinator::controlBehavior.parameters
   uint32_t arithmeticFirst = 0;       // ArithmeticCombinatorParameters::first, SignalOrConstant
   uint32_t arithmeticSecond = 0;      // ::second
   uint32_t arithmeticOperation = 0;   // ::operation, ArithmeticCombinatorParameters::Operation
   uint32_t arithmeticOutput = 0;      // ::output, SignalID
   uint32_t signalOrConstantType = 0;  // SignalOrConstant::type, SignalOrConstant::Type
   uint32_t signalOrConstantSignal = 0; // ::signal, SignalID
   uint32_t signalOrConstantIsSignal = 0; // SignalOrConstant::Type Signal
   uint32_t deciderConditions = 0;     // DeciderCombinator::controlBehavior.parameters.conditions
   uint32_t deciderOutputs = 0;        // ::outputs, std::vector<DeciderCombinatorParameters::Output>
   uint32_t conditionFirst = 0;        // DeciderCombinatorParameters::Condition::first, SignalID
   uint32_t conditionComparator = 0;   // ::comparator, Comparison
   uint32_t conditionSecond = 0;       // ::second, SignalOrConstant
   uint32_t deciderOutputSignal = 0;   // DeciderCombinatorParameters::Output::signalId
   uint32_t selectorParameters = 0;    // SelectorCombinator::controlBehavior.parameters
   uint32_t selectorOperation = 0;     // SelectorCombinatorParameters::operation
   uint32_t selectorMax = 0;           // ::selectMax, bool
   uint32_t selectorIndexSignal = 0;   // ::index.signal, SignalID
   uint32_t selectorCountSignal = 0;   // ::countSignalID
   uint32_t constantSignals = 0;       // ConstantCombinator::controlBehavior.sections.compiled
   uint32_t compiledFilterSize = 0;    // CompiledLogisticFilter, its SignalFilter first
   // The enumerators of the operations, in the order vocab names them.
   uint32_t arithmeticOperations[11] = {};
   uint32_t comparisons[6] = {};
   uint32_t selectorOperations[9] = {};
   uint32_t selectorSelect = 0;
   uint32_t selectorCount = 0;
   // SignalID: an IDWithQuality<SignalIDBase>, the base a type and an index packed in 32 bits.
   // PrototypeBase const* SignalIDBase::getPrototypeSafe() const resolves it, or null.
   uint32_t signalQuality = 0;         // IDWithQuality<SignalIDBase>::qualityID
   uintptr_t signalPrototype = 0;
   uint32_t pumpFilter = 0;            // Pump::fluidBox.buffer.filter.fluidID, ID<FluidPrototype>
   uint32_t collectorFilters = 0;      // AsteroidCollector::chunkFilters, std::vector<ID<AsteroidChunkPrototype>>
   uint32_t panelIcon = 0;             // DisplayPanel::icon, SignalID
   uint32_t panelText = 0;             // DisplayPanel::text, std::string
   uint32_t panelAlwaysShow = 0;       // DisplayPanel::alwaysShow, bool
   uintptr_t tilePrototypes = 0;
   uintptr_t fluidPrototypes = 0;
   uintptr_t asteroidChunkPrototypes = 0;

   // What the slot of a blueprint, a book or a planner shows (BlueprintItem::draw and the like): the
   // item's icon under the up to four icons its owner chose (PreviewIcons, a std::vector<SignalID>),
   // its name on hover. A book without icons shows its active item, drawn small inside it; a planner
   // without icons shows its first filters (DeconstructionData::getIcons, UpgradeData::getIcons).
   uint32_t itemStackData = 0;         // ItemStack::item, Item*: the stack's own data, null for plain items
   uint32_t inventoryHand = 0;         // Inventory::handPosition: the slot whose item is in the hand
   uint32_t itemLabel = 0;             // ItemWithLabel::labelData.label.value, std::string
   uint32_t signalSize = 0;            // sizeof(SignalID)
   // A blueprint item and a library record hold the same Blueprint, DeconstructionData and
   // UpgradeData, so those are read from wherever they sit.
   uint32_t blueprintDataIcons = 0;    // Blueprint::previewIcons.data
   uint32_t blueprintDataDescription = 0; // Blueprint::description.value
   uint32_t blueprintItemBlueprint = 0; // BlueprintItem::blueprint
   uint32_t bookIcons = 0;             // BlueprintBook::previewIcons.data
   uint32_t bookDescription = 0;       // BlueprintBook::description.value
   uint32_t bookActiveIndex = 0;       // BlueprintBook::activeIndex, the slot built from
   uint32_t bookInventory = 0;         // BlueprintBook::inventory, an Inventory
   uint32_t deconItemData = 0;         // DeconstructionItem::deconstructionData
   uint32_t deconDataIcons = 0;        // DeconstructionData::previewIcons.data
   uint32_t deconDataDescription = 0;  // ::description.value
   uint32_t deconDataTreesAndRocks = 0; // ::treesAndRocksOnly, bool
   uint32_t deconDataEntityMode = 0;   // ::entityFilterMode
   uint32_t deconDataEntities = 0;     // ::entityFilters, std::vector<IDWithQualityFilter<EntityID>>
   uint32_t deconDataTileMode = 0;     // ::tileSelectionMode
   uint32_t deconDataTiles = 0;        // ::tileFilters, std::vector<ID<TilePrototype>>
   uint32_t entityFilterWhitelist = 0; // DeconstructionData::EntityFilterMode Whitelist
   uint32_t entityFilterBlacklist = 0; // DeconstructionData::EntityFilterMode Blacklist
   uint32_t tileSelectionOnly = 0;     // DeconstructionData::TileSelectionMode Only
   uint32_t tileSelectionNever = 0;    // DeconstructionData::TileSelectionMode Never
   uint32_t entityFilterSize = 0;      // sizeof(IDWithQualityFilter<ID<EntityPrototype>>)
   uint32_t entityFilterId = 0;        // ::baseID
   uint32_t entityFilterQuality = 0;   // ::qualityCondition.qualityID
   uint32_t entityFilterComparison = 0; // ::qualityCondition.comparison, Comparison::Enum
   uint32_t comparisonEquals = 0;      // Comparison::Enum Equals: only then is the quality drawn
   uint32_t upgradeItemData = 0;       // UpgradeItem::upgradeData
   uint32_t upgradeDataIcons = 0;      // UpgradeData::previewIcons.data
   uint32_t upgradeDataDescription = 0; // ::description.value
   uint32_t upgradeDataMappings = 0;   // ::mappings, std::vector<UpgradeMapping>
   uint32_t mappingSize = 0;           // sizeof(UpgradeMapping)
   // A rule counts when its source is set; its destination, an UpgradeID (UpgradeIDBase {type,
   // an item or entity ID}, quality), is the icon.
   uint32_t mappingSourceId = 0;       // UpgradeMapping::source.filter.baseID.itemID
   uint32_t mappingSourceQuality = 0;  // ::source.filter.qualityCondition.qualityID
   uint32_t mappingSourceEntity = 0;   // ::source.entityFilter.baseID
   uint32_t mappingSourceEntityQuality = 0; // ::source.entityFilter.qualityCondition.qualityID
   uint32_t mappingDestinationType = 0; // ::destination.upgradeID.baseID.type, UpgradeIDBase::Type
   uint32_t mappingDestinationId = 0;  // ::destination.upgradeID.baseID.itemID
   uint32_t mappingDestinationQuality = 0; // ::destination.upgradeID.qualityID
   uint32_t upgradeTypeEntity = 0;     // UpgradeIDBase::Type Entity
   // A book's slot (BlueprintBookSlot, an InventoryGuiSlot) is highlighted when it is the book's
   // active one.
   uint32_t bookSlotBook = 0;          // BlueprintBookSlot::book, BlueprintBook*

   // A blueprint book's window (BlueprintBookGui), beside the player's inventory. The header frame
   // (a FrameWithSubheader) has the book's buttons in its subheader (copy, upgrade, export,
   // destroy) and the navigation flow in its body: a row with the go-to-root arrow and where the
   // book is ("Inventory: name"), a row per book it is inside with an arrow and that book's name,
   // and the book's own row (an arrow, its name and the rename button); then the description. The
   // inside frame's subheader has the cycling hint and the view buttons, its body the contents:
   // a BlueprintsList, a Table of one BlueprintBookSlot per slot of the book.
   uint32_t bookGuiHeader = 0;         // BlueprintBookGui::headerFrame, FrameWithSubheader
   uint32_t bookGuiInside = 0;         // ::insideFrame, FrameWithSubheader
   uint32_t bookGuiNavigation = 0;     // ::navigationFlow, agui::VerticalFlow
   uint32_t bookGuiName = 0;           // ::blueprintBookLabel, agui::Label
   uint32_t bookGuiRename = 0;         // ::editButton, IconButton
   uint32_t bookGuiDescription = 0;    // ::descriptionLabel, agui::Label
   uint32_t bookGuiList = 0;           // ::blueprintsList, BlueprintsList
   // How the list lays its items out (the player's choice, kept across books): List puts each
   // slot in a row with its name and description, Grid its name under it, Slots the slot alone.
   uint32_t listViewMode = 0;          // BlueprintsList::viewMode, BlueprintsListViewMode
   uint32_t listViewList = 0;          // BlueprintsListViewMode List

   // A blueprint library record (BlueprintRecord: a SingleBlueprintRecord, a BlueprintBookRecord,
   // a DeconstructionRecord or an UpgradeRecord), shown by a BlueprintRecordSlotButton as its item
   // would be: the item's icon under the owner's icons, grey while only its preview has arrived.
   // The slot shows a hand while the player holds the record, the transfer's progress under it,
   // and highlights the active one of a book.
   uint32_t recordId = 0;              // BlueprintRecord::id, BlueprintRecordID
   uint32_t recordItem = 0;            // ::itemID, ID<ItemPrototype>
   uint32_t recordLabel = 0;           // ::label.value, std::string
   uint32_t recordIdSize = 0;          // sizeof(BlueprintRecordID)
   uint32_t recordIdPlayer = 0;        // BlueprintRecordID::playerIndex
   uint32_t recordIdIndex = 0;         // BlueprintRecordID::id
   uint32_t recordIsPreview = 0;       // virtual bool BlueprintRecord::isPreview() const
   uint32_t singleRecordBlueprint = 0; // SingleBlueprintRecord::blueprint
   uint32_t bookRecordRecords = 0;     // BlueprintBookRecord::records, std::vector<std::unique_ptr<BlueprintRecord>>
   // uint16_t BlueprintBookRecord::getActiveIndex(Player const*, LatencyState*) const: the record a
   // player builds from. A book on the game's shelf or another player's keeps one per player.
   uintptr_t bookRecordActiveIndex = 0;
   uint32_t playerLatencyState = 0;    // Player::latencyState, LatencyState* (null without latency hiding)
   uint32_t bookRecordIcons = 0;       // ::previewIcons.data
   uint32_t bookRecordDescription = 0; // ::description.value
   uint32_t deconRecordData = 0;       // DeconstructionRecord::deconstructionData
   uint32_t upgradeRecordData = 0;     // UpgradeRecord::upgradeData
   // BlueprintRecord const* BlueprintRecordSlotButton::getRecord() const: by its id, or by its place
   // in a book for a book in the player's hand.
   uintptr_t recordSlotRecord = 0;
   uint32_t recordSlotPlayer = 0;      // BlueprintRecordSlotButton::context.player, Player*
   uint32_t recordSlotBook = 0;        // ::parentBook, BlueprintBookRecord*, null on a shelf
   uint32_t recordSlotIndex = 0;       // ::location.slotIndex
   uint32_t recordSlotGrabbed = 0;     // ::showGrabbed, ShowGrabbed
   uint32_t recordSlotShowsGrabbed = 0; // BlueprintRecordSlotButton::ShowGrabbed True
   uint32_t recordSlotProgress = 0;    // ::progress, float: the transfer's, drawn while in (0, 1)
   // virtual BlueprintRecordID GameAdapter::getCursorRecordID() const: the record in the hand.
   uint32_t adapterCursorRecord = 0;

   // The blueprint library's window (BlueprintLibraryGui), beside the player's inventory, with the
   // history arrows in its title bar. Its inside frame (a FrameWithSubheader) holds the "not
   // synchronised" warning and the view buttons in its subheader, and the tabs (My blueprints, Game
   // blueprints) in its body, each a BlueprintShelfWidget per shelf: the "synchronising" label or
   // its BlueprintsList of BlueprintRecordSlotButtons, padded with empty slots to drop into. A book
   // record opened swaps the inside frame for a BlueprintBookRecordWidget in the book holder, laid
   // out as a book's window: the header frame's subheader has the copy, upgrade, export and delete
   // buttons, its body the BlueprintBookHeader (a VerticalFlow of rows: the go-to-root arrow and the
   // shelf's name, an arrow and name per book it is inside, the book's arrow, name and rename
   // button), then the description; the inside frame has the hint and view buttons over the list.
   uint32_t libraryInside = 0;         // BlueprintLibraryGui::insideFrame, FrameWithSubheader
   uint32_t libraryTabs = 0;           // ::libraryTabs, agui::TabbedPane
   uint32_t libraryBookHolder = 0;     // ::bookWidgetHolder, agui::VerticalFlow
   uint32_t libraryMemory = 0;         // ::memoryUsageLabel, agui::Label
   uint32_t shelfList = 0;             // BlueprintShelfWidget::blueprintsList, BlueprintsList
   uint32_t shelfSynchronising = 0;    // ::synchronisingLabel, agui::Label
   uint32_t bookRecordGuiHeader = 0;   // BlueprintBookRecordWidget::headerFrame, FrameWithSubheader
   uint32_t bookRecordGuiInside = 0;   // ::insideFrame, FrameWithSubheader
   uint32_t bookRecordGuiNavigation = 0; // ::windowHeader, BlueprintBookHeader
   uint32_t bookRecordGuiDescription = 0; // ::descriptionLabel, agui::Label
   uint32_t bookRecordGuiList = 0;     // ::blueprintsList, BlueprintsList
   uint32_t bookRecordGuiRecord = 0;   // ::recordID, BlueprintRecordID
   uint32_t bookHeaderName = 0;        // BlueprintBookHeader::nameLabel, agui::Label
   uint32_t bookHeaderRename = 0;      // ::editButton, IconButton

   // The achievements window (AchievementGui), opened from the side menu in place of the inventory.
   // Its inside frame's subheader holds the bar and the "Earned 12 of 47" label; the modded-game
   // and played-too-little labels show over the frame when they apply. The card holder (a scroll
   // pane) lists an AchievementCard per achievement, earned first, then normal, then failed, the
   // hidden ones left out until earned; the title bar's search hides the cards that do not match.
   // A card's description flow holds the name, the description and the progress (labels and a
   // progress bar whose text is "12.3k/1.0M"), or the failure's reason; its right flow holds the
   // track toggle (normal cards) or a warning icon (failed ones). The game refreshes a card's
   // description each second while it is scrolled into view.
   uint32_t achievementsHolder = 0;    // AchievementGui::achievementHolder, AchievementCardHolder
   uint32_t achievementsProgress = 0;  // ::progressLabel, agui::Label
   uint32_t achievementsBar = 0;       // ::progressBar, agui::ProgressBar
   uint32_t achievementsModded = 0;    // ::moddedGame, agui::Label
   uint32_t achievementsPlaytime = 0;  // ::notInGameLongEnoughLabel, agui::Label
   uint32_t achievementCardPrototype = 0; // AchievementCard::achievementPrototype, AchievementPrototype*
   uint32_t achievementCardState = 0;  // ::state, AchievementState
   uint32_t achievementCardRight = 0;  // ::rightFlow, agui::VerticalFlow
   uint32_t achievementCardDescription = 0; // ::descriptionFlow, agui::VerticalFlow
   uint32_t achievementCompleted = 0;  // AchievementState Completed
   uint32_t achievementFailed = 0;     // AchievementState Failed

   // The tips and tricks window (TipsAndTricksGui), opened from the side menu or the "New tip"
   // button; it pauses a single player game while open. TipsAndTricksGui::populateListbox makes a
   // list box button per TipsAndTricks::items entry, in that order, so a button's index is its
   // tip's: its caption indented six spaces per TipsAndTricksItem::indent, a title tip (the heading
   // of the indented tips under it) styled apart, a Suggested tip given the notification sprite, a
   // Locked or DependenciesNotMet one hidden. The title bar's search hides the buttons that do not
   // match. updateSelection shows the selected tip in contentFlow: the title label, the simulation
   // or image (nothing to read), then the description (a LabelWithHoverableRichText in a scroll
   // pane) over the Play tutorial button (tips with a tutorial; disabled in multiplayer) and Mark
   // as unread (enabled once read). With nothing selected nothingFoundFlow shows instead: "No tips
   // and tricks selected" and unlockMessage (not a freeplay, cheat mode), empty when neither.
   uint32_t tipsList = 0;              // TipsAndTricksGui::listbox, agui::ListBox
   uint32_t tipsContent = 0;           // ::contentFlow, agui::VerticalFlow
   uint32_t tipsTitle = 0;             // ::title, agui::Label
   uint32_t tipsText = 0;              // ::text, LabelWithHoverableRichText
   uint32_t tipsPlayTutorial = 0;      // ::playTutorialButton, agui::TextButton
   uint32_t tipsUnread = 0;            // ::unreadButton, agui::TextButton
   uint32_t tipsNothingFound = 0;      // ::nothingFoundFlow, agui::VerticalFlow
   uint32_t globalTipsAndTricks = 0;   // GlobalContext::tipsAndTricks.value, TipsAndTricks*
   uint32_t tipsItems = 0;             // TipsAndTricks::items, std::vector<std::unique_ptr<TipsAndTricksItem>>
   uint32_t tipItemIndent = 0;         // TipsAndTricksItem::indent, uint8_t
   uint32_t tipItemIsTitle = 0;        // ::isTitle, bool
   uint32_t tipItemStatus = 0;         // ::status, TipStatus (a TipStatus::Enum)
   uint32_t tipStatusSuggested = 0;    // TipStatus::Enum Suggested: the notification sprite

   // The keys of a mod's custom input: ControlSettings::customInputs, std::vector<ControlInput>,
   // each naming its CustomInputPrototype and holding the player's two keyboard bindings
   // (SimpleConfigItem<ControlInputValue>): a ControlInputValue::Type (Keyboard for a key), an
   // SDL_Scancode and the modifiers held with it. SDL_Keycode SDL_GetKeyFromScancode(SDL_Scancode,
   // SDL_Keymod, bool), the game's own SDL, turns the scancode into the key events' keycode.
   uint32_t customInputs = 0;
   uint32_t controlInputSize = 0;
   uint32_t controlInputPrototype = 0; // ControlInput::customInputPrototype
   uint32_t controlInputKey1 = 0;      // ::keyboardAndMouseInput1.value
   uint32_t controlInputKey2 = 0;      // ::keyboardAndMouseInput2.value
   uint32_t inputValueType = 0;        // ControlInputValue::type
   uint32_t inputValueScancode = 0;    // ::scancode
   uint32_t inputValueModifiers = 0;   // ::modifiers
   uint32_t inputValueKeyboard = 0;    // ControlInputValue::Type Keyboard
   uintptr_t keyFromScancode = 0;
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
   // A player's mod GUI (LuaPlayer::gui): a root element per position, and for player.gui.screen
   // each child's widget is added straight to the Gui's base widget
   // (CustomEmptyWidget::addScreenWidget), beside the game's own windows. A Lua frame there is a
   // plain agui::Window.
   uint32_t playerCustomGui = 0;             // Player::customGui, CustomGui*
   uint32_t customGuiRootElements = 0;       // CustomGui::rootElements, std::map<Position, CustomGuiElement*>
   // CustomGui::modOwnersElementMapping, std::map<std::string, std::set<unsigned>>: "mod-<name>" to
   // the indices of every element that mod made.
   uint32_t customGuiModOwners = 0;
   uint32_t customGuiScreen = 0;             // CustomGui::Position Screen (a one-byte enum)
   uint32_t positionNodeKey = 0;             // the rootElements node's _Myval.first
   uint32_t positionNodeElement = 0;         // ::_Myval.second
   uint32_t ownerNodeName = 0;               // the modOwnersElementMapping node's _Myval.first
   uint32_t ownerNodeIndices = 0;            // ::_Myval.second, std::set<unsigned>
   uint32_t indexNodeValue = 0;              // the std::set<unsigned> node's _Myval
   uint32_t guiElementWidget = 0;            // CustomGuiElement::widget, agui::Widget*
   uint32_t guiElementIndex = 0;             // CustomGuiElement::index
   uint32_t guiElementChildren = 0;          // CustomGuiElement::children, std::vector<CustomGuiElement*>
   // What the player has open (LuaPlayer::opened), the controller's GuiTarget: a deque of
   // GuiTargetData, the topmost with a type being the open one (GuiTarget::current, which needs a
   // non-empty deque). A mod's element set as opened has the type CustomGui and its element in the
   // customGui targeter.
   uint32_t controllerGuiTarget = 0;         // Controller::guiTarget, GuiTarget
   uint32_t guiTargetSize = 0;               // GuiTarget::data._Mypair._Myval2._Mysize
   uintptr_t guiTargetCurrent = 0;           // GuiTargetData const& GuiTarget::current() const
   uint32_t guiTargetType = 0;               // GuiTargetItemBase::openGuiType, OpenGuiType (one byte)
   uint32_t guiTargetCustomGui = 0;          // GuiTargetItemBase::customGui, Targeter<CustomGuiElement>
   // The game's Targeter (TargeterBase, with a vtable), not agui's GenericTargeterBase.
   uint32_t gameTargeterTarget = 0;          // TargeterBase::target, Targetable*
   uint32_t openGuiTypeCustomGui = 0;        // OpenGuiType CustomGui

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

   // The achievements the player tracks, at the top left under the mods' gui (GameView::loadGui puts
   // it in topLeftContainer): an AchievementCardHolder of sidebar cards, each the achievement's icon
   // (its name only as the tooltip), its progress, and a track button that stops tracking it.
   // GameView::trackedAchievementHolder, std::unique_ptr<AchievementCardHolder>
   uint32_t gameViewTrackedAchievements = 0;

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
   // CurrentResearchInfo::title, agui::Label: the technology, or "not researching"
   uint32_t researchTitle = 0;
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
   // The map search's results in remote view (GameView::chartSearchResultGui, a Window at the right):
   // what the search box in remote view's title bar found, filled as the player types. Like the
   // alerts window, a ListBox of rows and a flow of their pin buttons.
   uint32_t gameViewChartSearch = 0;     // std::unique_ptr<ChartSearchResultGui>
   // ChartSearchResultGui::listbox, agui::ListBox: a row per result, as the game words it ("[item=
   // iron-ore] Iron ore 402k"). Recipes in machines first, then map tags, train stops, resource
   // patches and tiles. A click moves the camera to the result.
   uint32_t chartSearchList = 0;
   // ChartSearchResultGui::pinButtonFlow, agui::VerticalFlow: beside each row its pin button (an
   // IconButton, gui.pin-search-result), which pins the result to the pins panel.
   uint32_t chartSearchPins = 0;
   // The map view options at the right in remote view (MapViewOptionsGui, a VerticalFlow): a frame
   // with the add tag and add ping buttons, and on the map a frame with a table of the overlay
   // toggles (IconButtons named by their tooltips: logistic network, electric network, turret range,
   // pollution, station names, player names, tags, worker robots, rail signal states, recipe icons,
   // pipelines). A toggle flips its MapViewSettings item, which is this client's alone.
   uint32_t gameViewMapViewOptions = 0;  // GameView::mapViewOptionsGui, std::unique_ptr<MapViewOptionsGui>
   // What the map draws (MapViewSettings, the client's config, not game state). Each item is a
   // SimpleConfigItem<bool>; every overlay but station names, player names and tags shows only while
   // showNonstandardMapInfo is also on, as MapViewOptionsGui::updateToggleState shows them.
   uint32_t globalMapViewSettings = 0;   // GlobalContext::mapViewSettings.value, MapViewSettings*

   // Files inside a mod, read as the game reads them whether the mod is a folder or a zip: the
   // ModManager turns "__mod__/path" into a PackagePath, which opens a ReadStream.
   uint32_t globalModManager = 0; // GlobalContext::modManager.value, ModManager*
   // PackagePath ModManager::resolveResourcePath(std::string_view) const; throws
   // ResolveResourcePathError for a path of no enabled mod.
   uintptr_t resolveResourcePath = 0;
   // UniquePointer<ReadStream> PackagePath::open() const; throws when the file is missing.
   uintptr_t packagePathOpen = 0;
   uint32_t packagePathSize = 0;     // sizeof(PackagePath): Package*, then Filesystem::Path
   uint32_t packagePathPath = 0;     // PackagePath::path, Filesystem::Path, a std::wstring
   uint32_t readStreamRead = 0;       // vtable slot of uint64 ReadStream::read(char*, uint64)
   uint32_t readStreamRemaining = 0;  // vtable slot of uint64 ReadStream::remaining() const
   uint32_t readStreamDestructor = 0; // vtable slot of ReadStream::~ReadStream (scalar deleting)
   uintptr_t operatorDelete = 0;      // the game's void operator delete(void*, size_t)
   uint32_t mapViewLogisticNetwork = 0;
   uint32_t mapViewElectricNetwork = 0;
   uint32_t mapViewTurretRange = 0;
   uint32_t mapViewPollution = 0;
   uint32_t mapViewStationNames = 0;
   uint32_t mapViewPlayerNames = 0;
   uint32_t mapViewTags = 0;
   uint32_t mapViewWorkerRobots = 0;
   uint32_t mapViewRailSignalStates = 0;
   uint32_t mapViewRecipeIcons = 0;
   uint32_t mapViewPipelines = 0;
   uint32_t mapViewNonstandardInfo = 0;
   // IconButtonWithNumber::count. The alert button blinks by setting it to 0 every other half second.
   uint32_t iconButtonCount = 0;
   uint32_t gameViewGoal = 0;            // GameView::goalDescription, std::unique_ptr<GoalDescription>
   // The goal window's frame: the goal text (GoalDescription::label), then the root of the
   // scenario's player.gui.goal, where story.lua's set_info puts hints, progress bars and tables.
   uint32_t goalInnerFrame = 0;          // GoalDescription::innerFrame, agui::Frame
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
   // agui::Label::resizableText.richTextData, std::unique_ptr to TextDrawSections
   uint32_t labelRichText = 0;
   uint32_t richTextSectionsBegin = 0;   // TextDrawSections::sections.begin_, TextDrawSection*
   uint32_t richTextSectionsEnd = 0;     // TextDrawSections::sections.end_
   uint32_t richTextSectionSize = 0;     // sizeof(TextDrawSection)
   uint32_t richTextSectionType = 0;     // TextDrawSection::type, TagType
   uint32_t richTextSectionTag = 0;      // TextDrawSection::tagText, std::string_view: "item=iron-plate"
   // TextDrawSection::text, std::string_view: a Text section's words, line breaks included
   uint32_t richTextSectionText = 0;
   uint32_t hoverableLabelManager = 0;   // LabelWithHoverableRichText::hoverManger, LabelRichTextHoverManager
   uint32_t hoverManagerTooltip = 0;     // RichTextHoverManager::hoverTooltip, GenericTargeter<agui::ToolTip>
   // void RichTextHoverManager::handleHover(TextDrawSection const&, OutputConsole::Item const*, bool
   // clicked): what the label runs for the section under the mouse, on a move and on a click.
   uintptr_t richTextHandleHover = 0;
   uintptr_t richTextClearTooltip = 0;   // void RichTextHoverManager::clearTooltip()

   // The console's log, as the open console draws it: OutputConsoleRenderer::getRenderItems merges
   // the two lists of the player's OutputConsole newest first, until the top of the screen. Each
   // line keeps its text wrapped at the width it was last drawn at, with the line's rich text laid
   // out in sections as a label's are; its icons hover and click through the player's own hover
   // manager, which needs the line (a gps tag pings and opens the map).
   uint32_t playerOutputConsole = 0;      // Player::outputConsole, OutputConsole*
   uint32_t playerConsoleHoverManager = 0; // Player::outputConsoleRichTextHoverManager, std::unique_ptr
   // Each list (outputConsoleItems, outputConsoleItemsNotSaved) starts with its head node pointer.
   uint32_t consoleNodeNext = 0;          // std::_List_node<OutputConsole::Item>::_Next
   uint32_t consoleNodeValue = 0;         // std::_List_node<OutputConsole::Item>::_Myval, the Item
   uint32_t consoleItemUpdateTick = 0;    // OutputConsole::Item::updateTick, MapTick (u64)
   uint32_t consoleItemWrappedText = 0;   // OutputConsole::Item::wrappedText, std::unique_ptr<agui::ResizableText>
   uint32_t resizableTextData = 0;        // agui::ResizableText::data, std::string: "[color=#..]Name[/color]: text"
   uint32_t resizableTextRichText = 0;    // agui::ResizableText::richTextData, std::unique_ptr to TextDrawSections
   uint32_t resizableTextMaxWidth = 0;    // agui::ResizableText::lastMaxWidth, int
   uint32_t consoleRenderItemSize = 0;    // sizeof(OutputConsoleRenderer::RenderItem)
   uint32_t consoleRenderItemItem = 0;    // OutputConsoleRenderer::RenderItem::item, OutputConsole::Item const*
   // std::vector<RenderItem> OutputConsoleRenderer::getRenderItems(OutputConsole const&, bool consoleOpen,
   // MapTick, DrawQueue*, int maxWidth): reads no member, so any `this` will do.
   uintptr_t consoleGetRenderItems = 0;
   uintptr_t consoleRenderItemsFree = 0;  // std::vector<OutputConsoleRenderer::RenderItem>::~vector
   uintptr_t resizableTextLines = 0;      // agui::ResizableText::lines() const: wraps and lays out, if stale

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
   // LocalisedString::LocalisedString(Mode, char const*): with Mode::Literal, a parameter the game
   // says as it is.
   uintptr_t localisedStringLiteral = 0;
   // LocalisedString::LocalisedString(std::string const& key, LocalisedString const&...): a key
   // with one, two and three parameters, which it copies.
   uintptr_t localisedStringWithParameters[3] = {};
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
   // slot of Tile const* Controller::deduceSelectedTile(MapPosition const&) const
   uint32_t controllerSelectedTile = 0;
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
inline constexpr uint32_t kEntityNotOnMap = 0x1000;               // "not-on-map"
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

// RenderParameters::Flags::Enum bits, code constants: ShowEntityInfo 8 (alt mode) and
// ShowStatusIcons 17.
inline constexpr uint32_t kRenderShowEntityInfo = 1u << 8;
inline constexpr uint32_t kRenderShowStatusIcons = 1u << 17;
// DrawingFlags bits, code constants: the backing pass of an info icon, and a filter or any-quality
// icon, which shows a quality badge even without a quality.
inline constexpr uint32_t kDrawingFlagIconBackground = 0x400;
inline constexpr uint32_t kDrawingFlagQualityFilter = 0x2000;
inline constexpr uint32_t kDrawingFlagAnyQuality = 0x4000;
// Comparison::Enum, code constants.
inline constexpr uint8_t kComparisonEquals = 2;

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
