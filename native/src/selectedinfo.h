#pragma once

namespace fa::agui {
struct Widget;
}

// The game's info panel for what the player points at: the entity under the cursor, or with no
// entity the tile, which only the map editor describes (Controller::deduceSelectedTile finds none
// elsewhere). It is the panel the game shows beside the mouse (or at the right of the screen),
// filled with what the game says about the entity: its status, recipe, contents, power, health and
// the rest, already translated.
//
// The game builds its own only after the hover delay and only while the "show entity tooltip"
// setting is on, and takes it down when the mouse moves. So the DLL makes one of its own with the
// game's code, kept out of the Gui's widget tree, which nothing draws; it lives while a screen
// reads it. It is filled once, when made, so that a line does not change under the reader: values
// that move (products finished, energy) are read anew by making it again.
namespace fa::selectedinfo {

// The mod's Lua asks for the panel (the Y key in the world). Safe from any thread.
void request();
// Whether a request came since the last call, which clears it.
bool takeRequest();

// The rest is for the main thread, inside the application Gui's logic, in a game.

// Makes the panel for what the local player points at now, closing any open one. Null when there
// is nothing to describe.
const agui::Widget* open();
// The open panel's window, or null.
const agui::Widget* window();
// Whether the player still points at what the open panel describes. When not, or when that is
// gone, closes the panel and returns false.
bool check();
void close();

} // namespace fa::selectedinfo
