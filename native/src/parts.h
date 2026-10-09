#pragma once

// The parts of the in-game screen Ctrl+Tab moves between, as F6 moves between the panes of a
// Windows window: whatever is open (the world, the character screen, an entity's window) and the
// parts of the HUD around it, each read by a screen of its own while it is the one in use.
// Ctrl+Shift+Tab goes back. FA's own menus keep Ctrl+Tab for their sections.
namespace fa::parts {

enum class Part {
   None, // back to what is open
   ModWindows, // only while a mod or the scenario shows a window on the screen
   QuickBar,
   ShortcutBar,
   SideMenu,
   MapViewOptions, // in remote view only
   Status,
   CraftingQueue,
   TrackedAchievements,
};

// The part in use, None while it is what is open.
Part current();
// Moves to the next part, or with a negative `direction` the previous one; past the last part comes
// what is open again. Parts that are not available are passed over. Safe from any thread.
void cycle(int direction);
// Whether a part that is not always there (ModWindows) has anything now, as its screen sees it each
// frame. Safe from any thread.
void setAvailable(Part part, bool available);
// Back to what is open.
void close();

} // namespace fa::parts
