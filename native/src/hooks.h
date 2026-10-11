#pragma once

namespace fa::hooks {

// Hooks the game functions resolved into game::layout. Returns false, with every hook removed,
// if any of them could not be installed.
bool install();

// Before the core is unloaded: puts every hooked function back and waits until no call that
// entered a detour can still be running in it.
void uninstall();

} // namespace fa::hooks
