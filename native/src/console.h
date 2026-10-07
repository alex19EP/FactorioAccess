#pragma once

// The console's lines: chat, game.print and player.print from the scenario and mods, command
// output, players joining. They show at the bottom left of the screen for a while even with the
// console closed, and Lua sees only chat, so each line is spoken as the game adds it.
namespace fa::console {

// MinHook detour for OutputConsole::add, which every line goes through, and where MinHook keeps
// the original.
void* addDetour();
void** addOriginal();

} // namespace fa::console
