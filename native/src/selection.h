#pragma once

// Selection tools take two presses while the mod drives the cursor: a select press starts the
// selection at the cursor, and the next select press, after the cursor has moved to the other
// corner, finishes it. Releasing the key does nothing. This is the game's own gamepad selection,
// which this client gets while the input method stays keyboard and mouse: the game keeps the mode
// of the first press and sends the finished selection as an ordinary input action. Escape cancels
// an open selection before the game reads Escape for anything else. With the mouse back in charge
// the game's own drag selection applies.
namespace fa::selection {

// MinHook detours for PlayerInputSource::expectedSelectionModeFromInputs,
// PlayerInputSource::processSelectionToolCommon and PlayerInputSource::processActions, and where
// MinHook keeps the originals.
void* expectedModeDetour();
void** expectedModeOriginal();
void* selectionToolDetour();
void** selectionToolOriginal();
void* processActionsDetour();
void** processActionsOriginal();

} // namespace fa::selection
