#pragma once

// The fa_native table the mod's Lua calls into. It is added to every Lua state the game sets up,
// right after the game's own globals (log, localised_print, ...). Nothing in it changes game
// state, so a game where only some clients have the DLL stays in sync as long as the mod never
// lets game state depend on whether fa_native exists.
//
//   fa_native.set_cursor(player_index, x, y)       the FA cursor, a map position in tiles
//   fa_native.release_cursor(player_index)         the game cursor follows the mouse again
//   fa_native.speak(player_index, message)         says a LocalisedString through the screen
//                                                  reader, if it is for this client
#include "stub/stub-api.h"

namespace fa::luabridge {

// Hands each fa_native function to the stub, which gives Lua a function of its own that calls it,
// so the Lua states the game keeps across a reload never hold this core's code.
void bind(stub::LuaFunction (*bindLua)(const char* name, stub::LuaFunction target));

// MinHook detour for LuaHelper::initLuaState, and where MinHook keeps the original.
void* initLuaStateDetour();
void** initLuaStateOriginal();

} // namespace fa::luabridge
