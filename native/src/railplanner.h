#pragma once

// The game's rail planner, as this client draws it. With rails in hand the build press on a rail
// starts planning from that rail's end; the game then searches a route from there to the cursor a
// little each frame, draws it, and builds it on the next build press, through its own input action.
// Building moves the start to the route's end; clear cursor ends planning.
//
// This client says:
// - planning starting, with the direction the route leaves in, and "built" when the start moves to
//   the end of the route just shown;
// - each time the cursor moves to another goal, once the game's search stops: the item counts the
//   game draws beside the cursor, which way the route ends, how far short of the cursor it stops,
//   and "out of reach" when the game draws the end that way; or "no route" when it draws none;
// - planning ending.
// It only reads: the route is the game's, built by the game's own click.
namespace fa::railplanner {

// Whether the rail planner is drawing on this thread: the pieces it draws are its route, not the
// item in hand's preview.
bool drawing();

// MinHook detours for BuildingRenderer::drawRailPlanBuilder, BuildingRenderer::drawRailPlanArrow
// and RenderUtil::drawItemCounts, and where MinHook keeps the originals.
void* drawPlanDetour();
void** drawPlanOriginal();
void* drawArrowDetour();
void** drawArrowOriginal();
void* drawCountsDetour();
void** drawCountsOriginal();

} // namespace fa::railplanner
