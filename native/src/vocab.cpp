#include "vocab.h"

#include <format>

namespace fa::vocab {

std::string position(int index, int count) { return std::format("{} of {}", index, count); }

std::string expandedState(bool expanded) { return expanded ? "expanded" : "collapsed"; }

std::string flyoutHint(int count) { return std::format("submenu, {} {}", count, count == 1 ? "item" : "items"); }

} // namespace fa::vocab
