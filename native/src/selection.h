#pragma once

// Selection tools take two presses while the mod drives the cursor: a select press starts the
// selection at the cursor, and the next select press, after the cursor has moved to the other
// corner, finishes it. Releasing the key does nothing. This is the game's own gamepad selection,
// which this client gets while the input method stays keyboard and mouse: the game keeps the mode
// of the first press and sends the finished selection as an ordinary input action. Escape cancels
// an open selection before the game reads Escape for anything else. With the mouse back in charge
// the game's own drag selection applies.
//
// While a selection is open, each time the cursor corner moves to another tile this client says
// the box's size in tiles and the counts the game draws beside it, largest first: what a copy or
// blueprint would build, what an upgrade planner would upgrade and to what, what a deconstruction
// planner would remove and the items that would give. They follow what the mod says of the move.
// A deconstruction or upgrade planner's selection, once finished, says what it did (marked or
// cancelled) and the counts it showed last; the game shows no total of its own, nor one for the
// marks a blueprint makes as it is built.
namespace fa::selection {

// MinHook detours for PlayerInputSource::expectedSelectionModeFromInputs,
// PlayerInputSource::processSelectionToolCommon, PlayerInputSource::processDeselection,
// PlayerInputSource::processActions and SelectionToolRenderer::drawSelectionCounts, and where
// MinHook keeps the originals.
void* expectedModeDetour();
void** expectedModeOriginal();
void* selectionToolDetour();
void** selectionToolOriginal();
void* deselectionDetour();
void** deselectionOriginal();
void* processActionsDetour();
void** processActionsOriginal();
void* drawCountsDetour();
void** drawCountsOriginal();

} // namespace fa::selection
