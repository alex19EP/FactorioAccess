#pragma once

#include <cstddef>

// The map editor's own actions (MapEditorActionHandler), said as this client's player makes them:
// the editor's keys change what the game shows without a window of their own.
//
// - Pause, speed up, speed down and reset speed say the speed as the Time editor shows it, with
//   "(paused)" while the map is paused. Tick once says the tick the map reaches.
// - Next and previous variation read the cursor's tile again, with its new look.
// - The clone brush's source and destination say they were set.
// - Removing a scripting area or position says which, by its name.
// - Switching surface says the surface's name.
//
// The game applies every player's actions on its update; only the local player's are said.
namespace fa::editoractions {

// Each frame, for this client's player: says the tick a tick once reached, once the map got there.
void frame(const std::byte* player);

// MinHook detour for MapEditorActionHandler::actionPerformed, and where MinHook keeps the original.
void* actionPerformedDetour();
void** actionPerformedOriginal();

} // namespace fa::editoractions
