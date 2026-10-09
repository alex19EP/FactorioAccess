#pragma once

#include "input.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The scanner: lists what is on the player's surface by category (production, logistics,
// resources ...), subcategory (assemblers making gears, iron chests holding coal ...) and entry, and
// moves through the list with PageUp, PageDown, Home and their modifiers.
//
// Only this client has the list. A move must still reach every client alike, because the mod moves
// its cursor (storage) and the player's selection there. So the move rides on the key's own custom
// input: when a scanner key reaches the game, the game's cursor reads as the entry's position while
// the game turns the key into its input action, which carries that position to every client as the
// event's cursor_position. The mod's Lua moves to it, the same on every client, and asks entryAt()
// what to say, which only this client can.
namespace fa::scanner {

// What the mod's refresh hands in. From Lua, inside the game's update: the world stands still.
struct Refresh {
   int playerIndex = 0;           // LuaPlayer::index
   uint32_t surfaceIndex = 0;     // LuaSurface::index
   double x = 0;                 // where distances are measured from, in tiles
   double y = 0;
   double radius = 0;             // how far the scanner sees, in tiles
   std::optional<int> direction;  // only entries in this direction (an 8-way defines.direction)
};

// Rebuilds the list for this client's player. Returns whether it did: false for another client's.
bool refresh(const Refresh& request);

// Whether the mod's own UI has the keys, so the scanner keys are not the scanner's. From Lua.
void setModUiOpen(int playerIndex, bool open);

// A key the game is about to handle, or null for any other poll. On the game thread, while the
// world stands still (see input::setKeyObserver).
void observeKey(const input::KeyEvent* key);

// What a scanner key moved onto, for the mod to say.
struct Entry {
   std::string category;       // the category's key (scanner-consts.lua CATEGORIES)
   bool edge = false;          // the move found nothing further that way and stayed
   bool empty = false;         // nothing in the category: the fields below are unset
   uint32_t index = 0;         // one-based place in its subcategory
   uint32_t count = 0;         // entries in the subcategory
   std::string prototype;      // the entity's prototype name
   double x = 0;               // where it is, in tiles
   double y = 0;
   double originX = 0;         // where the list was sorted from
   double originY = 0;
};

// The move whose key carried `x`, `y` (event.cursor_position) for this client's player, if it is
// one of the latest.
std::optional<Entry> entryAt(int playerIndex, double x, double y);

// The category the cursor is in after a category key, and whether that key found nothing further.
struct Category {
   std::string category;
   bool edge = false;
};
std::optional<Category> category(int playerIndex);

} // namespace fa::scanner
