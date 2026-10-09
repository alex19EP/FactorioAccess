#pragma once

// The achievement cards (AchievementCard) of an AchievementCardHolder, read alike in the
// achievements window and in the tracked achievements on the HUD.
//
// A card reads its state first, which the game shows only by the card's frame: earned, failed, or
// on a normal card in the window tracked when the player tracks it, and nothing otherwise. Then
// what the card shows: in the window its name, description and progress; on the HUD its icon,
// read as the achievement's name, and its progress. What the game shows only as a tooltip stays
// there, read with Y: a failed card's reason, the track button's "Start tracking", a HUD card's name.
// Enter presses the card's track button where it has one, as the mouse would, and says whether it
// is tracked now.

#include <string>

#include "AguiNodes.hpp"

namespace fa::screens
{

/// Declares a card each, in the holder's order, into the builder's current stop; "empty" when the
/// holder shows none. Keyed by achievement under `prefix`, so that the cursor stays on one as the
/// search hides others. `hud` for the tracked achievements on the HUD.
void AddAchievementCards(graph::GraphBuilder& builder, const std::string& prefix, const agui::Widget* holder, bool hud);

} // namespace fa::screens
