#pragma once

#include <string>
#include <string_view>

namespace fa::text {

// Turns Factorio GUI text into something a screen reader can say: drops formatting tags such as
// [color=red]...[/color] and [font=...], reads icon tags like [item=iron-plate] as their name, and
// collapses whitespace.
std::string speakable(std::string_view text);

} // namespace fa::text
