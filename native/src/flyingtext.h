#pragma once

// Flying text: the short messages the game floats up from the map ("Cannot reach", "+5 Iron plate
// (12)") or from the mouse over a window ("Not enough ingredients"), and those other mods make with
// LuaPlayer::create_local_flying_text. The game makes them only for this client's player and Lua
// can't read them, so each is spoken as the game creates it.
namespace fa::flyingtext {

// MinHook detours for Map::addLocalFlyingText, where every text over the map lands, and for the
// construction of an agui::GuiFlyingText, every text over the GUI; and where MinHook keeps the
// originals.
void* mapDetour();
void** mapOriginal();
void* guiDetour();
void** guiOriginal();

} // namespace fa::flyingtext
