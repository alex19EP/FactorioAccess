#pragma once

// Every function the DLL hooks, as X(field, detour, original, name): `field` is the game::Layout
// member holding the target, `detour` and `original` give the MinHook detour and the slot for the
// original, `name` is the function for logs. hooks::install expands whole entries;
// fa_symbols_check expands only the fields and names, to find calls the compiler inlined past a
// hook, so a hook added here is checked without further work.
#define FA_HOOKS(X)                                                                                                    \
   X(guiLogic, reinterpret_cast<void*>(&detourGuiLogic), reinterpret_cast<void**>(&g_originalGuiLogic),                \
     "agui::Gui::logic")                                                                                               \
   X(sdlPollEvent, input::pollEventDetour(), input::pollEventOriginal(), "SDL_PollEvent")                              \
   X(determineWidgetUnderMouse, agui::underMouseDetour(), agui::underMouseOriginal(), "determineWidgetUnderMouse")     \
   X(playerCursorPosition, world::playerCursorDetour(), world::playerCursorOriginal(), "Player::getCursorMapPosition") \
   X(sourceCursorPosition, world::sourceCursorDetour(), world::sourceCursorOriginal(),                                 \
     "PlayerInputSource::getCursorMapPosition")                                                                        \
   X(gameViewMapPosition, world::mapPositionDetour(), world::mapPositionOriginal(), "GameView::getMapPosition")        \
   X(dragBuildingUpdate, world::dragUpdateDetour(), world::dragUpdateOriginal(), "ClientDragBuildingContext::update")  \
   X(controlInputIsActive, world::isActiveDetour(), world::isActiveOriginal(), "ControlInput::isActive")               \
   X(processZoom, zoom::processZoomDetour(), zoom::processZoomOriginal(), "PlayerInputSource::processZoom")            \
   X(expectedSelectionMode, selection::expectedModeDetour(), selection::expectedModeOriginal(),                        \
     "PlayerInputSource::expectedSelectionModeFromInputs")                                                             \
   X(processSelectionToolCommon, selection::selectionToolDetour(), selection::selectionToolOriginal(),                 \
     "PlayerInputSource::processSelectionToolCommon")                                                                  \
   X(processActions, selection::processActionsDetour(), selection::processActionsOriginal(),                           \
     "PlayerInputSource::processActions")                                                                              \
   X(drawSelectionCounts, selection::drawCountsDetour(), selection::drawCountsOriginal(),                              \
     "SelectionToolRenderer::drawSelectionCounts")                                                                     \
   X(simpleBuildInput, world::simpleBuildInputDetour(), world::simpleBuildInputOriginal(),                             \
     "Player::getSimpleBuildInput")                                                                                    \
   X(prepareBuildingInGame, world::prepareBuildingDetour(), world::prepareBuildingOriginal(),                          \
     "BuildingRenderer::prepareBuildingInGame")                                                                        \
   X(playerBuildFromCursor, world::buildFromCursorDetour(), world::buildFromCursorOriginal(),                          \
     "Player::buildFromCursor")                                                                                        \
   X(settingsDraw, world::settingsDrawDetour(), world::settingsDrawOriginal(), "EntityToBeBuiltSettings::draw")        \
   X(renderCursorBox, highlights::renderCursorBoxDetour(), highlights::renderCursorBoxOriginal(),                      \
     "RenderUtil::renderCursorBox")                                                                                    \
   X(renderDoubleCursorBox, highlights::renderDoubleCursorBoxDetour(), highlights::renderDoubleCursorBoxOriginal(),    \
     "RenderUtil::renderDoubleCursorBox")                                                                              \
   X(adapterRenderCursorBox, highlights::adapterRenderCursorBoxDetour(), highlights::adapterRenderCursorBoxOriginal(), \
     "DrawAdapter::renderCursorBox")                                                                                   \
   X(adapterDestroy, highlights::adapterDestroyDetour(), highlights::adapterDestroyOriginal(), "DrawAdapter::destroy") \
   X(adapterSetDirection, highlights::adapterSetDirectionDetour(), highlights::adapterSetDirectionOriginal(),          \
     "DrawAdapter::setDirectionAndMirroring")                                                                          \
   X(drawPoleConnections, highlights::drawPoleConnectionsDetour(), highlights::drawPoleConnectionsOriginal(),          \
     "ElectricEnergySource::drawPoleConnections")                                                                      \
   X(findMatchingNetwork, highlights::findMatchingNetworkDetour(), highlights::findMatchingNetworkOriginal(),          \
     "LogisticManager::findMatchingNetworkByPosition")                                                                 \
   X(roboportPostPrepare, highlights::roboportPostPrepareDetour(), highlights::roboportPostPrepareOriginal(),          \
     "RoboportInfoRenderer::postPrepare")                                                                              \
   X(drawOnTilesBetween, highlights::drawOnTilesBetweenDetour(), highlights::drawOnTilesBetweenOriginal(),             \
     "RenderUtil::drawOnTilesBetween")                                                                                 \
   X(entityDrawAlert, entityicons::drawAlertDetour(), entityicons::drawAlertOriginal(), "Entity::drawAlert")           \
   X(drawInfoIcon, entityicons::drawInfoIconDetour(), entityicons::drawInfoIconOriginal(), "DrawQueue::drawInfoIcon")  \
   X(initLuaState, luabridge::initLuaStateDetour(), luabridge::initLuaStateOriginal(), "LuaHelper::initLuaState")      \
   X(versionForDisplay, disclosure::versionDetour(), disclosure::versionOriginal(),                                    \
     "ApplicationVersion::strDetailedNoBuildMode")                                                                     \
   X(addLocalFlyingText, flyingtext::mapDetour(), flyingtext::mapOriginal(), "Map::addLocalFlyingText")                \
   X(constructGuiFlyingText, flyingtext::guiDetour(), flyingtext::guiOriginal(), "the GuiFlyingText construct")        \
   X(outputConsoleAdd, console::addDetour(), console::addOriginal(), "OutputConsole::add")                             \
   X(tipNotificationButton, popups::tipDetour(), popups::tipOriginal(),                                                \
     "the TipsAndTricksNotificationButton constructor")                                                                \
   X(speechBubbleGui, popups::speechBubbleDetour(), popups::speechBubbleOriginal(), "the SpeechBubbleGui constructor") \
   X(infoBoxManagerUpdate, popups::infoBoxesDetour(), popups::infoBoxesOriginal(), "InfoBoxManager::update")           \
   X(characterChangePosition, movement::changePositionDetour(), movement::changePositionOriginal(),                    \
     "Character::changePosition")
