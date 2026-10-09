#pragma once

// The blueprint library's window (BlueprintLibraryGui), opened by the game's own key or the
// shortcut bar, beside the player's inventory.
//
// With the shelves showing, four stops then the inventory. The tabs: My blueprints and Game
// blueprints. The records: the title bar's search button (and its field while the search is open,
// filtering every shelf as it is typed in), then each shelf of the chosen tab, laid out as the game
// lays it out in the player's view mode, a slot reading as an inventory's blueprint slot does, then
// "not available yet" while only its preview has arrived, how far its transfer is, and "in hand" on
// the one held; the empty slots padding a shelf read empty, and take what is dropped there. The
// view: the "not synchronised" warning when shown and the List, Grid and Slots buttons. The history:
// the back and forward arrows of the title bar, and the memory warning when shown.
//
// With a book record open (it replaces the shelves), the stops of a book's window: the book (name
// and rename, description, the way up to the shelf and the books it is inside), its buttons (copy,
// upgrade, export, delete), its contents (after the search, which filters them too) with the active
// one marked, the view, then the history. Opening a book lands on its name; leaving it, by the way
// up, the history or deleting it, lands on its slot.
//
// Every control is the game's own, pressed as the Gui presses it: Enter takes a record into the hand
// or drops what is held into a slot, ] opens it (a blueprint's setup, a planner's window, a book in
// the library), Shift+Enter moves it into the inventory, as the mouse does. Ctrl+F is the game's
// own: opening the search lands on its field, and closing it keeps the cursor on its record.

#include <optional>
#include <string>

#include "BlueprintLists.hpp"
#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class BlueprintLibraryScreen final : public EntityWindowScreen
{
public:
    const char* TakeSuggestedLanding() override;
    void OnCursorMoved(const graph::GraphNode& node) override;
    void OnPop() override;

protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;

private:
    void FollowOpenBook(std::optional<agui::RecordId> book, const RecordKeys& shown);

    bool _searching = false;                 // the search field showed at the last build
    std::optional<agui::RecordId> _openBook; // the book record open at the last build, kept across pops
    RecordKeys _recordKeys;                  // the key each record had when last shown, kept as well
    RecordKeys _shown;                       // the records of the last build
    std::string _cursorKey;                  // where the cursor is
    std::string _landing;                    // where the next render lands, once
    std::string _landed;                     // what the last TakeSuggestedLanding returned
};

} // namespace fa::screens
