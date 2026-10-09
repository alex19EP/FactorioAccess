#pragma once

// The parts a blueprint book's window (BlueprintBookGui) shares with a book opened in the blueprint
// library (BlueprintBookRecordWidget), and the lists of blueprints both and the library's shelves
// show (BlueprintsList).

#include <map>
#include <string>

#include "AguiNodes.hpp"

namespace fa::screens
{

struct BookParts
{
    const agui::Widget* name = nullptr;        // the book's name, an agui::Label
    const agui::Widget* rename = nullptr;      // the button editing its name, description and icons
    const agui::Widget* description = nullptr; // an agui::Label, hidden when empty
    // The flow of rows above the book's own: where the book is, then a row per book it is inside,
    // each an arrow beside the name of where it goes.
    const agui::Widget* navigation = nullptr;
};

/// The book stop: the name with the rename button in a row, the description, then the way up.
void AddBook(graph::GraphBuilder& builder, const BookParts& book);

/// A stop of the buttons in a frame's subheader, in a row.
void AddSubheaderButtons(graph::GraphBuilder& builder, const std::string& key, const agui::Widget* frame);

/// The slots of a BlueprintsList, laid out as the game lays them out in the player's view mode: in
/// List view one to a row with the description read after the slot (the table's columns hold the
/// lines between items too), in Grid and Slots view the table's rows. Keyed by their place in the
/// list, so the cursor keeps its slot when the view changes. Adds to the current stop, and returns
/// how many slots it added. A library's list also puts each record's key in `recordKeys`.
using RecordKeys = std::map<agui::RecordId, std::string>;
int AddBlueprintList(graph::GraphBuilder& builder, const std::string& key, const agui::Widget* list,
    RecordKeys* recordKeys = nullptr);

/// A stop of a list's frame subheader: its labels (the hint on cycling through a book, the library's
/// warnings), then the List, Grid and Slots buttons in a row.
void AddListView(graph::GraphBuilder& builder, const agui::Widget* frame);

} // namespace fa::screens
