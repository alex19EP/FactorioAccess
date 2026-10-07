#pragma once

#include <string>
#include <string_view>

namespace fa::text {

// Turns Factorio GUI text into something a screen reader can say: drops formatting tags such as
// [color=red]...[/color] and [font=...], reads every icon ([item=iron-plate], [img=item/wood],
// [img=utility/clock]) as its localised name where it has one and as its own name otherwise, and
// collapses whitespace. An icon beside its own name reads the name once. Translating names
// touches the game's locale cache, so this runs where the game itself translates (GUI and game
// hooks), never on a thread of our own.
std::string speakable(std::string_view text);

} // namespace fa::text
