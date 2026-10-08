#include "vocab.h"

#include <format>

namespace fa::vocab {

std::string position(int index, int count) { return std::format("{} of {}", index, count); }

std::string expandedState(bool expanded) { return expanded ? "expanded" : "collapsed"; }

std::string flyoutHint(int count) { return std::format("submenu, {} {}", count, count == 1 ? "item" : "items"); }

std::string unlocked(unsigned count) { return std::format("{} unlocked", count); }

std::string_view alertCategory(uint8_t category) {
   constexpr std::string_view names[] = {kAttack, kConstruction, kPlatformConstruction, kCustom,
                                         kLogistics, kTrains, kPipelines};
   return category < std::size(names) ? names[category] : std::string_view();
}

} // namespace fa::vocab
