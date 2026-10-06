#pragma once

namespace fa::hooks {

// Hooks the game functions resolved into game::layout. Returns false, with every hook removed,
// if any of them could not be installed.
bool install();

} // namespace fa::hooks
