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
   X(playerCursorPosition, world::playerCursorDetour(), world::playerCursorOriginal(),                                 \
     "Player::getCursorMapPosition")                                                                                   \
   X(sourceCursorPosition, world::sourceCursorDetour(), world::sourceCursorOriginal(),                                 \
     "PlayerInputSource::getCursorMapPosition")                                                                        \
   X(simpleBuildInput, world::simpleBuildInputDetour(), world::simpleBuildInputOriginal(),                             \
     "Player::getSimpleBuildInput")                                                                                    \
   X(prepareBuildingInGame, world::prepareBuildingDetour(), world::prepareBuildingOriginal(),                          \
     "BuildingRenderer::prepareBuildingInGame")                                                                        \
   X(playerBuildFromCursor, world::buildFromCursorDetour(), world::buildFromCursorOriginal(),                          \
     "Player::buildFromCursor")                                                                                        \
   X(settingsDraw, world::settingsDrawDetour(), world::settingsDrawOriginal(), "EntityToBeBuiltSettings::draw")        \
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
   X(characterChangePosition, movement::changePositionDetour(), movement::changePositionOriginal(),                     \
     "Character::changePosition")
