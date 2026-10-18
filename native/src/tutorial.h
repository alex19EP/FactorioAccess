#pragma once

// Runs the mod's Lua in the game's tutorials (the Tips and tricks "Play tutorial" levels, from
// data/base/tutorials). LuaContext's constructor makes no mod script while
// TutorialManager::activeTutorial is set, so without this none of FA's keys, scanner or speech
// works there. The constructor is run with no active tutorial and only FactorioAccess and
// Kruise_Kontrol in its mod list: tutorials assume a vanilla game, and other mods could change
// their steps. A tutorial is single player, so no other peer runs a different set of scripts.
namespace fa::tutorial {

// MinHook detour for the LuaContext constructor, and where MinHook keeps the original.
void* luaContextDetour();
void** luaContextOriginal();

} // namespace fa::tutorial
