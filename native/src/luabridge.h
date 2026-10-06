#pragma once

// The fa_native table the mod's Lua calls into. It is added to every Lua state the game sets up,
// right after the game's own globals (log, localised_print, ...). Nothing in it changes game
// state, so a game where only some clients have the DLL stays in sync as long as the mod never
// lets game state depend on whether fa_native exists.
//
//   fa_native.set_cursor(player_index, x, y)       the FA cursor, a map position in tiles
//   fa_native.release_cursor(player_index)         the game cursor follows the mouse again
//   fa_native.speak(player_index, message)         says a LocalisedString through the screen
//                                                  reader, interrupting, if it is for this client
namespace fa::luabridge {

// MinHook detour for LuaHelper::initLuaState, and where MinHook keeps the original.
void* initLuaStateDetour();
void** initLuaStateOriginal();

} // namespace fa::luabridge
