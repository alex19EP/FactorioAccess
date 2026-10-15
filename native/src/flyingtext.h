#pragma once

#include "cursormoves.h"

#include <cstdint>
#include <optional>

// Flying text: the short messages the game floats up from the map ("Cannot reach", "+5 Iron plate
// (12)") or from the mouse over a window ("Not enough ingredients"), and those other mods make with
// LuaPlayer::create_local_flying_text. Lua can't read them. The game makes most of them only for
// this client's player, but the item counts (ShowCounts::showDiff, from mining and transfers) for
// every player's actions on every client, at that player's position. So a text over the map is said
// only where the view draws it, as a sighted player sees it; a text over the GUI as it is made.
namespace fa::flyingtext {

// What the view draws map texts over: the surface (the engine's SurfaceIndex) and the tiles on the
// screen.
struct Shown {
   uint32_t surface;
   cursormoves::Screen screen;
};

// Once per frame, on the main thread: says the map texts made since the last frame that land where
// `shown` is, and drops the rest. Empty `shown` (no view, or the full map, which draws none) drops
// them all.
void frame(const std::optional<Shown>& shown);

// MinHook detours for Map::addLocalFlyingText, where every text over the map lands, and for the
// construction of an agui::GuiFlyingText, every text over the GUI; and where MinHook keeps the
// originals.
void* mapDetour();
void** mapOriginal();
void* guiDetour();
void** guiOriginal();

} // namespace fa::flyingtext
