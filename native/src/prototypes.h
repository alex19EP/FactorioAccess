#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace fa::prototypes {

// The localised name, in the game's current locale, of the prototype that rich text names as
// `kind` and `name`: a tag ([item=iron-plate]) or a sprite path (item/iron-plate). Nothing when
// no such prototype exists or `kind` names none (utility sprites, GPS tags).
std::optional<std::string> localisedName(std::string_view kind, std::string_view name);

} // namespace fa::prototypes
