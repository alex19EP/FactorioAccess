#pragma once

// The full map (Player::renderMode CHART): what a sighted player learns there by pointing.
//
// The map selects no entity in the world. It selects what it draws at the cursor
// (ChartSelectionLogic): a map tag, a vehicle, a display panel shown on the map, or else the
// resource patch under the cursor, which it outlines and labels with what is left in it. While the
// mod drives the cursor, this client says each new selection after what the mod says of the move.
namespace fa::chart {

// Every frame, on the main thread.
void tick();

} // namespace fa::chart
