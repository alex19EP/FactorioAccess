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
   address(layout.drawSelectionCounts,
           "?drawSelectionCounts@SelectionToolRenderer@@QEAAXAEAVDrawQueue@@AEBVColor@@@Z");
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
   size(layout.buildingModifierSize, "BuildingModifier");
   offset(layout.buildingModifierCentre, "BuildingModifier", "afterRotationShift");
   offset(layout.blueprintRotation, "Blueprint", "rotation");
   offset(layout.blueprintFlip, "Blueprint", "flip");
   offset(layout.blueprintSnapToGrid, "Blueprint", "snapToGrid._Has_value");
   address(layout.playerBuildId, "?getBuildID@Player@@QEBA?AVBuildID@@XZ");
   size(layout.buildIdSize, "BuildID");
   offset(layout.buildIdEntity, "BuildID", "entityID");
   offset(layout.buildIdTile, "BuildID", "placeAsTile");
   offset(layout.buildIdRailPlanner, "BuildID", "isRailPlanner");
   offset(layout.gameViewEntityMirrored, "GameView", "entityMirrored");
   offset(layout.entityFlipping, "EntityPrototype", "flipping");
   address(layout.settingsDraw, "?draw@EntityToBeBuiltSettings@@QEBAXAEAVDrawQueue@@PEBVEntity@@@Z");
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
   address(layout.renderCursorBox,
           "?renderCursorBox@RenderUtil@@YAXW4CursorBoxType@1@VBoundingBox@@AEAVDrawQueue@@W4Enum@RenderLayer@@CNVColor@@"
           "@Z");
   address(layout.renderDoubleCursorBox, "?renderDoubleCursorBox@RenderUtil@@YAXW4CursorBoxType@1@AEBVBoundingBox@@1AEAV"
                                         "DrawQueue@@W4Enum@RenderLayer@@VColor@@@Z");
   address(layout.adapterRenderCursorBox, "?renderCursorBox@DrawAdapter@@AEBAXAEBVEntity@@V?$NamedBool@"
                                          "VSkipSurfaceCheckTag@@@@W4CursorBoxType@RenderUtil@@@Z");
   address(layout.adapterDestroy, "?destroy@DrawAdapter@@UEBAXPEAVEntity@@@Z");
   address(layout.adapterSetDirection, "?setDirectionAndMirroring@DrawAdapter@@UEBA?AVActionResult@@PEAVEntity@@"
                                       "VDirection@@V?$NamedBool@VMirroringTag@@@@@Z");
   address(layout.drawPoleConnections,
           "?drawPoleConnections@ElectricEnergySource@@SAXAEAVDrawQueue@@AEBVSurface@@AEBVBoundingBox@@@Z");
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
   classSlot(layout.entityDraw, "Entity", "draw");
   address(layout.entityDrawAlert, "?drawAlert@Entity@@QEBAXAEAVDrawQueue@@AEBVSprite@@AEBVMapPosition@@_N@Z");
   offset(layout.globalUtilitySprites, "GlobalContext", "utilitySprites.value");
   size(layout.utilitySpritesSize, "UtilitySprites");
   offset(layout.utilitySpritesMapping, "UtilitySprites", "spritesMapping");
   address(layout.drawInfoIcon, "?drawInfoIcon@DrawQueue@@QEAAXPEBVSprite@@VQualityCondition@@AEBVMapPosition@@NVDrawingFlags@@"
                                "W4Enum@RenderLayer@@AEBVVector@@CVColor@@@Z");
   offset(layout.qualityDrawByDefault, "QualityPrototype", "drawSpriteByDefault");
   address(layout.comparisonStr, "?str@Comparison@@QEBAPEBDXZ");
   address(layout.luaParamEntity,
           "??$getParamOrDefault@PEAVEntity@@@LuaHelper@@YAPEAVEntity@@PEAUlua_State@@HPEBDPEAV1@@Z");
   offset(layout.logisticNetworkId, "LogisticNetwork", "networkID");
   offset(layout.logisticNetworkName, "LogisticNetwork", "networkName.value");
   address(layout.initLuaState, "?initLuaState@LuaHelper@@YAXPEAUlua_State@@@Z");
   address(layout.addLocalFlyingText, "?addLocalFlyingText@Map@@QEAAX$$QEAVLocalMapFlyingText@@@Z");
   address(layout.constructGuiFlyingText,
           "??$construct@VGuiFlyingText@agui@@AEAVPoint@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@"
           "std@@AEAVColor@2@AEAPEBVFont@2@AEAHAEAH@?$_Default_allocator_traits@V?$allocator@VGuiFlyingText@agui@@@"
           "std@@@std@@SAXAEAV?$allocator@VGuiFlyingText@agui@@@1@QEAVGuiFlyingText@agui@@AEAVPoint@4@AEBV?$basic_"
           "string@DU?$char_traits@D@std@@V?$allocator@D@2@@1@AEAVColor@4@AEAPEBVFont@4@AEAH6@Z");
   offset(layout.localMapFlyingTextText, "LocalMapFlyingText", "text");
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
   address(layout.luaPushBoolean, "lua_pushboolean");
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
      constexpr std::string_view kString =
          "V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@";
      std::string type = kNamedPrototypes[i].type;
      std::string name = std::format("?nameToPrototype@?$PrototypeList@V{0}@@@@0V?$map@{1}PEAV{0}@@U?$less@X@2@V?$"
                                     "allocator@U?$pair@$$CB{1}PEAV{0}@@@std@@@2@@std@@A",
                                     type, kString);
      address(layout.prototypeNames[i], name.c_str());
   }

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
      const char* include[] = {"includeEntitiesCheckbox", "includeModulesCheckbox", "includeTilesCheckbox",
         "includeStationNamesCheckbox", "includeTrainsCheckbox", "includeFuelCheckbox", "includeVehiclesCheckbox"};
      for (int i = 0; i < 7; ++i)
         offset(layout.blueprintInclude[i], "BlueprintSettingsGui", include[i]);
   }
   offset(layout.editableLabelText, "EditableLabel", "label");
   offset(layout.editableLabelField, "EditableLabel", "labelEdit");
   offset(layout.editableLabelButton, "EditableLabel", "switchEditLabelMode");

   offset(layout.pictureBlueprint, "BlueprintWidget", "blueprint");
   offset(layout.pictureParameters, "BlueprintWidget", "blueprintParameters");
   offset(layout.pictureSelection, "BlueprintWidget", "blueprintSelection");
   offset(layout.pictureEditEnabled, "BlueprintWidget", "editEnabled");
   offset(layout.pictureScale, "BlueprintWidget", "renderParameters.scale");
   offset(layout.pictureViewLeftTop, "BlueprintWidget", "renderParameters.boundingBox.leftTop");
   offset(layout.picturePlayer, "BlueprintWidget", "context.player");
   classSlot(layout.adapterShowEntityInfo, "GameAdapter", "getShowEntityInfo");
   address(layout.blueprintSelectionAt,
           "?selectionFromPosition@Blueprint@@QEBA?AVBlueprintSelectionResult@@AEBVMapPosition@@AEBVSetupBlueprintParameters@@@Z");
   address(layout.picturePixelShift, "?getPixelShift@BlueprintWidget@@AEBA?AVPixelPosition@@AEBVPoint@agui@@@Z");
   size(layout.selectionResultSize, "BlueprintSelectionResult");
   offset(layout.selectionEntity, "BlueprintSelectionResult", "entity");
   offset(layout.selectionTile, "BlueprintSelectionResult", "tileID");
   offset(layout.selectionIndex, "BlueprintSelectionResult", "index");
   offset(layout.parametersEntities, "SetupBlueprintParameters", "entitiesData");
   offset(layout.parametersTiles, "SetupBlueprintParameters", "tilesData");
   offset(layout.blueprintEntityList, "Blueprint", "entities.data");
   size(layout.entityDataSize, "BlueprintEntities::EntityData");
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
   offset(layout.playerControllerBeforePause, "Player", "controllerManager.controllerBeforePause");
   offset(layout.mapForces, "Map", "forceManager.sortedForceDataList.begin_");
   offset(layout.forceRecipes, "ForceData", "recipes");
   offset(layout.recipeInstances, "Recipes", "indexToInstance");
   size(layout.recipeSize, "Recipe");
   offset(layout.recipeEnabled, "Recipe", "enabled");
   size(layout.itemFilterSize, "IDWithQualityFilter<ID<ItemPrototype,unsigned short> >");
   offset(layout.itemFilterId, "IDWithQualityFilter<ID<ItemPrototype,unsigned short> >", "baseID");
   offset(layout.itemFilterQuality, "IDWithQualityFilter<ID<ItemPrototype,unsigned short> >",
          "qualityCondition.qualityID");
   offset(layout.inserterFilters, "Inserter", "filter");
   offset(layout.inserterFlags, "Inserter", "flags");
   offset(layout.splitterLogic, "Splitter", "leftLogic");
   offset(layout.laneSplitterLogic, "LaneSplitter", "logic");
   offset(layout.splitterInputLocked, "SplitterLogic", "inputLocked");
   offset(layout.splitterOutputLocked, "SplitterLogic", "outputLocked");
   offset(layout.splitterTakeFrom, "SplitterLogic", "takeNextItemFrom");
   offset(layout.splitterGoesTo, "SplitterLogic", "nextItemGoesTo");
   offset(layout.splitterFilter, "SplitterLogic", "filter");
   enumerator(layout.splitterRight, "SplitterDirection", "Right");
   offset(layout.loaderFilters, "Loader", "filter");
   offset(layout.loaderFilterMode, "Loader", "filterMode");
   offset(layout.loaderType, "Loader", "type");
   offset(layout.loaderPerLane, "LoaderPrototype", "perLaneFilters");
   enumerator(layout.loaderWhitelist, "Loader::FilterMode", "Whitelist");
   enumerator(layout.loaderBlacklist, "Loader::FilterMode", "Blacklist");
   enumerator(layout.loaderOutput, "LoaderType", "Output");
   offset(layout.undergroundType, "UndergroundBelt", "type");
   enumerator(layout.undergroundOutput, "UndergroundBeltType", "Output");
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
   offset(layout.constantSignals, "ConstantCombinator", "controlBehavior.sections.compiled");
   size(layout.compiledFilterSize, "CompiledLogisticFilter");
   {
      const char* arithmetic[] = {"Multiply", "Divide",     "Add", "Subtract", "Modulo", "Power",
                                  "LeftShift", "RightShift", "AND", "OR",       "XOR"};
      for (size_t i = 0; i < std::size(arithmetic); ++i)
         enumerator(layout.arithmeticOperations[i], "ArithmeticCombinatorParameters::Operation", arithmetic[i]);
      const char* comparisons[] = {"GreaterThan", "LessThan", "Equals", "GreaterOrEqual", "LessOrEqual", "NotEqual"};
      for (size_t i = 0; i < std::size(comparisons); ++i)
         enumerator(layout.comparisons[i], "Comparison::Enum", comparisons[i]);
      const char* selector[] = {"Select",        "Count",    "Random", "QualityTransfer", "StackSize",
                                "RocketCapacity", "QualityFilter", "Time", "QualitySelect"};
      for (size_t i = 0; i < std::size(selector); ++i)
         enumerator(layout.selectorOperations[i], "SelectorCombinatorParameters::Operation", selector[i]);
   }
   enumerator(layout.selectorSelect, "SelectorCombinatorParameters::Operation", "Select");
   enumerator(layout.selectorCount, "SelectorCombinatorParameters::Operation", "Count");
   offset(layout.signalQuality, "IDWithQuality<SignalIDBase>", "qualityID");
   address(layout.signalPrototype, "?getPrototypeSafe@SignalIDBase@@QEBAPEBVPrototypeBase@@XZ");
   offset(layout.pumpFilter, "Pump", "fluidBox.buffer.filter.fluidID");
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
   address(layout.bookRecordActiveIndex,
           "?getActiveIndex@BlueprintBookRecord@@QEBAGPEBVPlayer@@PEAVLatencyState@@@Z");
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
   offset(layout.bookHeaderName, "BlueprintBookHeader", "nameLabel");
   offset(layout.bookHeaderRename, "BlueprintBookHeader", "editButton");

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
   offset(layout.electricNetworkBars, "ElectricNetworkGuiWindow<ElectricPole>", "satisfactionFlow");
   offset(layout.electricNetworkFlows, "ElectricNetworkGuiWindow<ElectricPole>", "gui");
   offset(layout.electricNetworkConsumption, "ElectricNetworkGuiWindow<ElectricPole>", "gui.inputFrame");
   offset(layout.electricNetworkProduction, "ElectricNetworkGuiWindow<ElectricPole>", "gui.outputFrame");
   offset(layout.electricNetworkStorage, "ElectricNetworkGuiWindow<ElectricPole>", "gui.storageFrame");
   offset(layout.flowFrameGraph,
          "FlowDataFrame<FlowStatistics<IDWithQuality<ID<EntityPrototype,unsigned short> >,double,ElectricityTag>,"
          "ElectricPole,FlowGuiEnabler<0> >",
          "graph");
   offset(layout.relativeWrapperTable, "CustomGuiGameGuiWrapper", "table");
   offset(layout.relativeWrapperTop, "CustomGuiGameGuiWrapper", "topFlow");
   offset(layout.relativeWrapperLeft, "CustomGuiGameGuiWrapper", "leftFlow");
   offset(layout.relativeWrapperRight, "CustomGuiGameGuiWrapper", "rightFlow");
   offset(layout.relativeWrapperBottom, "CustomGuiGameGuiWrapper", "bottomFlow");

   offset(layout.gameViewControllerView, "GameView", "controllerView");
   classSlot(layout.controllerViewQuickBar, "ControllerView", "getQuickBar");
   offset(layout.quickBarMainRows, "QuickBarGui", "mainWindowRows");
   offset(layout.quickBarPickerRows, "QuickBarGui", "pageSelectorRows");
   offset(layout.quickBarPicker, "QuickBarGui", "pageSelectorFrame");
   offset(layout.quickBarPickingFor, "QuickBarGui", "selectingNewPageForRow");
   offset(layout.rowPage, "QuickBarGui::RowWidgets", "pageIndex");
   offset(layout.rowButton, "QuickBarGui::RowWidgets", "button");
   offset(layout.rowSlots, "QuickBarGui::RowWidgets", "slots");
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
   offset(layout.goalLabel, "GoalDescription", "label");
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
   offset(layout.hoverableLabelManager, "LabelWithHoverableRichText", "hoverManger");
   offset(layout.hoverManagerTooltip, "RichTextHoverManager", "hoverTooltip");
   address(layout.richTextHandleHover,
           "?handleHover@RichTextHoverManager@@IEAAXAEBVTextDrawSection@@PEBVItem@OutputConsole@@_N@Z");
   address(layout.richTextClearTooltip, "?clearTooltip@RichTextHoverManager@@QEAAXXZ");
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
   address(layout.technologyNameWithLevel, "?getLocalisedNameWithLevel@TechnologyPrototype@@QEBA?AVLocalisedString@@I@Z");
   address(layout.researchProgress, "?getProgress@ResearchManager@@QEBANAEBVTechnology@@@Z");
   address(layout.localisedStringFromKey, "??0LocalisedString@@QEAA@PEBD@Z");
   address(layout.localisedStringLiteral, "??0LocalisedString@@QEAA@W4Mode@0@PEBD@Z");
   address(layout.localisedStringWithParameters[0],
           "??0LocalisedString@@QEAA@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV0@@Z");
   address(layout.localisedStringWithParameters[1],
           "??0LocalisedString@@QEAA@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV0@1@Z");
   address(layout.localisedStringWithParameters[2],
           "??0LocalisedString@@QEAA@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV0@11@Z");
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
   address(layout.tileInfoDestroy, "??_G?$SelectedInfo@VTile@@V?$ObjectButton@VTile@@VEmptyWidget@agui@@@@@@UEAAPEAXI@Z");
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

   if (ok && (layout.chartSelectionSize > kChartSelectionCapacity || layout.patchInfoSize > kPatchInfoCapacity)) {
      log::error("A chart structure outgrew its buffer: ChartSelection {} bytes, ResourcePatchInfo {}",
                 layout.chartSelectionSize, layout.patchInfoSize);
      ok = false;
   }
   if (ok && (layout.simpleBuildInputSize > kSimpleBuildInputCapacity ||
              layout.buildingModifierSize > kBuildingModifierCapacity || layout.buildIdSize > kBuildIdCapacity ||
              layout.buildCheckDataSize > kBuildCheckDataCapacity ||
              layout.buildCheckResultSize > kBuildCheckResultCapacity ||
              layout.iteratorSize > kEntityIteratorCapacity)) {
      log::error("A building structure outgrew its stack buffer: SimpleBuildInput {} bytes, BuildingModifier {}, "
                 "BuildID {}, BuildCheckData {}, BuildCheckResult {}, HeuristicEntityIterator {}",
                 layout.simpleBuildInputSize, layout.buildingModifierSize, layout.buildIdSize,
                 layout.buildCheckDataSize, layout.buildCheckResultSize, layout.iteratorSize);
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
      log::info("Entity icons: Entity::draw slot {}, DrawQueue {} bytes, renderParameters {:#x}",
                layout.entityDraw, layout.drawQueueSize, layout.drawQueueRenderParameters);
   }
   return ok;
}

} // namespace fa::game
