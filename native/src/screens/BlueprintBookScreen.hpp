#pragma once

// A blueprint book's window (BlueprintBookGui), opened by a right click on the book in a slot,
// beside the player's inventory.
//
// Four stops, then the inventory. The book: its name with the rename button in a row, its
// description when it has one, then where it is: the go-to-root arrow named by the place ("Inventory:
// name") and an arrow per book it is inside, named by that book, each going up to it. The book's
// buttons: copy, upgrade (clicked holding an upgrade planner), export to a string, destroy. The
// contents: a slot per slot of the book, laid out as the game lays them out in the player's view
// mode (List one to a row with the description read after the slot, Grid and Slots in the table's
// rows); a slot reads as every blueprint slot does, with "active" on the one the book builds from.
// The view: the hint on cycling through the book while holding it, and the List, Grid and Slots
// buttons.
//
// Every control is the game's own, pressed as the Gui presses it: Enter takes or puts a slot's
// item, ] opens it (a blueprint's setup, a book inside the book), as the mouse does.

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class BlueprintBookScreen final : public EntityWindowScreen
{
protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
