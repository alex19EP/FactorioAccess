#pragma once

// A dialog the game opens over a loaded game that no recipe claims: a Dialog<Result> or a
// FloatingGuiWindow (a planner's name, description and icons). A blueprint's setup has its own
// screen. Its content is read generically in its title's context as one stop, a Dialog's footer
// (Confirm, Back) as a second. Escape and the close button go to the game, which closes it
// unchanged.

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class GameDialogScreen final : public EntityWindowScreen
{
protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
