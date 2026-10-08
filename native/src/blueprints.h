#pragma once

#include <optional>
#include <string>
#include <vector>

// What the slot of a blueprint, a blueprint book or a planner shows, wherever the slot is (the
// character's inventory, a book, a chest, the blueprint library): the up to four icons its owner
// chose over the item's own icon, and on hover its name and description. Main thread: names are
// translated.
namespace fa::blueprints {

struct Shown {
   std::string label;              // its name, empty when it has none
   std::vector<std::string> icons; // what the icons depict, in the order drawn
   std::string description;
};

// For an item stack's own data (ItemStack::item); nothing for any other kind of item. A book
// without icons shows those of its active item, drawn inside it; a planner without icons shows its
// first filters, as the game draws them.
std::optional<Shown> shown(const void* item);

// The same for a blueprint library record (a BlueprintRecord), drawn as its item would be, for the
// player (a Player) looking: a book without icons shows the record that player builds from.
std::optional<Shown> shownRecord(const void* record, const void* player);

// The name of the item a library record is (blueprint, book, planner).
std::string recordItem(const void* record);

} // namespace fa::blueprints
