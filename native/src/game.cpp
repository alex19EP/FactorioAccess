#include "game.h"

#include "log.h"
#include "symbols.h"

#include <format>
#include <string>
#include <string_view>

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
   auto enumerator = [&](uint32_t& out, const char* type, const char* name) {
      if (auto value = symbols.enumValue(type, name))
         out = static_cast<uint32_t>(*value);
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
   address(layout.dispatchItemDrag, "?dispatchItemDrag@Widget@agui@@QEAAXHH@Z");
   slot(layout.slotInsert, "insert");
   slot(layout.slotRemove, "remove");
   slot(layout.slotSwapChildren, "swapChildren");
   offset(layout.dragPaneMode, "DragPane", "mode");
   enumerator(layout.dragModeSwap, "DragPane::DragMode", "SWAP");
   classSlot(layout.dragPaneChangedOrder, "DragPane", "onWidgetsChangedOrder");
   address(layout.determineWidgetUnderMouse, "?determineWidgetUnderMouse@@YA?AU?$Pair@_NPEAVWidget@agui@@@@XZ");
   address(layout.processNextDialog, "?processNextDialog@PlayerInputSource@@QEAA_NXZ");
   address(layout.playerCursorPosition,
           "?getCursorMapPosition@Player@@QEBA?AV?$Optional@VMapPosition@@U?$OptionalEmptyValue@VMapPosition@@@@@@XZ");
   address(layout.sourceCursorPosition, "?getCursorMapPosition@PlayerInputSource@@QEBA?AVMapPosition@@XZ");
   address(layout.dragBuildingUpdate, "?update@ClientDragBuildingContext@@QEAAXAEAVPlayerInputSource@@@Z");
   offset(layout.inputSourcePlayer, "PlayerInputSource", "player");
   offset(layout.inputSourceDragContext, "PlayerInputSource", "manualBuilder.dragBuildingContext");
   offset(layout.dragStartPosition, "ClientDragBuildingContext", "startPosition");
   offset(layout.dragTurnPending, "ClientDragBuildingContext", "belt.applySmartDirectionChangeWhenPossible");
   address(layout.controlInputIsActive,
           "?isActive@ControlInput@@QEBA_N_NV?$NamedBool@VGuiCheckTag@@@@0V?$NamedBool@VCheckModifiersTag@@@@@Z");
   address(layout.processZoom, "?processZoom@PlayerInputSource@@AEAA_NAEBVEvent@@@Z");
   address(layout.playerInputSourceZoom, "?zoom@PlayerInputSource@@AEAAXW4ZoomDirection@@N@Z");
   offset(layout.controlSettingsZoomIn, "ControlSettings", "zoomIn");
   offset(layout.controlSettingsZoomOut, "ControlSettings", "zoomOut");
   offset(layout.zoomTowardsCursor, "InterfaceSettings", "zoomTowardsCursor.value");
   classSlot(layout.adapterGetZoomer, "GameAdapter", "getZoomer");
   offset(layout.zoomerRate, "Zoomer", "config.zoomRate");
   address(layout.processSelectionToolCommon,
           "?processSelectionToolCommon@PlayerInputSource@@QEAA_NW4SelectionMode@@0@Z");
   address(layout.processDeselection, "?processDeselection@PlayerInputSource@@QEAA_NXZ");
   address(layout.expectedSelectionMode,
           "?expectedSelectionModeFromInputs@PlayerInputSource@@AEBA?AW4SelectionMode@@_N@Z");
   address(layout.finishSelection, "?finishSelection@PlayerInputSource@@QEAAXXZ");
   address(layout.processActions, "?processActions@PlayerInputSource@@QEAA_NAEBVEvent@@_N@Z");
   address(layout.controlTriggeredBy,
           "?triggeredBy@ControlInput@@AEBAPEBVControlInputValue@@AEBVEvent@@PEBVControlContext@@I@Z");
   offset(layout.controlSettingsToggleMenu, "ControlSettings", "toggleMenu");
   offset(layout.gameViewStartSelectionMode, "GameView", "startSelectionMode");
   offset(layout.gameViewSelectionMode, "GameView", "selectionMode");
   offset(layout.gameViewSelectionSurface, "GameView", "startSelectionSurface");
   offset(layout.gameViewSelectionPosition, "GameView", "selectionPosition");
   offset(layout.gameViewSelectionStartTime, "GameView", "selectionStartTime");
   offset(layout.editorControllerEditors, "EditorController", "editors");
   offset(layout.mapEditorsList, "MapEditors", "editors");
   offset(layout.mapEditorsActive, "MapEditors", "activeEditorType");
   enumerator(layout.editorTypeScripting, "EditorType", "Scripting");
   offset(layout.editorBaseTools, "EditorBase", "tools");
   offset(layout.editorBaseActiveTool, "EditorBase", "activeToolIndex");
   offset(layout.scriptingPositionTool, "ScriptingEditor", "positionCursorTool");
   address(layout.drawSelectionCounts, "?drawSelectionCounts@SelectionToolRenderer@@QEAAXAEAVDrawQueue@@AEBVColor@@@Z");
   offset(layout.selectionRendererCursor, "SelectionToolRenderer", "cursorPosition");
   offset(layout.selectionRendererStart, "SelectionToolRenderer", "selectionStart");
   offset(layout.selectionRendererDeconstruction, "SelectionToolRenderer", "isDeconstructionPlanner");
   offset(layout.selectionRendererCounts, "SelectionToolRenderer", "selectionCounts");
   offset(layout.countsItemsNotToBuild, "SelectionCounts", "itemCountsNotUsedToBuild");
   offset(layout.countsItemsToBuild, "SelectionCounts", "itemCountsToBuild");
   offset(layout.countsEntities, "SelectionCounts", "entityCounts");
   offset(layout.countsEntityUpgrades, "SelectionCounts", "entityUpgradeCounts");
   offset(layout.countsItemUpgrades, "SelectionCounts", "itemUpgradeCounts");
   {
      constexpr const char* kItemNode =
         "std::_Tree_node<std::pair<IDWithQuality<ID<ItemPrototype,unsigned short> > const ,unsigned int>,void *>";
      constexpr const char* kEntityNode =
         "std::_Tree_node<std::pair<IDWithQuality<ID<EntityPrototype,unsigned short> > const ,unsigned int>,void *>";
      constexpr const char* kItemUpgradeNode =
         "std::_Tree_node<std::pair<std::pair<IDWithQuality<ID<ItemPrototype,unsigned short> >,"
         "IDWithQuality<ID<ItemPrototype,unsigned short> > > const ,unsigned int>,void *>";
      constexpr const char* kEntityUpgradeNode =
         "std::_Tree_node<std::pair<std::pair<IDWithQuality<ID<EntityPrototype,unsigned short> >,"
         "IDWithQuality<ID<EntityPrototype,unsigned short> > > const ,unsigned int>,void *>";
      offset(layout.countNodeId, kItemNode, "_Myval.first");
      offset(layout.countNodeCount, kItemNode, "_Myval.second");
      offset(layout.upgradeNodeFrom, kItemUpgradeNode, "_Myval.first.first");
      offset(layout.upgradeNodeTo, kItemUpgradeNode, "_Myval.first.second");
      offset(layout.upgradeNodeCount, kItemUpgradeNode, "_Myval.second");
      offset(layout.idWithQualityBase, "IDWithQuality<ID<ItemPrototype,unsigned short> >", "baseID");
      offset(layout.idWithQualityQuality, "IDWithQuality<ID<ItemPrototype,unsigned short> >", "qualityID");
      uint32_t entityId = 0, entityCount = 0, entityFrom = 0, entityTo = 0, entityUpgradeCount = 0;
      uint32_t entityBase = 0, entityQuality = 0;
      offset(entityId, kEntityNode, "_Myval.first");
      offset(entityCount, kEntityNode, "_Myval.second");
      offset(entityFrom, kEntityUpgradeNode, "_Myval.first.first");
      offset(entityTo, kEntityUpgradeNode, "_Myval.first.second");
      offset(entityUpgradeCount, kEntityUpgradeNode, "_Myval.second");
      offset(entityBase, "IDWithQuality<ID<EntityPrototype,unsigned short> >", "baseID");
      offset(entityQuality, "IDWithQuality<ID<EntityPrototype,unsigned short> >", "qualityID");
      if (ok && (entityId != layout.countNodeId || entityCount != layout.countNodeCount ||
                 entityFrom != layout.upgradeNodeFrom || entityTo != layout.upgradeNodeTo ||
                 entityUpgradeCount != layout.upgradeNodeCount || entityBase != layout.idWithQualityBase ||
                 entityQuality != layout.idWithQualityQuality)) {
         log::error("Entity and item selection count maps no longer share a node layout");
         ok = false;
      }
   }
   address(layout.gameViewMapPosition, "?getMapPosition@GameView@@QEBA?AVMapPosition@@VPixelPosition@@@Z");
   offset(layout.inputStateMouseX, "InputState", "mouseState.x");
   offset(layout.inputStateMouseY, "InputState", "mouseState.y");
   address(layout.simpleBuildInput,
           "?getSimpleBuildInput@Player@@QEBA?AVSimpleBuildInput@@PEBVClientDragBuildingContext@@@Z");
   address(layout.prepareBuildingInGame,
           "?prepareBuildingInGame@BuildingRenderer@@AEAA?AW4ItemToBuildDrawnType@@PEBVPlayer@@AEBVMapPosition@@"
           "AEAVDrawQueue@@@Z");
   address(layout.playerBuildFromCursor,
           "?buildFromCursor@Player@@QEAA_NAEBV?$Optional@VMapPosition@@U?$OptionalEmptyValue@VMapPosition@@@@@@V?$"
           "NamedBool@VGhostModeTag@@@@PEBVClientDragBuildingContext@@@Z");
   size(layout.simpleBuildInputSize, "SimpleBuildInput");
   offset(layout.simpleBuildInputEntity, "SimpleBuildInput", "BuildID.entityID");
   offset(layout.simpleBuildInputRailPlanner, "SimpleBuildInput", "BuildID.isRailPlanner");
   offset(layout.simpleBuildInputClick, "SimpleBuildInput", "originalClickPosition");
   offset(layout.simpleBuildInputDirection, "SimpleBuildInput", "direction");
   offset(layout.simpleBuildInputPosition, "SimpleBuildInput", "position");
   offset(layout.simpleBuildInputTile, "SimpleBuildInput", "BuildID.placeAsTile");
   offset(layout.entityPrototypeFlags, "EntityPrototype", "flags");
   classSlot(layout.entityTileGridSize, "EntityPrototype", "tileGridSize");
   classSlot(layout.adapterCursorAdapter, "GameAdapter", "getCursorAdapter");
   classSlot(layout.readAdapterDestructor, "ReadAdapter", "~ReadAdapter");
   address(layout.buildableBlueprint, "?getBuildableBlueprint@ReadAdapter@@QEBAPEBVBlueprint@@XZ");
   address(layout.blueprintBuildingModifier,
           "?getBuildingModifier@Blueprint@@QEBA?AVBuildingModifier@@VMapPosition@@VDirection@@VFlip@@AEBV?$Optional@"
           "VMapPosition@@U?$OptionalEmptyValue@VMapPosition@@@@@@@Z");
   address(layout.blueprintTileBox, "?getTileBoxIgnoreSnapGrid@Blueprint@@QEBA?AVTileBox@@XZ");
   address(layout.buildingModifierTransform, "?transform@BuildingModifier@@QEBA?AVMapPosition@@V2@@Z");
   size(layout.buildingModifierSize, "BuildingModifier");
   offset(layout.buildingModifierCentre, "BuildingModifier", "afterRotationShift");
   offset(layout.blueprintRotation, "Blueprint", "rotation");
   offset(layout.blueprintFlip, "Blueprint", "flip");
   offset(layout.blueprintSnapToGrid, "Blueprint", "snapToGrid._Has_value");
   offset(layout.blueprintGridSize, "Blueprint", "snapToGrid._Value");
   offset(layout.blueprintAbsoluteSnapping, "Blueprint", "positionRelativeToGrid._Has_value");
   address(layout.playerBuildId, "?getBuildID@Player@@QEBA?AVBuildID@@XZ");
   size(layout.buildIdSize, "BuildID");
   offset(layout.buildIdEntity, "BuildID", "entityID");
   offset(layout.buildIdTile, "BuildID", "placeAsTile");
   offset(layout.buildIdRailPlanner, "BuildID", "isRailPlanner");
   offset(layout.gameViewEntityMirrored, "GameView", "entityMirrored");
   offset(layout.entityFlipping, "EntityPrototype", "flipping");
   address(layout.settingsDraw, "?draw@EntityToBeBuiltSettings@@QEBAXAEAVDrawQueue@@PEBVEntity@@@Z");
   address(layout.drawBlueprintEntities,
           "?drawEntitiesCore@DrawBlueprintToBeBuiltLogic@@AEAAXAEAVBlueprintEntities@@@Z");
   offset(layout.blueprintEntitiesData, "BlueprintEntities", "data");
   address(layout.settingsBuildCheckData, "?getBuildCheckData@EntityToBeBuiltSettings@@AEBA?AVBuildCheckData@@XZ");
   address(layout.buildCheckMessage, "?getMessage@BuildCheckResult@@QEBA?AVLocalisedString@@XZ");
   classSlot(layout.adapterBuildabilityCheck, "GameAdapter", "entityBuildabilityCheck");
   size(layout.buildCheckDataSize, "BuildCheckData");
   size(layout.buildCheckResultSize, "BuildCheckResult");
   offset(layout.buildCheckResultType, "BuildCheckResult", "type");
   offset(layout.buildCheckResultEntity, "BuildCheckResult", "entity");
   offset(layout.entityPosition, "Entity", "position");
   offset(layout.entityPrototypeOf, "Entity", "prototype");
   offset(layout.settingsTooFar, "EntityToBeBuiltSettings", "tooFar");
   offset(layout.settingsBlueprint, "EntityToBeBuiltSettings", "blueprint");
   offset(layout.settingsPlayer, "EntityToBeBuiltSettings", "player");
   address(
      layout.renderCursorBox,
      "?renderCursorBox@RenderUtil@@YAXW4CursorBoxType@1@VBoundingBox@@AEAVDrawQueue@@W4Enum@RenderLayer@@CNVColor@@"
      "@Z");
   address(layout.renderDoubleCursorBox,
           "?renderDoubleCursorBox@RenderUtil@@YAXW4CursorBoxType@1@AEBVBoundingBox@@1AEAV"
           "DrawQueue@@W4Enum@RenderLayer@@VColor@@@Z");
   address(layout.adapterRenderCursorBox, "?renderCursorBox@DrawAdapter@@AEBAXAEBVEntity@@V?$NamedBool@"
                                          "VSkipSurfaceCheckTag@@@@W4CursorBoxType@RenderUtil@@@Z");
   address(layout.adapterDestroy, "?destroy@DrawAdapter@@UEBAXPEAVEntity@@@Z");
   address(layout.adapterSetDirection, "?setDirectionAndMirroring@DrawAdapter@@UEBA?AVActionResult@@PEAVEntity@@"
                                       "VDirection@@V?$NamedBool@VMirroringTag@@@@@Z");
   address(layout.drawPoleConnections,
           "?drawPoleConnections@ElectricEnergySource@@SAXAEAVDrawQueue@@AEBVSurface@@AEBVBoundingBox@@@Z");
   address(layout.adapterOrderDeconstruction, "?orderDeconstruction@DrawAdapter@@UEBA_NPEAVEntity@@@Z");
   address(
      layout.drawTileToBeBuilt,
      "?drawTileToBeBuilt@BuildingRenderer@@SAXAEBVTileCheckData@@VTilePosition@@PEBVPlaceAsTile@@AEBVTilePrototype@@"
      "V?$NamedBool@VTooFarTag@@@@V?$NamedBool@VGhostModeTag@@@@_N6AEAVDrawQueue@@W4BuildMode@@PEBVDrawAdapter@@@Z");
   address(layout.tileCheckBuildability,
           "?checkBuildability@TileCheckData@@QEBA?AVBuildCheckResult@@AEBVTilePrototype@@PEBVPlaceAsTile@@"
           "AEBVTilePosition@@"
           "V?$NamedBool@VGhostModeTag@@@@_NW4BuildMode@@V?$NamedBool@VCheckCoverabilityTag@@@@@Z");
   offset(layout.settingsAddedWires, "EntityToBeBuiltSettings", "wiresInPreview.addedWires");
   size(layout.wireSize, "Wire");
   offset(layout.wireSource, "Wire", "source.entity");
   offset(layout.wireTarget, "Wire", "target.entity");
   address(layout.roboportPostPrepare, "?postPrepare@RoboportInfoRenderer@@QEAAXAEBV?$vector@PEAVDrawHelper@@V?$"
                                       "allocator@PEAVDrawHelper@@@std@@@std@@@Z");
   address(layout.drawOnTilesBetween, "?drawOnTilesBetween@RenderUtil@@YAXAEAVDrawQueue@@AEBVSprite@@AEBVMapPosition@@"
                                      "2AEBVRealOrientation@@W4Enum@RenderLayer@@VColor@@@Z");
   address(layout.entitySelectionBox, "?getSelectionBox@Entity@@UEBA?AVBoundingBox@@AEBVSelectionContext@@@Z");
   address(layout.iteratorStartTile, "?startAdvancedTile@?$HeuristicEntityIterator@$$CBVSurface@@@@AEAAXXZ");
   address(layout.iteratorMove, "?moveUntilEntityFound@?$HeuristicEntityIterator@$$CBVSurface@@@@AEAAXXZ");
   size(layout.iteratorSize, "HeuristicEntityIterator<Surface const >");
   offset(layout.iteratorSurface, "HeuristicEntityIterator<Surface const >", "surface");
   offset(layout.iteratorLeftTop, "HeuristicEntityIterator<Surface const >", "leftTop");
   offset(layout.iteratorRightBottom, "HeuristicEntityIterator<Surface const >", "rightBottom");
   offset(layout.iteratorCurrentTile, "HeuristicEntityIterator<Surface const >", "currentAdvancedTilePosition");
   offset(layout.iteratorCurrentEntity, "HeuristicEntityIterator<Surface const >", "currentEntity");
   offset(layout.entitySurface, "Entity", "surface");
   offset(layout.mapSurfaces, "Map", "surfaces");
   offset(layout.surfaceIndex, "Surface", "index");
   offset(layout.surfaceChunks, "Surface", "chunks");
   offset(layout.chunkPosition, "Chunk", "position");
   address(layout.surfaceChunkAt, "?getChunkSafe@Surface@@QEBAPEBVChunk@@AEBVChunkPosition@@@Z");
   offset(layout.chunkTiles, "Chunk", "tiles");
   size(layout.tileSize, "Tile");
   address(layout.surfaceTileAt, "?getTileOptional@Surface@@QEBAPEBVTile@@AEBVTilePosition@@@Z");
   offset(layout.tileCollisionLayers, "TilePrototype", "collisionMask");
   offset(layout.tileFluid, "TilePrototype", "fluid.id");
   address(layout.collisionLayerNames,
           "?nameToPrototype@?$PrototypeList@VCollisionLayerPrototype@@@@0V?$map@V?$basic_string@DU?$char_traits@D@"
           "std@@V?$allocator@D@2@@std@@PEAVCollisionLayerPrototype@@U?$less@X@2@V?$allocator@U?$pair@$$CBV?$"
           "basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@PEAVCollisionLayerPrototype@@@std@@@2@@std@@A");
   offset(layout.collisionLayerMask, "CollisionLayerPrototype", "layerMask");
   offset(layout.collisionLayerRow, "CollisionLayerPrototype", "layerRow");
   offset(layout.simpleEntityRock, "SimpleEntityPrototype", "countAsRockForFilteredDeconstruction");
   offset(layout.resourceSearchRadius, "ResourceEntityPrototype", "resourcePatchSearchRadius");
   offset(layout.resourceInfinite, "ResourceEntityPrototype", "infiniteType");
   classSlot(layout.prototypeGetType, "PrototypeBase", "getType");
   address(layout.entityTargeterVtable, "??_7?$Targeter@VEntity@@$0A@$0A@@@6B@");
   offset(layout.targetableTargeters, "Targetable", "targetingMe.firstTargeter");
   offset(layout.surfacePlatform, "Surface", "platform");
   offset(layout.platformAsteroidChunks, "SpacePlatform", "asteroidChunks");
   size(layout.asteroidChunkSize, "AsteroidChunk");
   offset(layout.asteroidChunkPrototype, "AsteroidChunk", "prototypeID");
   offset(layout.asteroidChunkPosition, "AsteroidChunk", "position");
   address(layout.asteroidChunkTargeterVtable, "??_7?$Targeter@VAsteroidChunk@@$0A@$0A@@@6B@");
   offset(layout.entityUsageBits, "Entity", "usageBitMask");
   offset(layout.scanAssemblerRecipe, "AssemblingMachine", "recipeID");
   offset(layout.scanFurnaceRecipe, "Furnace", "recipeID");
   offset(layout.scanFurnaceResult, "Furnace", "resultInventory");
   offset(layout.scanDrillResources, "MiningDrill", "resourcesToMine");
   size(layout.resourceTargeterSize, "Targeter<ResourceEntity,33,0>");
   offset(layout.resourceAmount, "ResourceEntity", "resourceAmount");
   offset(layout.resourceStageCounts, "ResourceEntityPrototype", "stageCounts");
   offset(layout.scanLocomotiveTrain, "Locomotive", "train");
   offset(layout.scanCargoWagonTrain, "CargoWagon", "train");
   offset(layout.scanFluidWagonTrain, "FluidWagon", "train");
   offset(layout.scanArtilleryWagonTrain, "ArtilleryWagon", "train");
   offset(layout.trainId, "Train", "id");
   offset(layout.scanGhostInner, "EntityGhost", "innerEntity");
   offset(layout.scanSpawnerPollution, "EnemySpawner", "absorbedPollution");
   offset(layout.scanContainerInventory, "ContainerEntity", "inventory");
   offset(layout.scanLogisticInventory, "LogisticContainer", "inventory");
   offset(layout.scanInfinityInventory, "InfinityContainer", "inventory");
   offset(layout.scanRoboportName, "Roboport", "backerName");
   offset(layout.scanPipeFluidBox, "Pipe", "fluidBox");
   offset(layout.scanInfinityPipeFluidBox, "InfinityPipe", "fluidBox");
   offset(layout.scanUndergroundFluidBox, "PipeToGround", "fluidBox");
   offset(layout.scanTankFluidBox, "StorageTank", "fluidBox");
   offset(layout.fluidBoxSegment, "FluidBox", "fluidSegment");
   offset(layout.fluidBoxFluid, "FluidBox", "buffer.fluid.fluidID");
   offset(layout.fluidBoxAmount, "FluidBox", "buffer.fluid.amount");
   offset(layout.segmentFluid, "FluidSegment", "buffer.fluid.fluidID");
   offset(layout.segmentAmount, "FluidSegment", "buffer.fluid.amount");
   offset(layout.fluidBoxConnectionsBegin, "FluidBox", "connections.begin_");
   offset(layout.fluidBoxConnectionsEnd, "FluidBox", "connections.end_");
   size(layout.fluidConnectionSize, "FluidBoxConnection");
   offset(layout.fluidConnectionTarget, "FluidBoxConnection", "target");
   address(layout.forceIsChunkCharted, "?isChunkCharted@ForceData@@QEBA_NVSurfaceIndex@@AEBVMapPosition@@@Z");
   address(layout.forceChart, "?getChart@ForceData@@QEBAPEBVChart@@VSurfaceIndex@@@Z");
   address(layout.chartChunkCovered, "?isChunkCoveredByFogOfWar@Chart@@QEBA_NAEBVChunkPosition@@@Z");
   offset(layout.chartColumns, "Chart", "chartedChunks.data");
   offset(layout.infiniteData, "InfiniteVector<SubChart *>", "data");
   offset(layout.infiniteSize, "InfiniteVector<SubChart *>", "dataSize");
   offset(layout.infiniteZero, "InfiniteVector<SubChart *>", "zeroIndex");
   size(layout.infiniteVectorSize, "InfiniteVector<SubChart *>");
   offset(layout.subChartPixels, "SubChart", "pixels");
   offset(layout.tilePrototypeMapColor, "TilePrototype", "mapColor");
   offset(layout.entityPrototypeMapColor, "EntityPrototype", "mapColor");
   offset(layout.entityPrototypeFriendlyMapColor, "EntityPrototype", "friendlyMapColor");
   offset(layout.entityPrototypeEnemyMapColor, "EntityPrototype", "enemyMapColor");
   offset(layout.globalUtilityConstants, "GlobalContext", "utilityConstants.value");
   offset(layout.chartDefaultFriendlyColor, "UtilityConstants", "chartDefaultFriendlyColor");
   offset(layout.chartDefaultEnemyColor, "UtilityConstants", "chartDefaultEnemyColor");
   offset(layout.chartEntityGhostColor, "UtilityConstants", "chartEntityGhostColor");
   offset(layout.chartTileGhostColor, "UtilityConstants", "chartTileGhostColor");
   offset(layout.prototypeOrder, "PrototypeBase", "orderString");
   offset(layout.prototypeHidden, "PrototypeBase", "hidden");
   classSlot(layout.entityGetForceId, "Entity", "getForceID");
   offset(layout.displayPanelShowInChart, "DisplayPanel", "showInChart");
   classSlot(layout.entityPrototypeAsPole, "EntityPrototype", "asElectricPole");
   address(layout.findMatchingNetwork,
           "?findMatchingNetworkByPosition@LogisticManager@@QEAAPEAVLogisticNetwork@@AEBVMapPosition@@@Z");
   offset(layout.gameViewRenderer, "GameView", "renderer");
   offset(layout.gameRendererParameters, "GameRenderer", "renderParameters");
   offset(layout.renderParametersFlags, "RenderParameters", "flags");
   size(layout.drawQueueSize, "DrawQueue");
   offset(layout.drawQueueRenderParameters, "DrawQueue", "renderParameters");
   address(layout.drawQueueConstruct, "??0DrawQueue@@QEAA@AEBVRenderParameters@@@Z");
   address(layout.drawQueueClear, "?clear@DrawQueue@@QEAAXXZ");
   offset(layout.drawQueueSprites, "DrawQueue", "sprites");
   offset(layout.drawQueueSpritesEnd, "DrawQueue", "gradientLights");
   offset(layout.drawQueueCorpses, "DrawQueue", "decayingCorpses");
   offset(layout.drawQueueOthers, "DrawQueue", "others");
   offset(layout.drawQueueOthersUnder, "DrawQueue", "othersUnderEntities");
   offset(layout.drawQueueOthersAbove, "DrawQueue", "othersAbove");
   size(layout.spriteOrdersSize, "SpriteDrawOrderVector");
   offset(layout.spriteOrdersUsed, "SpriteDrawOrderVector", "used_size");
   offset(layout.spriteOrdersData, "SpriteDrawOrderVector", "stored_data");
   size(layout.spriteOrderPairSize, "Pair<unsigned __int64,SpriteDrawOrder *>");
   offset(layout.spriteOrderPairOrder, "Pair<unsigned __int64,SpriteDrawOrder *>", "second");
   offset(layout.spriteOrderWidth, "SpriteDrawOrder", "sourceWidth");
   offset(layout.spriteOrderHeight, "SpriteDrawOrder", "sourceHeight");
   classSlot(layout.entityDraw, "Entity", "draw");
   address(layout.entityDrawAlert, "?drawAlert@Entity@@QEBAXAEAVDrawQueue@@AEBVSprite@@AEBVMapPosition@@_N@Z");
   offset(layout.globalUtilitySprites, "GlobalContext", "utilitySprites.value");
   size(layout.utilitySpritesSize, "UtilitySprites");
   offset(layout.utilitySpritesMapping, "UtilitySprites", "spritesMapping");
   offset(layout.checkMarkSprite, "UtilitySprites", "checkMark");
   address(layout.drawInfoIcon,
           "?drawInfoIcon@DrawQueue@@QEAAXPEBVSprite@@VQualityCondition@@AEBVMapPosition@@NVDrawingFlags@@"
           "W4Enum@RenderLayer@@AEBVVector@@CVColor@@@Z");
   offset(layout.qualityDrawByDefault, "QualityPrototype", "drawSpriteByDefault");
   address(layout.comparisonStr, "?str@Comparison@@QEBAPEBDXZ");
   offset(layout.logisticNetworkId, "LogisticNetwork", "networkID");
   offset(layout.logisticNetworkName, "LogisticNetwork", "networkName.value");
   address(layout.initLuaState, "?initLuaState@LuaHelper@@YAXPEAUlua_State@@@Z");
   address(layout.luaParamEntity,
           "??$getParamOrDefault@PEAVEntity@@@LuaHelper@@YAPEAVEntity@@PEAUlua_State@@HPEBDPEAV1@@Z");
   address(layout.addLocalFlyingText, "?addLocalFlyingText@Map@@QEAAX$$QEAVLocalMapFlyingText@@@Z");
   address(layout.constructGuiFlyingText,
           "??$construct@VGuiFlyingText@agui@@AEAVPoint@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@"
           "std@@AEAVColor@2@AEAPEBVFont@2@AEAHAEAH@?$_Default_allocator_traits@V?$allocator@VGuiFlyingText@agui@@@"
           "std@@@std@@SAXAEAV?$allocator@VGuiFlyingText@agui@@@1@QEAVGuiFlyingText@agui@@AEAVPoint@4@AEBV?$basic_"
           "string@DU?$char_traits@D@std@@V?$allocator@D@2@@1@AEAVColor@4@AEAPEBVFont@4@AEAH6@Z");
   offset(layout.localMapFlyingTextText, "LocalMapFlyingText", "text");
   offset(layout.localMapFlyingTextSurface, "LocalMapFlyingText", "startingPosition.surfaceIndex");
   offset(layout.localMapFlyingTextPosition, "LocalMapFlyingText", "startingPosition.position");
   address(layout.outputConsoleAdd,
           "?add@OutputConsole@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@VColor@@"
           "AEBVLocalisedString@@PEBVPlayer@@AEBUPrintSettings@@$$QEAV?$vector@VSavedSpecialItemReference@@V?$"
           "allocator@VSavedSpecialItemReference@@@std@@@3@@Z");
   offset(layout.outputConsoleOwner, "OutputConsole", "owner");
   offset(layout.outputConsoleItems, "OutputConsole", "items");
   offset(layout.outputConsoleItemsNotSaved, "OutputConsole", "itemsNotPartOfGameState");
   address(layout.tipNotificationButton,
           "??0TipsAndTricksNotificationButton@@QEAA@AEBVTipsAndTricksItem@@VGuiContext@@@Z");
   address(layout.speechBubbleGui,
           "??0SpeechBubbleGui@@AEAA@AEAVSpeechBubble@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@"
           "std@@AEAVGameView@@PEBVFlowStyle@agui@@PEAVSpeechBubbleStyle@@@Z");
   address(layout.infoBoxManagerUpdate, "?update@InfoBoxManager@@QEAAXXZ");
   offset(layout.infoBoxManagerFrame, "InfoBoxManager", "frame");
   offset(layout.infoBoxManagerRebuild, "InfoBoxManager", "rebuildConnectors");
   offset(layout.infoBoxManagerConnectors, "InfoBoxManager", "connectors");
   address(layout.characterChangePosition, "?changePosition@Character@@UEAA_NAEBVVector@@@Z");
   offset(layout.characterPosition, "Character", "position");
   offset(layout.characterMap, "Character", "map");
   offset(layout.characterController, "Character", "controller");
   offset(layout.controllerPlayer, "CharacterController", "player");
   offset(layout.mapUpdateTick, "Map", "updateTick");
   offset(layout.playerMap, "Player", "map");
   offset(layout.labelStyleParent, "agui::Label", "style.parent");
   offset(layout.globalStyle, "GlobalContext", "style.value");
   offset(layout.guiStyleBoldRedLabel, "GuiStyle", "_boldRedLabel");
   offset(layout.guiStyleBoldOrangeLabel, "GuiStyle", "_boldOrangeLabel");
   offset(layout.integratedLabelStyleAgui, "IntegratedStyle<LabelStyleSpecification>", "agui");
   offset(layout.buttonStyleParent, "agui::Button", "style.parent");
   offset(layout.guiStyleCompactRedSlot, "GuiStyle", "_compactRedSlot");
   offset(layout.integratedButtonStyleAgui, "IntegratedStyle<ButtonStyleSpecification>", "agui");
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
   address(layout.luaPushByte, "??$lua_pushnumber@E@@YAXPEAUlua_State@@E@Z");
   address(layout.luaPushInt, "??$lua_pushnumber@H@@YAXPEAUlua_State@@H@Z");
   address(layout.luaPushNumber, "lua_pushnumber");
   address(layout.luaPushBoolean, "lua_pushboolean");
   address(layout.luaGetField, "lua_getfield");
   address(layout.luaRawGetI, "lua_rawgeti");
   address(layout.luaType, "lua_type");
   address(layout.luaToNumberX, "lua_tonumberx");
   address(layout.luaToBoolean, "lua_toboolean");
   address(layout.luaToLString, "lua_tolstring");
   address(layout.luaRawLen, "lua_rawlen");
   address(layout.parseLocalisedString, "?parseLocalisedString@LuaHelper@@YA?AVLocalisedString@@PEAUlua_State@@H_N@Z");
   address(layout.localisedStringDestroy, "??1LocalisedString@@QEAA@XZ");
   size(layout.localisedStringSize, "LocalisedString");

   offset(layout.globalGui, "GlobalContext", "gui");
   offset(layout.globalGame, "GlobalContext", "game");
   offset(layout.globalAppManager, "GlobalContext", "appManager");
   offset(layout.globalPlayerInputSource, "GlobalContext", "playerInputSource");
   offset(layout.globalInputState, "GlobalContext", "inputState.value");
   offset(layout.globalControlSettings, "GlobalContext", "controlSettings.value");
   offset(layout.inputStateMouseButtons, "InputState", "mouseState.buttons");
   offset(layout.inputStateMouseBlocks, "InputState", "mouseBlocks");
   size(layout.mouseBlockSize, "InputState::MouseBlock");
   offset(layout.appManagerStates, "AppManager", "stateStack");
   offset(layout.appStateGui, "AppManagerStateWithGuiManualConstruction<GameMenuGui>", "gui");
   offset(layout.gameView, "Game", "gameView");
   offset(layout.gameViewPlayer, "GameView", "player");
   offset(layout.gameLocalPlayer, "Game", "localPlayer");
   offset(layout.playerIndex, "Player", "index");
   offset(layout.gameViewMessage, "GameView", "scenarioMessageDialog");
   offset(layout.gameViewBuildDirection, "GameView", "buildDirection");
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
   slot(layout.slotClearFocus, "clearFocus");
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
   offset(layout.labeledSwitchSwitch, "LabeledSwitch", "switchWidget");
   offset(layout.labeledSwitchLeft, "LabeledSwitch", "leftValueLabel");
   offset(layout.labeledSwitchRight, "LabeledSwitch", "rightValueLabel");
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
   offset(layout.tableOrderingColumn, "agui::TableWithSelection", "columnOrderingIndex");
   offset(layout.tableHeaders, "agui::TableWithSelection", "headers");
   size(layout.tableHeaderSize, "agui::TableWithSelection::Header");
   offset(layout.tableHeaderAscending, "agui::TableWithSelection::Header", "ascending");

   // Every Dialog<Result> instantiation lays its members out alike.
   offset(layout.dialogButtons, "GuiTemplate", "Dialog<enum ConfirmCancelResult>.bottomButtonsFlow");
   offset(layout.dialogConfirmValue, "GuiTemplate", "Dialog<enum ConfirmCancelResult>.confirmValue");
   offset(layout.dialogResultMapping, "GuiTemplate", "Dialog<enum ConfirmCancelResult>.resultMapping");
   offset(layout.dialogRightButtons, "GuiTemplate", "Dialog<enum ConfirmCancelResult>.rightButtons");
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
   offset(layout.saveMapLeftPart, "SaveMapGui", "leftPart");
   offset(layout.saveMapNameField, "SaveMapGui", "mapNameField");
   offset(layout.saveMapList, "SaveMapGui", "packageListGui");
   offset(layout.saveMapInfo, "SaveMapGui", "mapInfoGui");
   offset(layout.modsTabs, "ModsGui", "tabs");
   offset(layout.modsManageTab, "ModsGui", "manageTab");
   offset(layout.modsManagePane, "ModsGui", "manageTabContents");
   offset(layout.manageModsTable, "ManageModsPane", "modsTable");
   offset(layout.manageModsInfo, "ManageModsPane", "modInfoPane");
   offset(layout.manageModsSearch, "ManageModsPane", "searchBar");
   offset(layout.modsExploreTab, "ModsGui", "exploreTab");
   offset(layout.modsExplorePane, "ModsGui", "exploreTabContents");
   offset(layout.exploreFilters, "ExploreModsPane", "filtersTable");
   offset(layout.exploreResults, "ExploreModsPane", "resultsTable");
   offset(layout.exploreResultsCount, "ExploreModsPane", "resultsCountLabel");
   offset(layout.explorePages, "ExploreModsPane", "paginationWidget");
   offset(layout.exploreModInfo, "ExploreModsPane", "modInfoPane");
   offset(layout.modsDownloads, "ModsGui", "downloadModsPane");
   offset(layout.modsUpdatesTab, "ModsGui", "updatesTab");
   offset(layout.modsUpdatesPane, "ModsGui", "updatesTabContents");
   offset(layout.updatesTable, "UpdateModsPane", "updatesTable");
   offset(layout.updatesStatus, "UpdateModsPane", "statusPane");
   offset(layout.updatesModInfo, "UpdateModsPane", "modInfoPane");
   offset(layout.modInfoName, "ModInfoPane", "modName");
   offset(layout.modInfoBookmark, "ModInfoPane", "bookmarkButton");
   offset(layout.modInfoInstall, "ModInfoPane", "installButton");
   offset(layout.modInfoChangelog, "ModInfoPane", "changelogButton");
   offset(layout.modInfoDelete, "ModInfoPane", "deleteButton");
   offset(layout.modInfoDescription, "ModInfoPane", "modDescription");
   offset(layout.modInfoTable, "ModInfoPane", "modInfoTable");
   offset(layout.searchFilterHits, "SearchFilterWidgets", "hitCountLabel");
   offset(layout.searchFilterExclude, "SearchFilterWidgets", "excludeButton");
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
   address(layout.localisedStringStr, "?str@LocalisedString@@QEBAAEBV?$basic_string@DU?$char_traits@D@std@@V?$"
                                      "allocator@D@2@@std@@PEBVLocaleProvider@@@Z");
   offset(layout.prototypeName, "PrototypeBase", "name");

   offset(layout.slotInventory, "InventoryGuiSlot", "inventory");
   offset(layout.slotIndex, "InventoryGuiSlot", "targetSpecification.slotIndex");
   offset(layout.slotItemStack, "InventoryGuiSlot", "itemStack");
   offset(layout.slotEmptyInfo, "IDButton<IDWithQualityFilter<ID<ItemPrototype,unsigned short> >,ItemStack>",
          "emptySlotInfo");
   offset(layout.emptySlotCaption, "EmptySlotInfo", "caption");
   offset(layout.slotGhostItem, "InventoryGuiSlot", "ghostItem.id.baseID");
   offset(layout.slotGhostQuality, "InventoryGuiSlot", "ghostItem.id.qualityID");
   offset(layout.slotGhostCount, "InventoryGuiSlot", "ghostItem.count");
   offset(layout.slotGhostRemoval, "InventoryGuiSlot", "ghostRemoval");
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
   address(layout.entityPrototypes,
           "?indexToPrototype@?$PrototypeList@VEntityPrototype@@@@2V?$vector@PEAVEntityPrototype@@V?$allocator@"
           "PEAVEntityPrototype@@@std@@@std@@A");
   offset(layout.recipeListSlots, "SelectListGui<ID<RecipePrototype,unsigned short> >", "slots");
   address(layout.qualityPrototypes,
           "?indexToPrototype@?$PrototypeList@VQualityPrototype@@@@2V?$vector@PEAVQualityPrototype@@V?$allocator@"
           "PEAVQualityPrototype@@@std@@@std@@A");
   address(layout.recipePrototypes,
           "?indexToPrototype@?$PrototypeList@VRecipePrototype@@@@2V?$vector@PEAVRecipePrototype@@V?$allocator@"
           "PEAVRecipePrototype@@@std@@@std@@A");
   for (size_t i = 0; i < kNamedPrototypeCount; ++i) {
      constexpr std::string_view kString = "V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@";
      std::string type = kNamedPrototypes[i].type;
      std::string name = std::format("?nameToPrototype@?$PrototypeList@V{0}@@@@0V?$map@{1}PEAV{0}@@U?$less@X@2@V?$"
                                     "allocator@U?$pair@$$CB{1}PEAV{0}@@@std@@@2@@std@@A",
                                     type, kString);
      address(layout.prototypeNames[i], name.c_str());
   }

   classSlot(layout.providerBasePrototype, "PrototypeProvider", "getBasePrototype");
   classSlot(layout.providerQualityPrototype, "PrototypeProvider", "getQualityPrototype");
   classSlot(layout.buttonNumberCount, "ButtonNumber", "getCount");
   classSlot(layout.buttonNumberRoundType, "ButtonNumber", "getRoundType");
   address(layout.shortNumberFormat,
           "?shortNumberFormat@StringUtil@@YA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@"
           "NW4Compactness@1@W4UseDecimal@1@W4Rounding@@W4NumDecimals@1@@Z");
   enumerator(layout.compactnessCompact, "StringUtil::Compactness", "Compact");
   enumerator(layout.useDecimalAuto, "StringUtil::UseDecimal", "Auto");
   enumerator(layout.numDecimalsOne, "StringUtil::NumDecimals", "One");
   offset(layout.progressBarValue, "agui::ProgressBar", "value");

   offset(layout.entityMainWindow, "GameGuiWithControllerInventory", "mainWindow");
   offset(layout.entityInventoryHolder, "GameGuiWithControllerInventory", "controllerInventory");
   offset(layout.holderInventory, "GameControllerInventoryHolder", "inventoryGui");
   offset(layout.holderTitle, "GameControllerInventoryHolder", "titleLabel");
   offset(layout.remoteHolderSelect, "RemoteControllerInventoryHolder", "selectGui");
   offset(layout.itemSelectListSubheader, "SelectListGui<ID<ItemPrototype,unsigned short> >", "subheader");
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
   offset(layout.deconItemPart, "DeconstructionItemGui", "itemPart");
   offset(layout.deconItemPartName, "DeconstructionItemGui::ItemPartHelper", "nameLabel");
   offset(layout.frameSubheader, "FrameWithSubheader", "subheader");
   offset(layout.deconDescription, "DeconstructionItemGui", "descriptionLabel");
   offset(layout.deconTreesAndRocks, "DeconstructionItemGui", "treesAndRocksOnlyCheckbox");
   offset(layout.deconEntityTab, "DeconstructionItemGui", "entityTab");
   offset(layout.deconTileTab, "DeconstructionItemGui", "tileTab");
   offset(layout.deconEntityMode, "DeconstructionItemGui", "entityFiltersWidgets.modeSwitch");
   offset(layout.deconEntityFilters, "DeconstructionItemGui", "entityFiltersWidgets.table");
   offset(layout.deconTileMode, "DeconstructionItemGui", "tileFiltersWidgets.modeSwitch");
   offset(layout.deconTileSelection, "DeconstructionItemGui", "tileFiltersWidgets.tileModeDropdown");
   offset(layout.deconTileFilters, "DeconstructionItemGui", "tileFiltersWidgets.table");
   offset(layout.upgradeItemFrame, "UpgradeItemGui", "upgradeItemFrame");
   offset(layout.upgradeItemName, "UpgradeItemGui::UpgradeItemFrame", "nameLabel");
   offset(layout.upgradeDescription, "UpgradeItemGui", "descriptionLabel");
   offset(layout.upgradeRules, "UpgradeItemGui", "mappersWidgets.table");
   offset(layout.blueprintSettings, "BlueprintSetupGui", "blueprintSettingsGui");
   offset(layout.blueprintPreview, "BlueprintSetupGui", "blueprintPreview");
   offset(layout.blueprintName, "BlueprintSettingsGui", "labelEdit");
   offset(layout.blueprintScroll, "BlueprintSettingsGui", "verticalScroll");
   offset(layout.blueprintDescription, "BlueprintSettingsGui", "descriptionEdit");
   offset(layout.blueprintSnapCheckbox, "BlueprintSettingsGui", "snapToGridCheckbox");
   offset(layout.blueprintGridWidth, "BlueprintSettingsGui", "snapToGridX");
   offset(layout.blueprintComponents, "BlueprintSettingsGui", "componentsTable");
   {
      const char* include[] = {"includeEntitiesCheckbox",     "includeModulesCheckbox", "includeTilesCheckbox",
                               "includeStationNamesCheckbox", "includeTrainsCheckbox",  "includeFuelCheckbox",
                               "includeVehiclesCheckbox"};
      for (int i = 0; i < 7; ++i) offset(layout.blueprintInclude[i], "BlueprintSettingsGui", include[i]);
   }
   offset(layout.editableLabelText, "EditableLabel", "label");
   offset(layout.editableLabelField, "EditableLabel", "labelEdit");
   offset(layout.editableLabelButton, "EditableLabel", "switchEditLabelMode");

   offset(layout.pictureBlueprint, "BlueprintWidget", "blueprint");
   offset(layout.pictureParameters, "BlueprintWidget", "blueprintParameters");
   offset(layout.pictureSelection, "BlueprintWidget", "blueprintSelection");
   offset(layout.pictureEditEnabled, "BlueprintWidget", "editEnabled");
   offset(layout.pictureRenderParameters, "BlueprintWidget", "renderParameters");
   offset(layout.pictureScale, "BlueprintWidget", "renderParameters.scale");
   offset(layout.pictureViewLeftTop, "BlueprintWidget", "renderParameters.boundingBox.leftTop");
   offset(layout.picturePlayer, "BlueprintWidget", "context.player");
   classSlot(layout.adapterShowEntityInfo, "GameAdapter", "getShowEntityInfo");
   address(layout.blueprintSelectionAt, "?selectionFromPosition@Blueprint@@QEBA?AVBlueprintSelectionResult@@"
                                        "AEBVMapPosition@@AEBVSetupBlueprintParameters@@@Z");
   address(layout.picturePixelShift, "?getPixelShift@BlueprintWidget@@AEBA?AVPixelPosition@@AEBVPoint@agui@@@Z");
   size(layout.selectionResultSize, "BlueprintSelectionResult");
   offset(layout.selectionEntity, "BlueprintSelectionResult", "entity");
   offset(layout.selectionTile, "BlueprintSelectionResult", "tileID");
   offset(layout.selectionIndex, "BlueprintSelectionResult", "index");
   offset(layout.parametersEntities, "SetupBlueprintParameters", "entitiesData");
   offset(layout.parametersTiles, "SetupBlueprintParameters", "tilesData");
   offset(layout.parametersIncludeTiles, "SetupBlueprintParameters", "includeTiles");
   offset(layout.blueprintTiles, "Blueprint", "tiles");
   address(layout.blueprintTileAt, "?selectionFromPosition@BlueprintTiles@@QEBA?AVBlueprintSelectionResult@@"
                                   "AEBVMapPosition@@@Z");
   offset(layout.blueprintEntityList, "Blueprint", "entities.data");
   size(layout.entityDataSize, "BlueprintEntities::EntityData");
   offset(layout.entityDataEntity, "BlueprintEntities::EntityData", "entity");
   offset(layout.entityDataInsertPlan, "BlueprintEntities::EntityData", "insertPlan.data.data");
   size(layout.insertPairSize, "Pair<IDWithQuality<ID<ItemPrototype,unsigned short> >,ItemInventoryPositions>");
   offset(layout.insertPairPositions, "Pair<IDWithQuality<ID<ItemPrototype,unsigned short> >,ItemInventoryPositions>",
          "second");
   offset(layout.positionsGridCount, "ItemInventoryPositions", "gridCount");
   offset(layout.positionsStacks, "ItemInventoryPositions", "inventoryPositions");
   size(layout.stackLocationSize, "ItemStackLocationWithCount");
   offset(layout.stackLocationCount, "ItemStackLocationWithCount", "count");
   classSlot(layout.entityGetDirection, "Entity", "getDirection");
   classSlot(layout.entityHasDirection, "Entity", "hasDirection");
   offset(layout.entityQuality, "EntityWithOwner", "qualityID");
   offset(layout.craftingRecipe, "CraftingMachine", "recipeID");
   offset(layout.mapForces, "Map", "forceManager.sortedForceDataList.begin_");
   size(layout.itemFilterSize, "IDWithQualityFilter<ID<ItemPrototype,unsigned short> >");
   offset(layout.itemFilterId, "IDWithQualityFilter<ID<ItemPrototype,unsigned short> >", "baseID");
   offset(layout.inserterFilters, "Inserter", "filter");
   offset(layout.splitterLogic, "Splitter", "leftLogic");
   offset(layout.laneSplitterLogic, "LaneSplitter", "logic");
   offset(layout.splitterInputLocked, "SplitterLogic", "inputLocked");
   offset(layout.splitterOutputLocked, "SplitterLogic", "outputLocked");
   offset(layout.splitterTakeFrom, "SplitterLogic", "takeNextItemFrom");
   offset(layout.splitterGoesTo, "SplitterLogic", "nextItemGoesTo");
   offset(layout.splitterFilter, "SplitterLogic", "filter");
   enumerator(layout.splitterRight, "SplitterDirection", "Right");
   offset(layout.splitterSecondaryLeftLine, "Splitter", "secondaryLeftLine");
   offset(layout.splitterSecondaryRightLine, "Splitter", "secondaryRightLine");
   offset(layout.loaderFilters, "Loader", "filter");
   offset(layout.loaderPerLane, "LoaderPrototype", "perLaneFilters");
   offset(layout.loaderType, "Loader", "type");
   enumerator(layout.loaderOutput, "LoaderType", "Output");
   offset(layout.undergroundType, "UndergroundBelt", "type");
   enumerator(layout.undergroundOutput, "UndergroundBeltType", "Output");
   offset(layout.undergroundMaxDistance, "UndergroundBeltPrototype", "maxDistance");
   offset(layout.showCombinatorSettings, "InterfaceSettings", "showCombinatorSettingsWhenDetailedInfoIsOn.value");
   offset(layout.arithmeticParameters, "ArithmeticCombinator", "controlBehavior.parameters");
   offset(layout.arithmeticFirst, "ArithmeticCombinatorParameters", "first");
   offset(layout.arithmeticSecond, "ArithmeticCombinatorParameters", "second");
   offset(layout.arithmeticOperation, "ArithmeticCombinatorParameters", "operation");
   offset(layout.arithmeticOutput, "ArithmeticCombinatorParameters", "output");
   offset(layout.signalOrConstantType, "SignalOrConstant", "type");
   offset(layout.signalOrConstantSignal, "SignalOrConstant", "signal");
   enumerator(layout.signalOrConstantIsSignal, "SignalOrConstant::Type", "Signal");
   offset(layout.deciderConditions, "DeciderCombinator", "controlBehavior.parameters.conditions");
   offset(layout.deciderOutputs, "DeciderCombinator", "controlBehavior.parameters.outputs");
   offset(layout.conditionFirst, "DeciderCombinatorParameters::Condition", "first");
   offset(layout.conditionComparator, "DeciderCombinatorParameters::Condition", "comparator");
   offset(layout.conditionSecond, "DeciderCombinatorParameters::Condition", "second");
   offset(layout.deciderOutputSignal, "DeciderCombinatorParameters::Output", "signalId");
   offset(layout.selectorParameters, "SelectorCombinator", "controlBehavior.parameters");
   offset(layout.selectorOperation, "SelectorCombinatorParameters", "operation");
   offset(layout.selectorMax, "SelectorCombinatorParameters", "selectMax");
   offset(layout.selectorIndexSignal, "SelectorCombinatorParameters", "index.signal");
   offset(layout.selectorCountSignal, "SelectorCombinatorParameters", "countSignalID");
   {
      const char* arithmetic[] = {"Multiply",  "Divide",     "Add", "Subtract", "Modulo", "Power",
                                  "LeftShift", "RightShift", "AND", "OR",       "XOR"};
      for (size_t i = 0; i < std::size(arithmetic); ++i)
         enumerator(layout.arithmeticOperations[i], "ArithmeticCombinatorParameters::Operation", arithmetic[i]);
      const char* comparisons[] = {"GreaterThan", "LessThan", "Equals", "GreaterOrEqual", "LessOrEqual", "NotEqual"};
      for (size_t i = 0; i < std::size(comparisons); ++i)
         enumerator(layout.comparisons[i], "Comparison::Enum", comparisons[i]);
      const char* selector[] = {"Select",         "Count",         "Random", "QualityTransfer", "StackSize",
                                "RocketCapacity", "QualityFilter", "Time",   "QualitySelect"};
      for (size_t i = 0; i < std::size(selector); ++i)
         enumerator(layout.selectorOperations[i], "SelectorCombinatorParameters::Operation", selector[i]);
   }
   enumerator(layout.selectorSelect, "SelectorCombinatorParameters::Operation", "Select");
   enumerator(layout.selectorCount, "SelectorCombinatorParameters::Operation", "Count");
   offset(layout.signalQuality, "IDWithQuality<SignalIDBase>", "qualityID");
   address(layout.signalPrototype, "?getPrototypeSafe@SignalIDBase@@QEBAPEBVPrototypeBase@@XZ");
   offset(layout.collectorFilters, "AsteroidCollector", "chunkFilters");
   offset(layout.panelIcon, "DisplayPanel", "icon");
   offset(layout.panelText, "DisplayPanel", "text");
   offset(layout.panelAlwaysShow, "DisplayPanel", "alwaysShow");
   address(layout.tilePrototypes,
           "?indexToPrototype@?$PrototypeList@VTilePrototype@@@@2V?$vector@PEAVTilePrototype@@V?$allocator@"
           "PEAVTilePrototype@@@std@@@std@@A");
   address(layout.fluidPrototypes,
           "?indexToPrototype@?$PrototypeList@VFluidPrototype@@@@2V?$vector@PEAVFluidPrototype@@V?$allocator@"
           "PEAVFluidPrototype@@@std@@@std@@A");
   address(layout.asteroidChunkPrototypes,
           "?indexToPrototype@?$PrototypeList@VAsteroidChunkPrototype@@@@2V?$vector@PEAVAsteroidChunkPrototype@@V?$"
           "allocator@PEAVAsteroidChunkPrototype@@@std@@@std@@A");

   offset(layout.itemStackData, "ItemStack", "item");
   offset(layout.inventoryHand, "Inventory", "handPosition");
   offset(layout.itemLabel, "ItemWithLabel", "labelData.label.value");
   size(layout.signalSize, "SignalID");
   offset(layout.blueprintDataIcons, "Blueprint", "previewIcons.data");
   offset(layout.blueprintDataDescription, "Blueprint", "description.value");
   offset(layout.blueprintItemBlueprint, "BlueprintItem", "blueprint");
   offset(layout.bookIcons, "BlueprintBook", "previewIcons.data");
   offset(layout.bookDescription, "BlueprintBook", "description.value");
   offset(layout.bookActiveIndex, "BlueprintBook", "activeIndex");
   offset(layout.bookInventory, "BlueprintBook", "inventory");
   offset(layout.deconItemData, "DeconstructionItem", "deconstructionData");
   offset(layout.deconDataIcons, "DeconstructionData", "previewIcons.data");
   offset(layout.deconDataDescription, "DeconstructionData", "description.value");
   offset(layout.deconDataTreesAndRocks, "DeconstructionData", "treesAndRocksOnly");
   offset(layout.deconDataEntityMode, "DeconstructionData", "entityFilterMode");
   offset(layout.deconDataEntities, "DeconstructionData", "entityFilters");
   offset(layout.deconDataTileMode, "DeconstructionData", "tileSelectionMode");
   offset(layout.deconDataTiles, "DeconstructionData", "tileFilters");
   enumerator(layout.entityFilterWhitelist, "DeconstructionData::EntityFilterMode", "Whitelist");
   enumerator(layout.entityFilterBlacklist, "DeconstructionData::EntityFilterMode", "Blacklist");
   enumerator(layout.tileSelectionOnly, "DeconstructionData::TileSelectionMode", "Only");
   enumerator(layout.tileSelectionNever, "DeconstructionData::TileSelectionMode", "Never");
   {
      const char* filter = "IDWithQualityFilter<ID<EntityPrototype,unsigned short> >";
      size(layout.entityFilterSize, filter);
      offset(layout.entityFilterId, filter, "baseID");
      offset(layout.entityFilterQuality, filter, "qualityCondition.qualityID");
      offset(layout.entityFilterComparison, filter, "qualityCondition.comparison");
   }
   enumerator(layout.comparisonEquals, "Comparison::Enum", "Equals");
   offset(layout.upgradeItemData, "UpgradeItem", "upgradeData");
   offset(layout.upgradeDataIcons, "UpgradeData", "previewIcons.data");
   offset(layout.upgradeDataDescription, "UpgradeData", "description.value");
   offset(layout.upgradeDataMappings, "UpgradeData", "mappings");
   size(layout.mappingSize, "UpgradeMapping");
   offset(layout.mappingSourceId, "UpgradeMapping", "source.filter.baseID.itemID");
   offset(layout.mappingSourceQuality, "UpgradeMapping", "source.filter.qualityCondition.qualityID");
   offset(layout.mappingSourceEntity, "UpgradeMapping", "source.entityFilter.baseID");
   offset(layout.mappingSourceEntityQuality, "UpgradeMapping", "source.entityFilter.qualityCondition.qualityID");
   offset(layout.mappingDestinationType, "UpgradeMapping", "destination.upgradeID.baseID.type");
   offset(layout.mappingDestinationId, "UpgradeMapping", "destination.upgradeID.baseID.itemID");
   offset(layout.mappingDestinationQuality, "UpgradeMapping", "destination.upgradeID.qualityID");
   enumerator(layout.upgradeTypeEntity, "UpgradeIDBase::Type", "Entity");
   offset(layout.bookSlotBook, "BlueprintBookSlot", "book");
   offset(layout.bookGuiHeader, "BlueprintBookGui", "headerFrame");
   offset(layout.bookGuiInside, "BlueprintBookGui", "insideFrame");
   offset(layout.bookGuiNavigation, "BlueprintBookGui", "navigationFlow");
   offset(layout.bookGuiName, "BlueprintBookGui", "blueprintBookLabel");
   offset(layout.bookGuiRename, "BlueprintBookGui", "editButton");
   offset(layout.bookGuiDescription, "BlueprintBookGui", "descriptionLabel");
   offset(layout.bookGuiList, "BlueprintBookGui", "blueprintsList");
   offset(layout.listViewMode, "BlueprintsList", "viewMode");
   enumerator(layout.listViewList, "BlueprintsListViewMode", "List");
   offset(layout.recordId, "BlueprintRecord", "id");
   offset(layout.recordItem, "BlueprintRecord", "itemID");
   offset(layout.recordLabel, "BlueprintRecord", "label.value");
   size(layout.recordIdSize, "BlueprintRecordID");
   offset(layout.recordIdPlayer, "BlueprintRecordID", "playerIndex");
   offset(layout.recordIdIndex, "BlueprintRecordID", "id");
   classSlot(layout.recordIsPreview, "BlueprintRecord", "isPreview");
   offset(layout.singleRecordBlueprint, "SingleBlueprintRecord", "blueprint");
   offset(layout.bookRecordRecords, "BlueprintBookRecord", "records");
   address(layout.bookRecordActiveIndex, "?getActiveIndex@BlueprintBookRecord@@QEBAGPEBVPlayer@@PEAVLatencyState@@@Z");
   offset(layout.playerLatencyState, "Player", "latencyState");
   offset(layout.bookRecordIcons, "BlueprintBookRecord", "previewIcons.data");
   offset(layout.bookRecordDescription, "BlueprintBookRecord", "description.value");
   offset(layout.deconRecordData, "DeconstructionRecord", "deconstructionData");
   offset(layout.upgradeRecordData, "UpgradeRecord", "upgradeData");
   address(layout.recordSlotRecord, "?getRecord@BlueprintRecordSlotButton@@QEBAPEBVBlueprintRecord@@XZ");
   offset(layout.recordSlotPlayer, "BlueprintRecordSlotButton", "context.player");
   offset(layout.recordSlotBook, "BlueprintRecordSlotButton", "parentBook");
   offset(layout.recordSlotIndex, "BlueprintRecordSlotButton", "location.slotIndex");
   offset(layout.recordSlotGrabbed, "BlueprintRecordSlotButton", "showGrabbed");
   enumerator(layout.recordSlotShowsGrabbed, "BlueprintRecordSlotButton::ShowGrabbed", "True");
   offset(layout.recordSlotProgress, "BlueprintRecordSlotButton", "progress");
   classSlot(layout.adapterCursorRecord, "GameAdapter", "getCursorRecordID");
   offset(layout.libraryInside, "BlueprintLibraryGui", "insideFrame");
   offset(layout.libraryTabs, "BlueprintLibraryGui", "libraryTabs");
   offset(layout.libraryBookHolder, "BlueprintLibraryGui", "bookWidgetHolder");
   offset(layout.libraryMemory, "BlueprintLibraryGui", "memoryUsageLabel");
   offset(layout.shelfList, "BlueprintShelfWidget", "blueprintsList");
   offset(layout.shelfSynchronising, "BlueprintShelfWidget", "synchronisingLabel");
   offset(layout.bookRecordGuiHeader, "BlueprintBookRecordWidget", "headerFrame");
   offset(layout.bookRecordGuiInside, "BlueprintBookRecordWidget", "insideFrame");
   offset(layout.bookRecordGuiNavigation, "BlueprintBookRecordWidget", "windowHeader");
   offset(layout.bookRecordGuiDescription, "BlueprintBookRecordWidget", "descriptionLabel");
   offset(layout.bookRecordGuiList, "BlueprintBookRecordWidget", "blueprintsList");
   offset(layout.bookRecordGuiRecord, "BlueprintBookRecordWidget", "recordID");
   offset(layout.bookHeaderName, "BlueprintBookHeader", "nameLabel");
   offset(layout.bookHeaderRename, "BlueprintBookHeader", "editButton");
   offset(layout.achievementsHolder, "AchievementGui", "achievementHolder");
   offset(layout.achievementsProgress, "AchievementGui", "progressLabel");
   offset(layout.achievementsBar, "AchievementGui", "progressBar");
   offset(layout.achievementsModded, "AchievementGui", "moddedGame");
   offset(layout.achievementsPlaytime, "AchievementGui", "notInGameLongEnoughLabel");
   offset(layout.achievementCardPrototype, "AchievementCard", "achievementPrototype");
   offset(layout.achievementCardState, "AchievementCard", "state");
   offset(layout.achievementCardRight, "AchievementCard", "rightFlow");
   offset(layout.achievementCardDescription, "AchievementCard", "descriptionFlow");
   enumerator(layout.achievementCompleted, "AchievementState", "Completed");
   enumerator(layout.achievementFailed, "AchievementState", "Failed");
   offset(layout.tipsList, "TipsAndTricksGui", "listbox");
   offset(layout.tipsContent, "TipsAndTricksGui", "contentFlow");
   offset(layout.tipsTitle, "TipsAndTricksGui", "title");
   offset(layout.tipsText, "TipsAndTricksGui", "text");
   offset(layout.tipsPlayTutorial, "TipsAndTricksGui", "playTutorialButton");
   offset(layout.tipsUnread, "TipsAndTricksGui", "unreadButton");
   offset(layout.tipsNothingFound, "TipsAndTricksGui", "nothingFoundFlow");
   offset(layout.globalTipsAndTricks, "GlobalContext", "tipsAndTricks.value");
   offset(layout.tipsItems, "TipsAndTricks", "items");
   offset(layout.tipItemIndent, "TipsAndTricksItem", "indent");
   offset(layout.tipItemIsTitle, "TipsAndTricksItem", "isTitle");
   offset(layout.tipItemStatus, "TipsAndTricksItem", "status");
   enumerator(layout.tipStatusSuggested, "TipStatus::Enum", "Suggested");

   offset(layout.customInputs, "ControlSettings", "customInputs");
   size(layout.controlInputSize, "ControlInput");
   offset(layout.controlInputPrototype, "ControlInput", "customInputPrototype");
   offset(layout.controlInputKey1, "ControlInput", "keyboardAndMouseInput1.value");
   offset(layout.controlInputKey2, "ControlInput", "keyboardAndMouseInput2.value");
   offset(layout.inputValueType, "ControlInputValue", "type");
   offset(layout.inputValueScancode, "ControlInputValue", "scancode");
   offset(layout.inputValueModifiers, "ControlInputValue", "modifiers");
   enumerator(layout.inputValueKeyboard, "ControlInputValue::Type", "Keyboard");
   address(layout.keyFromScancode, "SDL_GetKeyFromScancode_REAL");
   offset(layout.sidePanelContainer, "GuiWithSideButtons", "sidePanelContainer");
   offset(layout.onOffEntityWindow, "GenericOnOffEntityGui", "entityWindow");
   offset(layout.singleFluidBoxGui, "SingleFluidBoxEntityGui", "fluidBoxGui");
   offset(layout.fluidBoxIcon, "FluidBoxGui", "fluidIcon");
   offset(layout.fluidBoxBar, "FluidBoxGui", "fluidPercentageBar");
   offset(layout.trainStopTabs, "TrainStopGui", "tabs");
   offset(layout.trainsWidgetTable, "TrainsWidget", "trainsTable");
   offset(layout.trainsWidgetEmpty, "TrainsWidget", "noTrainsSubheader");
   offset(layout.trainCardMinimap, "TrainWithMinimap", "minimapSlot");
   offset(layout.trainCardStatus, "TrainWithMinimap", "statusButton");
   offset(layout.trainGuiTabs, "TrainGui", "tabs");
   offset(layout.trainGuiScheduleTab, "TrainGui", "scheduleTab");
   offset(layout.trainGuiSchedule, "TrainGui", "scheduleGui");
   offset(layout.scheduleGroupNote, "ScheduleGui", "trainGroupNote");
   offset(layout.scheduleAddStation, "ScheduleGui", "addStationButton");
   offset(layout.scheduleAddInterrupt, "ScheduleGui", "addInterruptButton");
   offset(layout.scheduleStations, "ScheduleGui", "scheduleDragPane");
   offset(layout.scheduleInterrupts, "ScheduleGui", "scheduleInterruptsDragPane");
   offset(layout.conditionHolderConditions, "ScheduleConditionGuiHolder", "conditionsDragPane");
   offset(layout.conditionHolderAdd, "ScheduleConditionGuiHolder", "addWaitConditionButton");
   offset(layout.scheduleStationGoTo, "ScheduleStationGui", "goToButton");
   offset(layout.scheduleStationLabel, "ScheduleStationGui", "stationLabel");
   offset(layout.scheduleStationChange, "ScheduleStationGui", "changeStationButton");
   offset(layout.scheduleStationRemove, "ScheduleStationGui", "removeButton");
   offset(layout.scheduleStationUnload, "ScheduleStationGui", "unloadCheckBox");
   offset(layout.scheduleConditionDrag, "ScheduleConditionGui", "draggableSpace");
   offset(layout.scheduleConditionRemove, "ScheduleConditionGui", "removeButton");
   offset(layout.scheduleInterruptGoTo, "ScheduleInterruptGui", "gotoButton");
   offset(layout.scheduleInterruptName, "ScheduleInterruptGui", "name");
   offset(layout.scheduleInterruptEdit, "ScheduleInterruptGui", "editButton");
   offset(layout.scheduleInterruptRemove, "ScheduleInterruptGui", "removeButton");
   offset(layout.interruptEditorConditionsScroll, "EditInterruptGui", "conditionsScroll");
   offset(layout.interruptEditorTargetsScroll, "EditInterruptGui", "targetsScroll");
   offset(layout.interruptEditorConditions, "EditInterruptGui", "conditionsHolder");
   offset(layout.interruptEditorTargets, "EditInterruptGui", "stationsDragPane");
   offset(layout.interruptEditorAddTarget, "EditInterruptGui", "addInterruptStationButton");
   offset(layout.trainsGuiTabs, "TrainsGui", "tabs");
   offset(layout.trainsGuiTrains, "TrainsGui", "trainsWidget");
   offset(layout.trainsGuiTrainsList, "TrainsGui", "trainsList");
   offset(layout.trainsGuiStationsTab, "TrainsGui", "stationsTab");
   offset(layout.trainsGuiStationsList, "TrainsGui", "stationsList");
   offset(layout.trainsGuiStations, "TrainsGui", "stationsTable");
   offset(layout.trainsGuiNoStations, "TrainsGui", "noStopsFrame");
   offset(layout.trainsGuiGroupsTab, "TrainsGui", "groupsTab");
   offset(layout.trainsGuiGroups, "TrainsGui", "groupsWidget");
   offset(layout.trainsGuiGroupsList, "TrainsGui", "groupsList");
   offset(layout.electricNetworkBars, "ElectricNetworkGuiWindow<ElectricPole>", "satisfactionFlow");
   offset(layout.electricNetworkFlows, "ElectricNetworkGuiWindow<ElectricPole>", "gui");
   offset(layout.electricNetworkConsumption, "ElectricNetworkGuiWindow<ElectricPole>", "gui.inputFrame");
   offset(layout.electricNetworkProduction, "ElectricNetworkGuiWindow<ElectricPole>", "gui.outputFrame");
   offset(layout.electricNetworkStorage, "ElectricNetworkGuiWindow<ElectricPole>", "gui.storageFrame");
   offset(layout.flowFrameGraph,
          "FlowDataFrame<FlowStatistics<IDWithQuality<ID<EntityPrototype,unsigned short> >,double,ElectricityTag>,"
          "ElectricPole,FlowGuiEnabler<0> >",
          "graph");
   offset(layout.productionTabs, "ProductionGui", "tabbedPane");
   offset(layout.productionPrecisions, "ProductionGui", "insideFrame.subheader");
   {
      // In the order of the window's tabs.
      static constexpr const char* flows[Layout::kProductionTabs] = {
         "itemProductionGui", "fluidProductionGui", "buildingStatisticsGui", "pollutionStatisticsGui", "killsGui"};
      static constexpr const char* columns[2] = {"inputFrame", "outputFrame"};
      for (int tab = 0; tab < Layout::kProductionTabs; ++tab) {
         offset(layout.productionFlows[tab], "ProductionGui", flows[tab]);
         for (int column = 0; column < 2; ++column)
            offset(layout.productionColumns[tab][column], "ProductionGui",
                   std::format("{}.{}", flows[tab], columns[column]).c_str());
         // A column's members, relative to the column: both columns of a tab are one type.
         auto inColumn = [&](uint32_t& out, const char* member) {
            uint32_t at = 0;
            offset(at, "ProductionGui", std::format("{}.inputFrame.{}", flows[tab], member).c_str());
            out = at - layout.productionColumns[tab][0];
         };
         inColumn(layout.flowColumnSubheader[tab], "subheader");
         inColumn(layout.flowColumnGlobal[tab], "globalStatisticsCheckBox");
         inColumn(layout.flowColumnQuality[tab], "qualitySelectionDropDown");
         inColumn(layout.flowColumnReset[tab], "resetFiltersButton");
         inColumn(layout.flowColumnTable[tab], "table");
      }
   }
   offset(layout.relativeWrapperTable, "CustomGuiGameGuiWrapper", "table");
   offset(layout.relativeWrapperTop, "CustomGuiGameGuiWrapper", "topFlow");
   offset(layout.relativeWrapperLeft, "CustomGuiGameGuiWrapper", "leftFlow");
   offset(layout.relativeWrapperRight, "CustomGuiGameGuiWrapper", "rightFlow");
   offset(layout.relativeWrapperBottom, "CustomGuiGameGuiWrapper", "bottomFlow");
   offset(layout.playerCustomGui, "Player", "customGui");
   offset(layout.customGuiRootElements, "CustomGui", "rootElements");
   offset(layout.customGuiModOwners, "CustomGui", "modOwnersElementMapping");
   enumerator(layout.customGuiScreen, "CustomGui::Position", "Screen");
   {
      constexpr const char* kPositionNode =
         "std::_Tree_node<std::pair<enum CustomGui::Position const ,CustomGuiElement *>,void *>";
      constexpr const char* kOwnerNode =
         "std::_Tree_node<std::pair<std::basic_string<char,std::char_traits<char>,std::allocator<char> > const ,"
         "std::set<unsigned int,std::less<unsigned int>,std::allocator<unsigned int> > >,void *>";
      offset(layout.positionNodeKey, kPositionNode, "_Myval.first");
      offset(layout.positionNodeElement, kPositionNode, "_Myval.second");
      offset(layout.ownerNodeName, kOwnerNode, "_Myval.first");
      offset(layout.ownerNodeIndices, kOwnerNode, "_Myval.second");
      offset(layout.indexNodeValue, "std::_Tree_node<unsigned int,void *>", "_Myval");
   }
   offset(layout.guiElementWidget, "CustomGuiElement", "widget");
   offset(layout.guiElementIndex, "CustomGuiElement", "index");
   offset(layout.guiElementChildren, "CustomGuiElement", "children");
   offset(layout.controllerGuiTarget, "Controller", "guiTarget");
   offset(layout.guiTargetSize, "GuiTarget", "data._Mypair._Myval2._Mysize");
   address(layout.guiTargetCurrent, "?current@GuiTarget@@AEBAAEBVGuiTargetData@@XZ");
   offset(layout.guiTargetType, "GuiTargetItemBase", "openGuiType");
   offset(layout.guiTargetCustomGui, "GuiTargetItemBase", "customGui");
   offset(layout.gameTargeterTarget, "TargeterBase", "target");
   enumerator(layout.openGuiTypeCustomGui, "OpenGuiType", "CustomGui");

   offset(layout.gameViewControllerView, "GameView", "controllerView");
   classSlot(layout.controllerViewQuickBar, "ControllerView", "getQuickBar");
   offset(layout.quickBarMainRows, "QuickBarGui", "mainWindowRows");
   offset(layout.quickBarPickerRows, "QuickBarGui", "pageSelectorRows");
   offset(layout.quickBarPicker, "QuickBarGui", "pageSelectorFrame");
   offset(layout.quickBarPickingFor, "QuickBarGui", "selectingNewPageForRow");
   offset(layout.rowPage, "QuickBarGui::RowWidgets", "pageIndex");
   offset(layout.rowButton, "QuickBarGui::RowWidgets", "button");
   offset(layout.rowSlots, "QuickBarGui::RowWidgets", "slots");
   classSlot(layout.controllerViewToolBar, "ControllerView", "getToolBar");
   classSlot(layout.controllerViewVehicleToolBar, "ControllerView", "getVehicleToolBar");
   offset(layout.toolBarTable, "ToolEquipGui", "equipmentTable");
   classSlot(layout.controllerViewShortcutBar, "ControllerView", "getShortcutBar");
   offset(layout.shortcutBarColumns, "ShortcutBarGui", "columns");
   offset(layout.shortcutBarListButton, "ShortcutBarGui", "expandButton");
   offset(layout.shortcutBarList, "ShortcutBarGui", "shortcutSelectionFrame");
   offset(layout.shortcutBarListOpen, "ShortcutBarGui", "shortcutSelectionFrameVisible");
   offset(layout.shortcutBarListRows, "ShortcutBarGui", "shortcutRows");
   offset(layout.shortcutRowCheckBox, "ShortcutBarGui::ShortcutRow", "dockCheckbox");
   offset(layout.shortcutButtonBehavior, "ShortcutButton", "behavior");
   offset(layout.shortcutBehaviorPrototype, "ShortcutBehavior", "prototype");
   offset(layout.buttonIsToggle, "agui::Button", "isButtonToggleButton");
   offset(layout.gameViewSideMenu, "GameView", "sideMenu");
   offset(layout.gameViewTrackedAchievements, "GameView", "trackedAchievementHolder");
   classSlot(layout.controllerViewCraftingQueue, "ControllerView", "getCraftingQueue");
   offset(layout.craftingQueueSlots, "CraftingQueueGui", "slots");
   offset(layout.characterInfoQueueLabel, "CharacterInfoGui", "craftingQueueLabel");
   offset(layout.characterInfoQueue, "CharacterInfoGui", "craftingQueueGui");
   offset(layout.sideMenuMuteButton, "SideMenu", "masterMutedButton");
   offset(layout.gameViewResearch, "GameView", "currentResearchInfo");
   offset(layout.researchTitle, "CurrentResearchInfo", "title");
   offset(layout.researchProgressFlow, "CurrentResearchInfo", "progressBarFlow");
   offset(layout.researchProgressLabel, "CurrentResearchInfo", "researchProgressLabel");
   offset(layout.gameViewAlerts, "GameView", "alertGuis");
   offset(layout.alertGuiCategory, "AlertGui", "category");
   offset(layout.alertGuiButton, "AlertGui", "warningSlot");
   offset(layout.gameViewAlertsOverview, "GameView", "alertsOverview");
   offset(layout.alertsOverviewCategory, "AlertsOverview", "category");
   offset(layout.alertsOverviewList, "AlertsOverview", "alertGroupsList");
   offset(layout.alertsOverviewPins, "AlertsOverview", "pinButtonFlow");
   offset(layout.gameViewChartSearch, "GameView", "chartSearchResultGui");
   offset(layout.chartSearchList, "ChartSearchResultGui", "listbox");
   offset(layout.chartSearchPins, "ChartSearchResultGui", "pinButtonFlow");
   offset(layout.gameViewMapViewOptions, "GameView", "mapViewOptionsGui");
   offset(layout.globalMapViewSettings, "GlobalContext", "mapViewSettings.value");
   offset(layout.gameViewPins, "GameView", "pinsGui");
   offset(layout.pinsGuiList, "PinsGui", "pinList");
   offset(layout.pinGuiEdit, "PinGui", "editButton");
   offset(layout.pinGuiLabel, "PinGui", "labelButton");
   offset(layout.pinGuiDelete, "PinGui", "deleteButton");
   offset(layout.mapEditorViewGui, "MapEditorView", "mapEditorGui");
   address(layout.editorActionPerformed,
           "?actionPerformed@MapEditorActionHandler@@QEAAXAEBVEditorAction@@AEAVController@@@Z");
   offset(layout.editorActionType, "EditorAction", "type");
   offset(layout.editorActionString, "EditorAction", "string");
   offset(layout.editorActionSurface, "EditorAction", "surfaceIndex");
   offset(layout.surfaceAndDataData, "ActionData::SurfaceIndexAndData<unsigned int>", "data");
   enumerator(layout.editorDecreaseSpeed, "EditorActionType", "DecreaseGameSpeed");
   enumerator(layout.editorIncreaseSpeed, "EditorActionType", "IncreaseGameSpeed");
   enumerator(layout.editorResetSpeed, "EditorActionType", "ResetGameSpeed");
   enumerator(layout.editorTickOnce, "EditorActionType", "TickOnce");
   enumerator(layout.editorTogglePause, "EditorActionType", "ToggleEntityUpdatePaused");
   enumerator(layout.editorEntityVariation, "EditorActionType", "ChangeEntityVariation");
   enumerator(layout.editorCliffVariation, "EditorActionType", "ChangeCliffVariation");
   enumerator(layout.editorResourceVariation, "EditorActionType", "ChangeResourceVariation");
   enumerator(layout.editorTileVariation, "EditorActionType", "ChangeTileVariation");
   enumerator(layout.editorSwitchToSurface, "EditorActionType", "SwitchToSurface");
   enumerator(layout.editorCloneSource, "EditorActionType", "SetCloneBrushSourceSurfaceAndPosition");
   enumerator(layout.editorCloneDestination, "EditorActionType", "SetCloneBrushDestinationSurfaceAndPosition");
   enumerator(layout.editorRemoveScriptRectangle, "EditorActionType", "RemoveScriptRectangle");
   enumerator(layout.editorRemoveScriptPosition, "EditorActionType", "RemoveScriptPosition");
   enumerator(layout.editorCreateScriptRectangle, "EditorActionType", "CreateScriptRectangle");
   enumerator(layout.editorCreateScriptPosition, "EditorActionType", "CreateScriptPosition");
   enumerator(layout.editorSelectScriptRectangle, "EditorActionType", "SelectScriptRectangle");
   enumerator(layout.editorSelectScriptPosition, "EditorActionType", "SelectScriptPosition");
   offset(layout.surfaceName, "Surface", "name");
   address(layout.surfaceDisplayName,
           "?getDisplayName@Surface@@QEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@XZ");
   offset(layout.mapGameSpeed, "Map", "gameSpeed");
   address(layout.gameSpeedStr,
           "?str@GameSpeed@@QEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@XZ");
   offset(layout.mapPausedState, "Map", "entityUpdatePausedState");
   offset(layout.pausedSetting, "EntityUpdatePausedState", "pausedSetting");
   offset(layout.pausedTicksToRun, "EntityUpdatePausedState", "ticksToRun");
   offset(layout.mapEntityTick, "Map", "entityTick");
   offset(layout.mapExtraScriptData, "Map", "extraScriptData");
   offset(layout.scriptRectangles, "ExtraScriptData", "rectanglesBySurface");
   offset(layout.scriptPositions, "ExtraScriptData", "positionsBySurface");
   {
      constexpr const char* kRectangleNode =
         "std::_Tree_node<std::pair<SurfaceIndex const ,std::vector<std::unique_ptr<ScriptRectangle,std::default_"
         "delete<ScriptRectangle> >,std::allocator<std::unique_ptr<ScriptRectangle,std::default_delete<"
         "ScriptRectangle> > > > >,void *>";
      constexpr const char* kPositionNode =
         "std::_Tree_node<std::pair<SurfaceIndex const ,std::vector<std::unique_ptr<ScriptPosition,std::default_"
         "delete<ScriptPosition> >,std::allocator<std::unique_ptr<ScriptPosition,std::default_delete<"
         "ScriptPosition> > > > >,void *>";
      offset(layout.scriptRectangleNodeValue, kRectangleNode, "_Myval.first");
      offset(layout.scriptRectangleNodeVector, kRectangleNode, "_Myval.second");
      offset(layout.scriptPositionNodeValue, kPositionNode, "_Myval.first");
      offset(layout.scriptPositionNodeVector, kPositionNode, "_Myval.second");
   }
   offset(layout.scriptDataName, "CommonScriptData", "name");
   offset(layout.scriptDataId, "CommonScriptData", "id");
   offset(layout.globalModManager, "GlobalContext", "modManager.value");
   address(layout.resolveResourcePath,
           "?resolveResourcePath@ModManager@@QEBA?AVPackagePath@@V?$basic_string_view@DU?$char_traits@D@std@@@std@@@Z");
   address(layout.packagePathOpen,
           "?open@PackagePath@@QEBA?AV?$UniquePointer@VReadStream@@U?$SimpleDeleter@VReadStream@@@@$0A@@@XZ");
   size(layout.packagePathSize, "PackagePath");
   offset(layout.packagePathPath, "PackagePath", "path");
   classSlot(layout.readStreamRead, "ReadStream", "read");
   classSlot(layout.readStreamRemaining, "ReadStream", "remaining");
   classSlot(layout.readStreamDestructor, "ReadStream", "~ReadStream");
   address(layout.operatorDelete, "??3@YAXPEAX_K@Z");
   offset(layout.mapViewLogisticNetwork, "MapViewSettings", "showLogisticNetwork");
   offset(layout.mapViewElectricNetwork, "MapViewSettings", "showElectricNetwork");
   offset(layout.mapViewTurretRange, "MapViewSettings", "showTurretRange");
   offset(layout.mapViewPollution, "MapViewSettings", "showPollution");
   offset(layout.mapViewStationNames, "MapViewSettings", "showTrainStationNames");
   offset(layout.mapViewPlayerNames, "MapViewSettings", "showPlayerNames");
   offset(layout.mapViewTags, "MapViewSettings", "showTags");
   offset(layout.mapViewWorkerRobots, "MapViewSettings", "showWorkerRobots");
   offset(layout.mapViewRailSignalStates, "MapViewSettings", "showRailSignalStates");
   offset(layout.mapViewRecipeIcons, "MapViewSettings", "showRecipeIcons");
   offset(layout.mapViewPipelines, "MapViewSettings", "showPipelines");
   offset(layout.mapViewNonstandardInfo, "MapViewSettings", "showNonstandardMapInfo");
   offset(layout.iconButtonCount, "IconButtonWithNumber", "count");
   offset(layout.gameViewGoal, "GameView", "goalDescription");
   offset(layout.goalInnerFrame, "GoalDescription", "innerFrame");
   offset(layout.gameViewBottom, "GameView", "bottomContainer");
   offset(layout.bottomHealthBar, "BottomContainer", "healthProgressBar");
   offset(layout.bottomShieldBar, "BottomContainer", "shieldProgressBar");
   offset(layout.bottomVehicleHealthBar, "BottomContainer", "vehicleHealthProgressBar");
   offset(layout.bottomVehicleShieldBar, "BottomContainer", "vehicleShieldProgressBar");
   offset(layout.bottomMiningBar, "BottomContainer", "miningProgressBar");
   offset(layout.gameViewFactoriopedia, "GameView", "factoriopedia");
   offset(layout.factoriopediaList, "Factoriopedia", "selectList");
   offset(layout.factoriopediaSubheader, "Factoriopedia", "insideFrame.subheader");
   offset(layout.factoriopediaPage, "Factoriopedia", "scrollPane");
   offset(layout.factoriopediaUnresearched, "Factoriopedia", "showUnresearchedButton");
   offset(layout.factoriopediaPinned, "Factoriopedia", "pinned");
   offset(layout.labelRichText, "agui::Label", "resizableText.richTextData");
   offset(layout.richTextSectionsBegin, "TextDrawSections", "sections.begin_");
   offset(layout.richTextSectionsEnd, "TextDrawSections", "sections.end_");
   size(layout.richTextSectionSize, "TextDrawSection");
   offset(layout.richTextSectionType, "TextDrawSection", "type");
   offset(layout.richTextSectionTag, "TextDrawSection", "tagText");
   offset(layout.richTextSectionText, "TextDrawSection", "text");
   offset(layout.hoverableLabelManager, "LabelWithHoverableRichText", "hoverManger");
   offset(layout.hoverManagerTooltip, "RichTextHoverManager", "hoverTooltip");
   address(layout.richTextHandleHover,
           "?handleHover@RichTextHoverManager@@IEAAXAEBVTextDrawSection@@PEBVItem@OutputConsole@@_N@Z");
   address(layout.richTextClearTooltip, "?clearTooltip@RichTextHoverManager@@QEAAXXZ");
   offset(layout.playerOutputConsole, "Player", "outputConsole");
   offset(layout.playerConsoleHoverManager, "Player", "outputConsoleRichTextHoverManager");
   offset(layout.consoleNodeNext, "std::_List_node<OutputConsole::Item,void *>", "_Next");
   offset(layout.consoleNodeValue, "std::_List_node<OutputConsole::Item,void *>", "_Myval");
   offset(layout.consoleItemUpdateTick, "OutputConsole::Item", "updateTick.value");
   offset(layout.consoleItemWrappedText, "OutputConsole::Item", "wrappedText");
   offset(layout.resizableTextData, "agui::ResizableText", "data");
   offset(layout.resizableTextRichText, "agui::ResizableText", "richTextData");
   offset(layout.resizableTextMaxWidth, "agui::ResizableText", "lastMaxWidth");
   size(layout.consoleRenderItemSize, "OutputConsoleRenderer::RenderItem");
   offset(layout.consoleRenderItemItem, "OutputConsoleRenderer::RenderItem", "item");
   address(layout.consoleGetRenderItems,
           "?getRenderItems@OutputConsoleRenderer@@QEAA?AV?$vector@URenderItem@OutputConsoleRenderer@@V?$allocator@"
           "URenderItem@OutputConsoleRenderer@@@std@@@std@@AEBVOutputConsole@@_NVMapTick@@PEAVDrawQueue@@H@Z");
   address(layout.consoleRenderItemsFree,
           "??1?$vector@URenderItem@OutputConsoleRenderer@@V?$allocator@URenderItem@OutputConsoleRenderer@@@std@@@"
           "std@@QEAA@XZ");
   address(layout.resizableTextLines,
           "?lines@ResizableText@agui@@QEBAAEBV?$vector@V?$basic_string_view@DU?$char_traits@D@std@@@std@@V?$"
           "allocator@V?$basic_string_view@DU?$char_traits@D@std@@@std@@@2@@std@@XZ");
   offset(layout.gameViewTechnology, "GameView", "technologyGui");
   offset(layout.technologyQueue, "TechnologyGui", "researchQueueGui");
   offset(layout.technologyTitle, "TechnologyGui", "featuredTechnologyTitle");
   offset(layout.technologyStatus, "TechnologyGui", "featuredTechnologyStatus");
   offset(layout.technologyFeatured, "TechnologyGui", "featuredTechnologyGui");
   offset(layout.technologyList, "TechnologyGui", "technologiesGui");
   offset(layout.technologyGraphTitle, "TechnologyGui", "technologyGraphTitleFrame");
   offset(layout.technologyGraphHolder, "TechnologyGui", "technologyGraphHolder");
   offset(layout.technologyGraph, "TechnologyGui", "technologyGraph");
   offset(layout.technologyListTable, "TechnologyListGui", "table");
   offset(layout.queueTable, "ResearchQueueGui", "queueTable");
   offset(layout.queueElementSlot, "TechnologyQueueElement", "technologySlot");
   offset(layout.queueElementCancel, "TechnologyQueueElement", "cancelButton");
   offset(layout.techSlotTechnology, "TechnologySlot", "technology");
   offset(layout.techSlotResearchQueue, "TechnologySlot", "researchQueue");
   offset(layout.techSlotResearchManager, "TechnologySlot", "researchManager");
   offset(layout.techSlotIndicateProgress, "TechnologySlot", "indicateProgress");
   offset(layout.techReferenceId, "TechnologyReference", "technologyID");
   offset(layout.technologyPrototype, "Technology", "prototype");
   offset(layout.researchQueueMap, "ResearchQueue", "queue._Mypair._Myval2._Map");
   offset(layout.researchQueueMapSize, "ResearchQueue", "queue._Mypair._Myval2._Mapsize");
   offset(layout.researchQueueOffset, "ResearchQueue", "queue._Mypair._Myval2._Myoff");
   offset(layout.researchQueueSize, "ResearchQueue", "queue._Mypair._Myval2._Mysize");
   address(layout.getTechnology, "?getTechnology@TechnologyReference@@QEBAAEBVTechnology@@XZ");
   address(layout.technologyState, "?getState@Technology@@QEBA?AW4ResearchState@1@PEBVResearchQueue@@@Z");
   address(layout.techSlotLevel, "?getLevel@TechnologySlot@@QEBAIXZ");
   address(layout.technologyNameWithLevel,
           "?getLocalisedNameWithLevel@TechnologyPrototype@@QEBA?AVLocalisedString@@I@Z");
   address(layout.researchProgress, "?getProgress@ResearchManager@@QEBANAEBVTechnology@@@Z");
   address(layout.localisedStringFromKey, "??0LocalisedString@@QEAA@PEBD@Z");
   address(layout.localisedStringLiteral, "??0LocalisedString@@QEAA@W4Mode@0@PEBD@Z");
   address(layout.localisedStringWithParameters[0],
           "??0LocalisedString@@QEAA@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV0@@Z");
   address(layout.localisedStringWithParameters[1],
           "??0LocalisedString@@QEAA@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV0@1@Z");
   address(layout.localisedStringWithParameters[2],
           "??0LocalisedString@@QEAA@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV0@11@Z");
   address(layout.localisedStringWithVector,
           "??0LocalisedString@@QEAA@W4Mode@0@$$QEAV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@"
           "$$QEAV?$vector@VLocalisedString@@V?$allocator@VLocalisedString@@@std@@@3@@Z");
   address(layout.gameOperatorNew, "??2@YAPEAX_K@Z");
   offset(layout.localisedStringKey, "LocalisedString", "key");
   enumerator(layout.localisedTranslation, "LocalisedString::Mode", "Translation");
   offset(layout.graphVertices, "TechnologyGraphGui", "graph");
   offset(layout.graphCentral, "TechnologyGraphGui", "central");
   offset(layout.vertexTechnology, "TechnologyGraphGui::Vertex", "technology");
   offset(layout.vertexSlot, "TechnologyGraphGui::Vertex", "slot");
   offset(layout.vertexSuccessors, "TechnologyGraphGui::Vertex", "successors");
   offset(layout.vertexPredecessors, "TechnologyGraphGui::Vertex", "predecessors");
   offset(layout.vertexLayer, "TechnologyGraphGui::Vertex", "layer");
   offset(layout.vertexType, "TechnologyGraphGui::Vertex", "type");
   offset(layout.vertexNumOmitted, "TechnologyGraphGui::Vertex", "numOmitted");
   offset(layout.vertexX, "TechnologyGraphGui::Vertex", "position.x");
   address(layout.entityInfoConstruct,
           "??0?$SelectedInfo@PEBVEntity@@VEntityButton@@@@QEAA@V?$optional@VGuiContext@@@std@@AEBQEBVEntity@@_N@Z");
   address(layout.entityInfoUpdate, "?update@?$SelectedInfo@PEBVEntity@@VEntityButton@@@@QEAAXAEBQEBVEntity@@_N@Z");
   address(layout.entityInfoDestroy, "??_G?$SelectedInfo@PEBVEntity@@VEntityButton@@@@UEAAPEAXI@Z");
   size(layout.entityInfoSize, "SelectedInfo<Entity const *,EntityButton>");
   address(layout.tileInfoConstruct, "??0?$SelectedInfo@VTile@@V?$ObjectButton@VTile@@VEmptyWidget@agui@@@@@@QEAA@V?$"
                                     "optional@VGuiContext@@@std@@AEBVTile@@_N@Z");
   address(layout.tileInfoChange,
           "?change@?$SelectedInfo@VTile@@V?$ObjectButton@VTile@@VEmptyWidget@agui@@@@@@QEAAXAEBVTile@@_N@Z");
   address(layout.tileInfoDestroy,
           "??_G?$SelectedInfo@VTile@@V?$ObjectButton@VTile@@VEmptyWidget@agui@@@@@@UEAAPEAXI@Z");
   size(layout.tileInfoSize, "SelectedInfo<Tile,ObjectButton<Tile,agui::EmptyWidget> >");
   offset(layout.globalInterfaceSettings, "GlobalContext", "interfaceSettings.value");
   offset(layout.tooltipOnTheSide, "InterfaceSettings", "entityToolTipOnTheSide.value");
   offset(layout.playerLatencyAdapter, "Player", "latencyStateAdapter");
   offset(layout.playerGameStateAdapter, "Player", "gameStateAdapter");
   classSlot(layout.adapterEntitySelector, "GameAdapter", "getEntitySelector");
   offset(layout.selectorEntity, "EntitySelector", "selectedEntity.target");
   offset(layout.playerController, "Player", "controllerManager.controller");
   classSlot(layout.controllerSelectedTile, "Controller", "deduceSelectedTile");
   offset(layout.gameViewActiveWindow, "GameView", "activeWindow");

   address(layout.loggingLog, "?log@Logging@@SAXPEBDIW4LogLevel@@0ZZ");
   address(layout.stringAppend,
           "?append@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAAEAV12@QEBD_K@Z");
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

   offset(layout.playerRenderMode, "Player", "renderMode");
   address(layout.chartSelection, "?getChartSelection@PlayerInputSource@@QEBA?AVChartSelection@@XZ");
   size(layout.chartSelectionSize, "ChartSelection");
   offset(layout.chartSelectionTarget, "ChartSelection", "target");
   offset(layout.chartSelectionTag, "ChartSelection", "customTagTarget");
   offset(layout.chartSelectionPatch, "ChartSelection", "resourcePatch");
   offset(layout.chartTagText, "CustomChartTag", "text");
   size(layout.patchInfoSize, "ResourcePatchInfo");
   address(layout.patchInfoConstruct, "??0ResourcePatchInfo@@QEAA@_N@Z");
   address(layout.patchInfoDestroy, "??1ResourcePatchInfo@@QEAA@XZ");
   address(layout.patchInfoUpdate, "?update@ResourcePatchInfo@@QEAA_NPEBVResourceEntity@@AEBVForceData@@_N@Z");
   address(layout.patchFormattedName,
           "?getFormattedNameFor@ResourcePatchInfo@@SA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@"
           "AEBVMaterialID@@NPEBVResourceEntityPrototype@@@Z");
   offset(layout.patchInfoCounts, "ResourcePatchInfo", "expectedMiningAmount.counts");
   offset(layout.patchInfoPrototype, "ResourcePatchInfo", "resourcePrototype");
   size(layout.materialIdSize, "MaterialID");
   offset(layout.playerForce, "Player", "forceID.index");
   offset(layout.mapForceData, "Map", "forceManager.sortedForceDataList.begin_");
   address(layout.gameOperatorDelete, "??3@YAXPEAX@Z");

   address(layout.processCustomInput, "?processCustomInput@PlayerInputSource@@QEAA_NAEBVEvent@@AEBVControlContext@@@Z");
   address(layout.controlTriggeredQuietly,
           "?triggeredByWithoutSideEffect@ControlInput@@AEBAPEBVControlInputValue@@AEBVEvent@@PEBVControlContext@@I@Z");
   address(layout.mainLoopProcessEvent, "?processEvent@MainLoop@@YA?AW4ProcessResult@@AEBVEvent@@_N@Z");
   offset(layout.eventType, "Event", "type");
   offset(layout.eventKeyRepeat, "Event", "keyboard.repeat");
   size(layout.eventSize, "Event");
   enumerator(layout.eventKeyDown, "Event::Type", "KEY_DOWN");
   address(layout.sendRenderStateChanges, "?sendRenderStateChanges@PlayerInputSource@@UEAAXXZ");
   offset(layout.gameViewSurfaceIndex, "GameView", "cachedSurfaceView.surfaceIndex");
   offset(layout.gameViewCentre, "GameView", "cachedSurfaceView.surfaceViewData.position");
   offset(layout.gameViewZoom, "GameView", "cachedSurfaceView.surfaceViewData.zoomData.zoom");
   offset(layout.gameViewChartMode, "GameView", "cachedSurfaceView.surfaceViewData.zoomData.chartMode");
   address(layout.gameViewDisplaySize, "?getDisplaySize@GameView@@QEBA?AVPixelSize@@XZ");
   offset(layout.displayDensity, "InterfaceSettings", "displayDensityValue");
   address(layout.gameViewSurface, "?getSurface@GameView@@QEBAAEBVSurface@@XZ");
   classSlot(layout.controllerGetType, "Controller", "getType");
   enumerator(layout.controllerCharacter, "ControllerType", "Character");
   enumerator(layout.controllerGod, "ControllerType", "God");
   enumerator(layout.controllerRemote, "ControllerType", "Remote");
   enumerator(layout.controllerEditor, "ControllerType", "Editor");
   classSlot(layout.controllerInstantDeconstruction, "Controller", "getInstantDeconstruction");
   classSlot(layout.controllerInstantUpgrading, "Controller", "getInstantUpgrading");
   size(layout.inputActionSize, "InputAction");
   offset(layout.inputActionType, "InputAction", "type");
   offset(layout.inputActionPlayer, "InputAction", "playerIndex");
   offset(layout.inputActionData, "InputAction", "buffer");
   classSlot(layout.inputSourceProcess, "InputSource", "process");
   enumerator(layout.actionMoveOnPan, "InputActionType", "MoveOnPan");
   enumerator(layout.actionSetZoom, "InputActionType", "SetZoom");
   offset(layout.moveOnPanPosition, "ActionData::MoveOnPanData", "position");
   offset(layout.moveOnPanZoom, "ActionData::MoveOnPanData", "zoom");
   offset(layout.moveOnPanSurface, "ActionData::MoveOnPanData", "surface");
   address(layout.openRemoteView,
           "?openRemoteView@PlayerInputSource@@QEAAXAEBVSurface@@AEBVMapPosition@@V?$optional@V?$StrongTypedef@"
           "NVZoomTag@@UPositiveDoubleValidator@@@@@std@@@Z");

   address(layout.deduceSelectedEntity,
           "?deduceSelectedEntity@EntitySelector@@QEAAPEAVEntity@@AEAVSurface@@AEBVMapPosition@@V?$NamedBool@"
           "VAllowToSelectMyCharacterTag@@@@PEAVLatencyState@@PEAV2@W4EntitySelectionMode@@@Z");
   enumerator(layout.selectionModeNormal, "EntitySelectionMode", "Normal");
   address(layout.respectsFogOfWar, "?shouldRespectFogOfWar@Player@@QEBA_N_N@Z");
   address(layout.playerGetCharacter, "?getCharacter@Player@@QEBAPEBVCharacter@@XZ");
   address(layout.selectPriorityLess, "?selectPriorityLess@EntitySelector@@SA_NPEBVEntity@@0AEBVSelectionContext@@@Z");
   size(layout.selectionContextSize, "SelectionContext");
   offset(layout.selectionContextPosition, "SelectionContext", "position");
   offset(layout.selectionContextMode, "SelectionContext", "selectionMode");
   address(layout.descriptionConstruct, "??0Description@@QEAA@V?$optional@VGuiContext@@@std@@@Z");
   address(layout.descriptionDestroy, "??1Description@@QEAA@XZ");
   address(layout.descriptionFill, "?fill@Description@@QEBAXAEAVWidget@agui@@V?$NamedBool@VPlainTag@@@@@Z");
   size(layout.descriptionSize, "Description");
   offset(layout.descriptionName, "Description", "name");
   offset(layout.descriptionText, "Description", "description");
   offset(layout.descriptionModLists, "Description", "modLists");
   offset(layout.descriptionCategories, "Description", "categories");
   offset(layout.descriptionSourceEntity, "Description", "sourceEntity");
   offset(layout.descriptionQuality, "Description", "qualityID");
   offset(layout.categoryKind, "PropertyCategory", "category");
   offset(layout.categoryProperties, "PropertyCategory", "properties");
   enumerator(layout.categoryNone, "PropertyCategory::Category", "None");
   enumerator(layout.categoryDebug, "PropertyCategory::Category", "Debug");
   enumerator(layout.categoryCraftingConditions, "PropertyCategory::Category", "CraftingSurfaceConditions");
   enumerator(layout.categoryConstructionConditions, "PropertyCategory::Category", "ConstructionSurfaceConditions");
   offset(layout.propertyLabel, "PropertyItem", "type");
   offset(layout.propertyOrder, "PropertyItem", "order");
   classSlot(layout.propertyDestroy, "PropertyItem", "~PropertyItem");
   address(layout.recipeItemVtable, "??_7RecipeDescriptionItem@@6B@");
   address(layout.progressBarItemVtable, "??_7ProgressBarPropertyItem@@6B@");
   address(layout.fluidBoxItemVtable, "??_7FluidBoxPropertyItem@@6B@");
   classSlot(layout.entityAddToDescription, "Entity", "addToDescription");
   classSlot(layout.entityGetQuality, "Entity", "getQuality");
   classSlot(layout.entityGetStatus, "Entity", "getStatus");
   size(layout.entityStatusSize, "EntityStatus");
   offset(layout.entityHealth, "EntityWithHealth", "health");
   for (size_t i = 0; i < kProblemStatusCount; ++i)
      enumerator(layout.problemStatuses[i], "EntityStatus::Enum", kProblemStatuses[i]);
   classSlot(layout.prototypeIsBuilding, "EntityPrototype", "isBuilding");
   static constexpr const char* kSides[4] = {"north", "east", "south", "west"};
   for (size_t i = 0; i < 4; ++i)
      offset(layout.beltNeighbours[i], "TransportBeltConnectable", std::format("connections.{}", kSides[i]).c_str());
   offset(layout.beltDirection, "TransportBeltConnectable", "direction");
   offset(layout.beltShape, "TransportBelt", "shape");
   enumerator(layout.beltShapeLeft, "TransportBelt::Shape", "Left");
   enumerator(layout.beltShapeRight, "TransportBelt::Shape", "Right");
   offset(layout.undergroundPartner, "UndergroundBelt", "connection");
   for (size_t i = 0; i < 4; ++i)
      offset(layout.splitterNeighbours[i], "Splitter", std::format("secondaryConnections.{}", kSides[i]).c_str());
   offset(layout.beltLeftLine, "TransportBeltConnectable", "leftLine");
   offset(layout.beltRightLine, "TransportBeltConnectable", "rightLine");
   offset(layout.connectorLine, "TransportLineConnector", "line");
   offset(layout.connectorStart, "TransportLineConnector", "range.start");
   offset(layout.connectorEnd, "TransportLineConnector", "range.end");
   offset(layout.lineFirst, "TransportLine", "distanceToFirst");
   offset(layout.lineItems, "TransportLine", "items._data");
   offset(layout.lineItemsCapacity, "TransportLine", "items._capacity");
   offset(layout.lineItemsHead, "TransportLine", "items._head");
   offset(layout.lineItemsSize, "TransportLine", "items._size");
   size(layout.lineItemSize, "ItemOnTransportLine");
   offset(layout.lineItemGap, "ItemOnTransportLine", "distanceToNext");
   offset(layout.lineItemStack, "ItemOnTransportLine", "stack");
   offset(layout.lineGapIndex, "TransportLine", "firstGapIndex");
   offset(layout.lineLength, "TransportLine", "length");
   offset(layout.lineDirectConnection, "TransportLine", "directConnection");
   offset(layout.lineDirectPosition, "TransportLine", "directConnectionPosition");
   offset(layout.lineDirectInput, "TransportLine", "directInput");
   offset(layout.qualityLevel, "QualityPrototype", "level");
   address(layout.entityGetFluidBoxes, "?getFluidBoxes@Entity@@QEAA?AVFluidBoxSet@@XZ");
   address(layout.fluidBoxSetDestroy, "??1?$SmallVector@PEAVFluidBox@@$00$0A@@@QEAA@XZ");
   size(layout.fluidBoxSetSize, "FluidBoxSet");
   offset(layout.fluidBoxSetBegin, "FluidBoxSet", "begin_");
   offset(layout.fluidBoxSetEnd, "FluidBoxSet", "end_");
   offset(layout.fluidBoxOwner, "FluidBox", "owner");
   offset(layout.fluidBoxPrototype, "FluidBox", "prototype");
   offset(layout.fluidBoxFilter, "FluidBox", "buffer.filter.fluidID");
   offset(layout.segmentFilter, "FluidSegment", "buffer.filter.fluidID");
   offset(layout.fluidConnectionOpposite, "FluidBoxConnection", "oppositeIndex");
   offset(layout.pipeDefinitionsBegin, "FluidBoxPrototype", "connectionDefinitions.begin_");
   size(layout.pipeDefinitionSize, "PipeConnectionDefinition");
   offset(layout.pipeDefinitionType, "PipeConnectionDefinition", "connectionType");
   offset(layout.pipeDefinitionFlow, "PipeConnectionDefinition", "flowDirection");
   enumerator(layout.pipeTypeNormal, "PipeConnectionType::Enum", "Normal");
   enumerator(layout.pipeTypeUnderground, "PipeConnectionType::Enum", "Underground");
   enumerator(layout.flowInput, "FluidFlowDirection::Enum", "Input");
   enumerator(layout.flowOutput, "FluidFlowDirection::Enum", "Output");
   address(layout.pipeLocation, "?getConnectionLocation@PipeConnectionDefinition@@QEBA?AVPipeConnectionLocation@@"
                                "VMapPosition@@VDirection@@V?$NamedBool@VMirroringTag@@@@@Z");
   offset(layout.pipeLocationPosition, "PipeConnectionLocation", "position");
   offset(layout.pipeLocationDirection, "PipeConnectionLocation", "direction");
   size(layout.pipeLocationSize, "PipeConnectionLocation");
   classSlot(layout.entityIsMirrored, "Entity", "isMirrored");
   classSlot(layout.entityGetHeatBuffer, "Entity", "getHeatBuffer");
   offset(layout.heatBufferPrototype, "HeatBuffer", "prototype");
   offset(layout.heatBufferHolder, "HeatBuffer", "holder");
   offset(layout.heatHolderOwner, "HeatBufferHolder", "owner");
   offset(layout.heatConnectionsBegin, "HeatBuffer", "connections.begin_");
   offset(layout.heatConnectionsEnd, "HeatBuffer", "connections.end_");
   size(layout.heatConnectionSize, "HeatBuffer::Connection");
   offset(layout.heatConnectionTarget, "HeatBuffer::Connection", "target");
   offset(layout.heatDefinitionsBegin, "HeatBufferPrototype", "connections.begin_");
   size(layout.heatDefinitionSize, "HeatConnectionDefinition");
   offset(layout.heatDefinitionX, "HeatConnectionDefinition", "position.x");
   offset(layout.heatDefinitionY, "HeatConnectionDefinition", "position.y");
   offset(layout.heatDefinitionDirection, "HeatConnectionDefinition", "direction");
   address(layout.vectorRotate, "?rotate@?$Vector2@NVVector@@@@QEAAXVDirection@@@Z");
   offset(layout.heatBufferEnergy, "HeatBuffer", "energy");
   offset(layout.heatSpecificHeat, "HeatBufferPrototype", "specificHeat");
   offset(layout.heatDefaultTemperature, "HeatBufferPrototype", "defaultTemperature");
   address(layout.reactorNeighbourBonus, "?getTotalNeighbourBonus@Reactor@@QEBANXZ");
   classSlot(layout.entityAvailableEnergy, "Entity", "getAvailableEnergy");
   offset(layout.poleWiresBegin, "ElectricPole", "copperConnector.connections.begin_");
   offset(layout.poleWiresEnd, "ElectricPole", "copperConnector.connections.end_");
   size(layout.wireConnectionSize, "WireConnection");
   address(layout.poleSupplyArea, "?getSupplyArea@ElectricPole@@QEBA?AVBoundingBox@@XZ");
   address(layout.entitySearch, "??$?0VSurface@@@?$EntitySearch@VEntity@@@@QEAA@AEAVSurface@@AEBVBoundingBox@@@Z");
   address(layout.entitySearchDestroy, "??1?$EntitySearch@VEntity@@@@QEAA@XZ");
   size(layout.entitySearchSize, "EntitySearch<Entity>");
   offset(layout.entitySearchContent, "EntitySearch<Entity>", "content");
   size(layout.boundingBoxSize, "BoundingBox");
   address(layout.entityElectricSource, "?getElectricEnergySource@Entity@@QEBAPEAVElectricEnergySource@@XZ");
   classSlot(layout.prototypePowerConsumption, "EntityPrototype", "maximumPowerConsumption");
   classSlot(layout.prototypePowerOutput, "EntityPrototype", "maximumPowerOutput");
   classSlot(layout.entityDropPosition, "Entity", "getDropTargetPosition");
   offset(layout.infinityChestFilters, "InfinityContainer", "settings.filters");
   offset(layout.infinityChestRemoveUnfiltered, "InfinityContainer", "settings.removeUnfilteredItems");
   size(layout.infinityFilterSize, "InfinityFilter");
   offset(layout.infinityFilterItem, "InfinityFilter", "itemWithQualityID.baseID");
   offset(layout.infinityFilterCount, "InfinityFilter", "count");
   offset(layout.infinityFilterMode, "InfinityFilter", "mode");
   enumerator(layout.infinityModeAtLeast, "InfinityFilter::Mode", "KeepAtLeast");
   enumerator(layout.infinityModeAtMost, "InfinityFilter::Mode", "KeepAtMost");
   offset(layout.infinityPipeFluid, "InfinityPipe", "settings.filter.fluidID");
   offset(layout.surfaceMap, "Surface", "map");
   offset(layout.forceLogisticManagers, "ForceData", "logisticManagers");
   address(layout.findMatchingNetworkConst,
           "?findMatchingNetworkByPosition@LogisticManager@@QEBAPEBVLogisticNetwork@@AEBVMapPosition@@@Z");
   address(layout.findClosestNetwork,
           "?findClosestNetworkByPosition@LogisticManager@@QEAAPEAVLogisticNetwork@@AEBVMapPosition@@@Z");
   address(layout.findCellClosestTo, "?findCellClosestTo@LogisticNetwork@@QEBAPEBVLogisticCell@@AEBVMapPosition@@@Z");
   offset(layout.cellOwner, "LogisticCell", "owner");
   offset(layout.cellParameters, "LogisticCell", "logisticParameters");
   offset(layout.cellLogisticRadius, "LogisticCellParameters", "logisticsRadius");
   classSlot(layout.entityDropTarget, "Entity", "getDropTarget");
   offset(layout.inserterPickupTarget, "Inserter", "pickupTarget.target");
   offset(layout.inserterHeldStack, "Inserter", "heldStack");
   offset(layout.inserterPickupVector, "Inserter", "pickupVector");
   classSlot(layout.entityValidPickupTarget, "Entity", "validMachinePickupTargetInternal");
   classSlot(layout.entityValidDropTarget, "Entity", "validMachineDropTargetInternal");
   offset(layout.inserterDropVector, "Inserter", "dropoffVector");
   classSlot(layout.beltInsertionSpecification, "TransportBeltConnectable", "getItemInsertionSpecification");
   address(layout.entityToBeUpgraded, "?isToBeUpgraded@Entity@@QEBA_NXZ");
   offset(layout.turretPriorityList, "Turret", "priorityList");
   offset(layout.turretIgnoreOthers, "Turret", "ignoreUnprioritised");
   offset(layout.combinatorDescription, "Combinator", "playerDescription");
   offset(layout.constantCombinatorDescription, "ConstantCombinator", "playerDescription");
   offset(layout.wallControlBehavior, "Wall", "controlBehavior");
   offset(layout.signalState, "RailSignalBase", "state");
   enumerator(layout.signalOpen, "RailSignalBase::State", "OPEN");
   enumerator(layout.signalClosed, "RailSignalBase::State", "CLOSED");
   enumerator(layout.signalReserved, "RailSignalBase::State", "RESERVED");
   enumerator(layout.signalReservedByCircuit, "RailSignalBase::State", "RESERVED_BY_CIRCUIT_NETWORK");
   offset(layout.chainSignalState, "RailChainSignal", "chainState");
   enumerator(layout.chainNone, "RailChainSignal::ChainState", "NONE");
   enumerator(layout.chainAllOpen, "RailChainSignal::ChainState", "ALL_OPEN");
   enumerator(layout.chainPartiallyOpen, "RailChainSignal::ChainState", "PARTIALLY_OPEN");
   enumerator(layout.chainNoneOpen, "RailChainSignal::ChainState", "NONE_OPEN");
   offset(layout.powerSwitchState, "PowerSwitch", "currentState");
   enumerator(layout.powerSwitchTurningOn, "PowerSwitch::State", "TURNING_ON");
   enumerator(layout.powerSwitchOn, "PowerSwitch::State", "ON");
   classSlot(layout.entityWireConnector, "Entity", "getWireConnectorInternal");
   enumerator(layout.connectorCircuitRed, "WireConnectorID::Enum", "CircuitRed");
   enumerator(layout.connectorCircuitGreen, "WireConnectorID::Enum", "CircuitGreen");
   enumerator(layout.connectorOutputRed, "WireConnectorID::Enum", "CombinatorOutputRed");
   enumerator(layout.connectorOutputGreen, "WireConnectorID::Enum", "CombinatorOutputGreen");
   enumerator(layout.connectorSwitchLeft, "WireConnectorID::Enum", "PowerSwitchLeftCopper");
   enumerator(layout.connectorSwitchRight, "WireConnectorID::Enum", "PowerSwitchRightCopper");
   offset(layout.circuitConnectorNetwork, "CircuitWireConnector", "network");
   offset(layout.circuitNetworkIndex, "CircuitNetwork", "index");
   offset(layout.circuitNetworkCircuits, "CircuitNetwork", "circuitsConnected");
   offset(layout.copperConnectorNetwork, "CopperWireConnector", "network");
   offset(layout.electricNetworkIndex, "ElectricSubNetwork", "index");
   offset(layout.connectorWiresBegin, "WireConnector", "connections.begin_");
   offset(layout.connectorWiresEnd, "WireConnector", "connections.end_");
   classSlot(layout.entityOrientation, "Entity", "getOrientation");
   classSlot(layout.entityBackerName, "Entity", "getBackerName");
   offset(layout.rollingStockTrain, "RollingStock", "train");
   offset(layout.trainFrontMovers, "Train", "rollingStocks.frontMovers");
   offset(layout.trainBackMovers, "Train", "rollingStocks.backMovers");
   offset(layout.trainController, "Train", "controller");
   classSlot(layout.controllerIsManual, "TrainController", "isManual");
   offset(layout.entityUnitNumber, "EntityWithOwner", "unitNumber");
   offset(layout.spiderDestinationsMap, "SpiderVehicle", "autopilot.destinations._Mypair._Myval2._Map");
   offset(layout.spiderDestinationsMapSize, "SpiderVehicle", "autopilot.destinations._Mypair._Myval2._Mapsize");
   offset(layout.spiderDestinationsOffset, "SpiderVehicle", "autopilot.destinations._Mypair._Myval2._Myoff");
   offset(layout.spiderDestinationsSize, "SpiderVehicle", "autopilot.destinations._Mypair._Myval2._Mysize");
   offset(layout.spiderFollowTarget, "SpiderVehicle", "autopilot.followTarget");
   offset(layout.spiderPatrolSize, "SpiderVehicle", "autopilot.patrolSize");
   offset(layout.playerSpidertronSelection, "Player", "spidertronRemoteSelectionState.current");
   address(layout.spidertronIsSelected, "?isSelected@SpidertronRemoteSelection@@QEBA_NPEBVSpiderVehicle@@@Z");
   offset(layout.roboportCell, "Roboport", "logisticCell");
   offset(layout.roboportMaterials, "Roboport", "materialInventory");
   offset(layout.cellNetwork, "LogisticCell", "network");
   offset(layout.cellStationedLogistic, "LogisticCell", "stationedLogisticRobots");
   offset(layout.cellStationedConstruction, "LogisticCell", "stationedConstructionRobots");
   offset(layout.cellChargingSpots, "LogisticCell", "charger.chargingSpots");
   size(layout.chargeSpotSize, "ChargeSpot");
   offset(layout.chargeSpotRobot, "ChargeSpot", "robot");
   offset(layout.cellWaitingToCharge, "LogisticCell", "charger.waitingToChargeQueue._size");
   {
      offset(layout.inserterFilterCount, "InserterPrototype", "itemFilterCount");
      offset(layout.loaderFilterCount, "LoaderPrototype", "itemFilterCount");
      offset(layout.drillFilters, "MiningDrill", "filter.filters");
      offset(layout.drillFilterCount, "MiningDrillPrototype", "filterCount");
      offset(layout.containerLogisticPoint, "LogisticContainer", "logisticPoint");
      offset(layout.containerMode, "LogisticContainerPrototype", "logisticMode.value");
      enumerator(layout.containerModeStorage, "LogisticMode::Enum", "Storage");
      offset(layout.pointSections, "LogisticPoint",
             "sections.std::vector<LogisticSection,std::allocator<LogisticSection> >");
      size(layout.sectionSize, "LogisticSection");
      offset(layout.sectionFilters, "LogisticSection", "filters");
      size(layout.logisticFilterSize, "LogisticFilter");
      offset(layout.logisticFilterSignal, "LogisticFilter", "signalFilter.baseID");
      offset(layout.recipeItemProducts, "RecipePrototype", "itemProducts");
      offset(layout.recipeFluidProducts, "RecipePrototype", "fluidProducts");
      size(layout.itemProductSize, "ItemProductPrototype");
      offset(layout.itemProductId, "ItemProductPrototype", "result.id");
      size(layout.fluidProductSize, "FluidProductPrototype");
      offset(layout.fluidProductId, "FluidProductPrototype", "result.id");
   }
   offset(layout.railDirection, "Rail", "direction");
   offset(layout.railFront, "Rail", "front");
   offset(layout.railBack, "Rail", "back");
   offset(layout.connectionLeft, "Rail::RailConnection", "left");
   offset(layout.connectionStraight, "Rail::RailConnection", "straight");
   offset(layout.connectionRight, "Rail::RailConnection", "right");
   offset(layout.railConnectionPoints, "RailPrototype", "connectionPoints");
   size(layout.connectionPointSize, "RailConnectionPoint");
   offset(layout.railLocationPosition, "RailLocation", "position");
   offset(layout.railLocationDirection, "RailLocation", "direction");
   address(layout.pointInSignal, "?inSignalLocation@RailConnectionPoint@@QEBA?AVRailLocation@@XZ");
   address(layout.pointOutSignal, "?outSignalLocation@RailConnectionPoint@@QEBA?AVRailLocation@@XZ");
   address(layout.pointAlternativeInSignal,
           "?alternativeInSignalLocation@RailConnectionPoint@@QEBA?AV?$optional@VRailLocation@@@std@@XZ");
   address(layout.pointAlternativeOutSignal,
           "?alternativeOutSignalLocation@RailConnectionPoint@@QEBA?AV?$optional@VRailLocation@@@std@@XZ");
   size(layout.optionalRailLocationSize, "std::optional<RailLocation>");
   offset(layout.optionalRailLocationHas, "std::optional<RailLocation>", "_Has_value");
   offset(layout.trainStopRail, "TrainStop", "rail");
   offset(layout.trainStopDirection, "TrainStop", "direction");
   offset(layout.railLocationLayer, "RailLocation", "layer");
   address(layout.drawRailPlanBuilder,
           "?drawRailPlanBuilder@BuildingRenderer@@SA_NAEBVController@@AEBVMapPosition@@AEAVDrawQueue@@V?$NamedBool@"
           "VGhostModeTag@@@@W4BuildMode@@PEBVRailPlannerPrototype@@V?$function@$$A6APEBVEntity@@XZ@std@@"
           "W4EntitySelectionMode@@@Z");
   address(layout.drawRailPlanArrow, "?drawRailPlanArrow@BuildingRenderer@@SAXAEBVRailLocation@@AEAVDrawQueue@@V?$"
                                     "NamedBool@VAnchoredTag@@@@_N@Z");
   address(layout.drawItemCounts,
           "?drawItemCounts@RenderUtil@@YAXAEAVDrawQueue@@AEBV?$map@V?$IDWithQuality@V?$ID@VItemPrototype@@G@@@@IU?$"
           "less@V?$IDWithQuality@V?$ID@VItemPrototype@@G@@@@@std@@V?$allocator@U?$pair@$$CBV?$IDWithQuality@V?$ID@"
           "VItemPrototype@@G@@@@I@std@@@3@@std@@W4Corner@1@AEBVColor@@@Z");
   offset(layout.gameViewRailPlannerActive, "GameView", "railPlannerActive");
   offset(layout.gameViewRailPlanStart, "GameView", "railPathFinderNodeLocation");
   offset(layout.gameViewRailPlanFinder, "GameView", "railPlanFinder._Mypair._Myval2");
   offset(layout.railPlanFinderStepCount, "RailPlanFinder", "stepCount");
   offset(layout.railPlanFinderGoal, "RailPlanFinder", "goalLocation");
   offset(layout.railPlanFinderGhostMode, "RailPlanFinder", "ghostMode");
   offset(layout.gameViewRailLayer, "GameView", "railLayer");
   address(layout.railPlanBestSolution, "?getBestSolution@RailPlanFinder@@QEAA?AVRailBuildPlan@@XZ");
   offset(layout.railBuildPlanStart, "RailBuildPlan", "startLocation");
   offset(layout.railBuildPlanRails, "RailBuildPlan", "rails");
   offset(layout.locatedEntityId, "LocatedEntity", "entityID");
   offset(layout.locatedEntityPosition, "LocatedEntity", "position");
   offset(layout.locatedEntityDirection, "LocatedEntity", "direction");
   size(layout.locatedEntitySize, "LocatedEntity");
   address(layout.entitySelectionUpdate,
           "?checkIfEntitySelectionUpdateShouldBeCreated@EntitySelector@@QEAAXAEAVSurface@@AEBVMapPosition@@V?$"
           "NamedBool@VAllowToSelectMyCharacterTag@@@@PEAVLatencyState@@PEAVEntity@@@Z");
   offset(layout.inputStateMouseInWindow, "InputState", "mouseIsInWindow");
   offset(layout.boundingLeftTop, "BoundingBox", "leftTop");
   offset(layout.boundingRightBottom, "BoundingBox", "rightBottom");
   address(layout.boundingAabb, "?getAABB@BoundingBox@@QEBA?AV1@XZ");

   if (ok && (layout.chartSelectionSize > kChartSelectionCapacity || layout.patchInfoSize > kPatchInfoCapacity)) {
      log::error("A chart structure outgrew its buffer: ChartSelection {} bytes, ResourcePatchInfo {}",
                 layout.chartSelectionSize, layout.patchInfoSize);
      ok = false;
   }
   if (ok &&
       (layout.simpleBuildInputSize > kSimpleBuildInputCapacity ||
        layout.buildingModifierSize > kBuildingModifierCapacity || layout.buildIdSize > kBuildIdCapacity ||
        layout.buildCheckDataSize > kBuildCheckDataCapacity ||
        layout.buildCheckResultSize > kBuildCheckResultCapacity || layout.iteratorSize > kEntityIteratorCapacity)) {
      log::error("A building structure outgrew its stack buffer: SimpleBuildInput {} bytes, BuildingModifier {}, "
                 "BuildID {}, BuildCheckData {}, BuildCheckResult {}, HeuristicEntityIterator {}",
                 layout.simpleBuildInputSize, layout.buildingModifierSize, layout.buildIdSize,
                 layout.buildCheckDataSize, layout.buildCheckResultSize, layout.iteratorSize);
      ok = false;
   }
   if (ok && (layout.boundingBoxSize > kBoundingBoxCapacity || layout.entitySearchSize > kEntitySearchCapacity)) {
      log::error("An entity search structure outgrew its stack buffer: BoundingBox {} bytes, EntitySearch {}",
                 layout.boundingBoxSize, layout.entitySearchSize);
      ok = false;
   }
   if (ok) {
      log::info("Layout: Gui baseWidget {:#x} focused {:#x} modals {:#x} (entry {} bytes); Widget parent {:#x} "
                "children {:#x} privateChildren {:#x} text {:#x} usage {:#x}; Label text {:#x}; vtable slots keyDown "
                "{} keyUp {} focus {} isFocusable {}; KeyEvent {} bytes",
                layout.guiBaseWidget, layout.guiFocusedWidget, layout.guiModals, layout.modalSize, layout.widgetParent,
                layout.widgetChildren, layout.widgetPrivateChildren, layout.widgetText, layout.widgetUsageBits,
                layout.labelText, layout.slotKeyDown, layout.slotKeyUp, layout.slotFocus, layout.slotIsFocusable,
                layout.keyEventSize);
      log::info("Building: SimpleBuildInput {} bytes, entity {:#x} rail planner {:#x} click {:#x} direction {:#x}; "
                "EntityPrototype flags {:#x}, tileGridSize slot {}; getCursorAdapter slot {}, ~ReadAdapter slot {}; "
                "BuildingModifier {} bytes, centre {:#x}; Blueprint rotation {:#x} flip {:#x} snapToGrid {:#x}; "
                "entityBuildabilityCheck slot {}",
                layout.simpleBuildInputSize, layout.simpleBuildInputEntity, layout.simpleBuildInputRailPlanner,
                layout.simpleBuildInputClick, layout.simpleBuildInputDirection, layout.entityPrototypeFlags,
                layout.entityTileGridSize, layout.adapterCursorAdapter, layout.readAdapterDestructor,
                layout.buildingModifierSize, layout.buildingModifierCentre, layout.blueprintRotation,
                layout.blueprintFlip, layout.blueprintSnapToGrid, layout.adapterBuildabilityCheck);
      log::info("Entity icons: Entity::draw slot {}, DrawQueue {} bytes, renderParameters {:#x}", layout.entityDraw,
                layout.drawQueueSize, layout.drawQueueRenderParameters);
   }
   return ok;
}

} // namespace fa::game
