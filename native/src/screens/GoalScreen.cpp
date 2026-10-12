#include "GoalScreen.hpp"

#include <string>

#include "AguiNodes.hpp"
#include "parts.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;

// The goal window, while it shows a goal over a loaded game with no menu in front of it.
const Widget* FindGoal()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return nullptr;
    const Widget* goal = agui::goalFrame();
    return goal && HasContent(goal) ? goal : nullptr;
}

} // namespace

std::string GoalScreen::Name() const { return vocab::kGoal; }

bool GoalScreen::IsActive()
{
    _goal = FindGoal();
    parts::setAvailable(parts::Part::Goal, _goal != nullptr);
    if (parts::current() != parts::Part::Goal)
        return false;
    // Nothing to go back to once the game is gone.
    if (!agui::inGame())
        parts::close();
    return _goal != nullptr;
}

void GoalScreen::Build(graph::GraphBuilder& builder)
{
    if (!_goal || FindGoal() != _goal)
        return;
    builder.BeginStop("goal");
    AddSubtree(builder, "goal", _goal);
}

void GoalScreen::OnEscape() { parts::close(); }

void GoalScreen::OnPop() { _goal = nullptr; }

std::string GoalScreen::LeaveLine() const
{
    // Left with Ctrl+Tab or Escape, not covered by a menu: the part is no longer in use.
    return parts::current() == parts::Part::None ? std::string(vocab::kMap) : std::string();
}

} // namespace fa::screens
