#pragma once

#include <string>
#include <vector>

// FA's own facts about an entity, which the game's tooltip lacks: which way it faces, a belt's
// shape, an underground belt's partner. The tile reader says them after the entity's name, in the
// mod's own words (the keys the mod's Lua said them with), each part a list item.
namespace fa::facts {

// In the application Gui's logic, where the game translates.
std::vector<std::string> describe(const void* entity);

} // namespace fa::facts
