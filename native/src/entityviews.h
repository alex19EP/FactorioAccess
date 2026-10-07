#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// The mod's own views of the entity whose window is open: what the game's window does not show,
// such as what a belt carries. The mod's Lua sends them when the window opens, already translated,
// and the entity's screen reads them after the game's own stops, a stop per view.
//
// Lua sends them between begin and end; nothing shows until end, so a screen never reads half a
// set. Each view is a grid of columns read top to bottom, side by side.
namespace fa::entityviews {

struct Column {
   std::string title; // read as focus enters the column; may be empty
   std::vector<std::string> cells;
};

struct View {
   std::string title;
   std::vector<Column> columns;
};

struct Views {
   uint64_t unitNumber = 0; // the entity they describe (LuaEntity::unit_number)
   std::vector<View> views;
};

// From the mod's Lua, on whatever thread runs it.
void begin(uint64_t unitNumber);
void addView(std::string title);
void addColumn(std::string title, std::vector<std::string> cells);
void end();

// The views last ended, or null before any.
std::shared_ptr<const Views> current();

} // namespace fa::entityviews
