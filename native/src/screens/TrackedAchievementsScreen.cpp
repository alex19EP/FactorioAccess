#include "TrackedAchievementsScreen.hpp"

#include <string>

#include "AchievementCards.hpp"
#include "parts.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;

// The tracked achievements, while they are on screen over a loaded game with no menu in front of
// them.
const Widget* FindHolder()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return nullptr;
    return agui::trackedAchievements();
}

} // namespace

std::string TrackedAchievementsScreen::Name() const { return vocab::kTrackedAchievements; }

bool TrackedAchievementsScreen::IsActive()
{
    if (parts::current() != parts::Part::TrackedAchievements)
        return false;
    _holder = FindHolder();
    // Nothing to go back to once the game is gone.
    if (!agui::inGame())
        parts::close();
    return _holder != nullptr;
}

void TrackedAchievementsScreen::Build(graph::GraphBuilder& builder)
{
    if (!_holder || FindHolder() != _holder)
        return;
    builder.BeginStop("tracked");
    AddAchievementCards(builder, "tracked", _holder, true);
}

void TrackedAchievementsScreen::OnEscape() { parts::close(); }

void TrackedAchievementsScreen::OnPop() { _holder = nullptr; }

std::string TrackedAchievementsScreen::LeaveLine() const
{
    // Left with Ctrl+Tab or Escape, not covered by a menu: the part is no longer in use.
    return parts::current() == parts::Part::None ? std::string(vocab::kMap) : std::string();
}

} // namespace fa::screens
