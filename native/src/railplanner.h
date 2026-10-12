#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The game's rail planner, as this client draws it. With rails in hand the build press on a rail
// starts planning from that rail's end; the game then searches a route from there to the cursor a
// little each frame, draws it, and builds it on the next build press, through its own input action.
// Building moves the start to the route's end; clear cursor ends planning.
//
// This client says:
// - planning starting, with the direction the route leaves in and whether it starts elevated, and
//   "built" when the start moves to the end of the route just shown;
// - each time the cursor moves to another goal, once the game's search stops: the item counts the
//   game draws beside the cursor, which way the route ends and whether elevated, that it joins the
//   rail under the cursor or how far short of the cursor it stops, "out of reach" when the game
//   draws the end that way, and "as ghosts" when it plans ghosts; or "no route" when it draws none;
// - planning ending.
// The route itself, rail by rail, is there for the mod's route key (route()).
// It only reads: the route is the game's, built by the game's own click.
namespace fa::railplanner {

// Where a route reaches, in map position units (1/256 tile), the way it leads on from there (0 to
// 15) and whether it is elevated there.
struct RoutePoint {
   int32_t x = 0;
   int32_t y = 0;
   uint8_t direction = 0;
   bool elevated = false;
};

// One rail of the route: its prototype's type ("straight-rail", "curved-rail-a", "rail-ramp") and
// the end it leads on from.
struct RoutePiece {
   std::string type;
   RoutePoint end;
};

// The route the game draws, from the planner's start, in the order the rails follow each other.
struct Route {
   RoutePoint start;
   std::vector<RoutePiece> pieces;
};

// The route drawn last while this client plans; nothing when it does not plan. Empty pieces: no
// route.
std::optional<Route> route();

// Whether the rail planner is drawing on this thread: the pieces it draws are its route, not the
// item in hand's preview.
bool drawing();

// MinHook detours for BuildingRenderer::drawRailPlanBuilder, BuildingRenderer::drawRailPlanArrow,
// RenderUtil::drawItemCounts and RailPlanFinder::getBestSolution, and where MinHook keeps the
// originals.
void* drawPlanDetour();
void** drawPlanOriginal();
void* drawArrowDetour();
void** drawArrowOriginal();
void* drawCountsDetour();
void** drawCountsOriginal();
void* bestSolutionDetour();
void** bestSolutionOriginal();

} // namespace fa::railplanner
