#pragma once

// Any window of the game's over a loaded game (a GameGui) that no recipe claims: a combinator's, a
// display panel's, a speaker's, a power switch's, a lamp's. Its content is read as one stop in its
// title's context, the logistics panel of a constant combinator among it, then the title bar's
// buttons, as an entity's window is.
//
// Registered before the screens of the game's other windows, which take a window they share with it.

#include <vector>

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class GameWindowScreen final : public EntityWindowScreen
{
public:
    /// A screen whose windows this one leaves alone.
    void Leave(const EntityWindowScreen* recipe) { _recipes.push_back(recipe); }

    bool Handles(const agui::Widget* window) const override;

protected:
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;

private:
    std::vector<const EntityWindowScreen*> _recipes;
};

} // namespace fa::screens
