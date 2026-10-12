#include "cursor.h"

#include "cues.h"
#include "editoractions.h"
#include "cursormoves.h"
#include "facts.h"
#include "footprint.h"
#include "game.h"
#include "reader.h"
#include "scanner.h"
#include "skip.h"
#include "speech.h"
#include "viewannounce.h"
#include "vocab.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <format>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fa::cursor {

namespace {

using game::layout;

template <class T>
T at(const void* object, uint32_t offset) {
   T value;
   std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
   return value;
}

template <class T>
void put(void* object, uint32_t offset, T value) {
   std::memcpy(static_cast<std::byte*>(object) + offset, &value, sizeof(value));
}

template <class Function>
Function virtualAt(const void* object, uint32_t slot) {
   auto vtable = *static_cast<void* const* const*>(object);
   return reinterpret_cast<Function>(vtable[slot]);
}

struct MsvcString {
   union {
      char buffer[16];
      const char* pointer;
   };
   size_t size;
   size_t capacity;
};

std::string_view view(const MsvcString& string) {
   return {string.capacity >= sizeof(string.buffer) ? string.pointer : string.buffer, string.size};
}

template <class T>
struct MsvcVector {
   T* first;
   T* last;
   T* end;
};

// MapPosition, in the game's fixed point (256 to a tile).
using cursormoves::kScale;
using cursormoves::Pixel;
using cursormoves::Position;
using cursormoves::sameTile;
using cursormoves::Screen;
using cursormoves::tileCentre;
using cursormoves::tileOf;
static_assert(kScale == game::kMapPositionScale);

Position fromTiles(double x, double y) {
   return {static_cast<int32_t>(std::lround(x * kScale)), static_cast<int32_t>(std::lround(y * kScale))};
}

const std::byte* globalContext() { return *reinterpret_cast<const std::byte* const*>(layout.globalContext); }

const std::byte* currentGame() {
   const std::byte* context = globalContext();
   return context ? at<const std::byte*>(context, layout.globalGame) : nullptr;
}

// The local player when `playerIndex` (LuaPlayer::index, one more than Player::index) is it.
bool isLocalPlayer(int playerIndex) {
   const std::byte* game = currentGame();
   const std::byte* player = game ? at<const std::byte*>(game, layout.gameLocalPlayer) : nullptr;
   return player && at<uint16_t>(player, layout.playerIndex) + 1 == playerIndex;
}

// This client's player and the view showing it.
struct View {
   const std::byte* player = nullptr;
   const std::byte* view = nullptr;
   uint8_t controller = 0; // ControllerType
   Position centre{};
   double tilesPerPixel = 0;
   Pixel size{};
};

std::optional<View> localView() {
   const std::byte* game = currentGame();
   if (!game) return std::nullopt;
   View result;
   result.player = at<const std::byte*>(game, layout.gameLocalPlayer);
   result.view = at<const std::byte*>(game, layout.gameView);
   if (!result.player || !result.view || at<const std::byte*>(result.view, layout.gameViewPlayer) != result.player)
      return std::nullopt;
   const std::byte* controller = at<const std::byte*>(result.player, layout.playerController);
   if (!controller) return std::nullopt;
   result.controller = virtualAt<uint8_t (*)(const void*)>(controller, layout.controllerGetType)(controller);
   result.centre = at<Position>(result.view, layout.gameViewCentre);
   if (result.centre.x == std::numeric_limits<int32_t>::max()) return std::nullopt;
   const std::byte* settings = at<const std::byte*>(globalContext(), layout.globalInterfaceSettings);
   const double zoom = at<double>(result.view, layout.gameViewZoom);
   const double density = settings ? at<double>(settings, layout.displayDensity) : 1.0;
   if (!(zoom > 0) || !(density > 0)) return std::nullopt;
   result.tilesPerPixel = 1.0 / (zoom * density * 32.0);
   reinterpret_cast<Pixel* (*)(const void*, Pixel*)>(layout.gameViewDisplaySize)(result.view, &result.size);
   return result;
}

bool fixedCamera(uint8_t controller) {
   return controller == layout.controllerCharacter || controller == layout.controllerGod;
}

Screen screen(const View& view) { return cursormoves::screenOf(view.centre, view.size, view.tilesPerPixel); }

// The full map's cell size (cursormoves::cellSize), zero off the full map.
int32_t cellSize(const View& view) {
   if (at<uint8_t>(view.player, layout.playerRenderMode) != game::kRenderModeChart) return 0;
   return cursormoves::cellSize(view.size, view.tilesPerPixel);
}

// The movement keys, by the mod's custom input names: a step, a skip (Shift), a move by the size of
// the building in hand (Ctrl), and the cycle through what the mouse can select inside the tile
// (Shift+F). The skips come first, so a key with Shift or Ctrl is never taken for the plain step.
enum class KeyKind { Step, Skip, BySize, Cycle };
struct MoveKey {
   std::string_view name;
   int32_t dx;
   int32_t dy;
   KeyKind kind;
};
constexpr std::array<MoveKey, 13> kMoveKeys{{
   {"fa-s-f", 0, 0, KeyKind::Cycle},
   {"fa-s-w", 0, -1, KeyKind::Skip},
   {"fa-s-a", -1, 0, KeyKind::Skip},
   {"fa-s-s", 0, 1, KeyKind::Skip},
   {"fa-s-d", 1, 0, KeyKind::Skip},
   {"fa-c-w", 0, -1, KeyKind::BySize},
   {"fa-c-a", -1, 0, KeyKind::BySize},
   {"fa-c-s", 0, 1, KeyKind::BySize},
   {"fa-c-d", 1, 0, KeyKind::BySize},
   {"fa-w", 0, -1, KeyKind::Step},
   {"fa-a", -1, 0, KeyKind::Step},
   {"fa-s", 0, 1, KeyKind::Step},
   {"fa-d", 1, 0, KeyKind::Step},
}};
// How far a skip looks for a change.
constexpr int kSkipLimit = 100;

// Their ControlInputs in the game's control settings, found again when the settings are rebuilt.
struct MoveInputs {
   const std::byte* first = nullptr;
   std::array<const std::byte*, kMoveKeys.size()> inputs{};
};
MoveInputs g_moveInputs;

const MoveInputs& moveInputs() {
   const std::byte* settings = at<const std::byte*>(globalContext(), layout.globalControlSettings);
   const auto inputs =
      settings ? at<MsvcVector<const std::byte>>(settings, layout.customInputs) : MsvcVector<const std::byte>{};
   if (inputs.first == g_moveInputs.first) return g_moveInputs;
   g_moveInputs = {inputs.first, {}};
   for (const std::byte* input = inputs.first; input < inputs.last; input += layout.controlInputSize) {
      const std::byte* prototype = at<const std::byte*>(input, layout.controlInputPrototype);
      if (!prototype) continue;
      const std::string_view name = view(at<MsvcString>(prototype, layout.prototypeName));
      for (size_t i = 0; i < kMoveKeys.size(); ++i)
         if (kMoveKeys[i].name == name) g_moveInputs.inputs[i] = input;
   }
   return g_moveInputs;
}

using TriggeredFunction = const void* (*)(const void* control, const void* event, const void* context, uint32_t);

const MoveKey* triggeredMove(const void* event, const void* context) {
   const MoveInputs& found = moveInputs();
   for (size_t i = 0; i < kMoveKeys.size(); ++i) {
      if (found.inputs[i] &&
          reinterpret_cast<TriggeredFunction>(layout.controlTriggeredQuietly)(found.inputs[i], event, context, 0))
         return &kMoveKeys[i];
   }
   return nullptr;
}

// What the mod's Lua said last, and what the movement keys did; Lua reads and writes these on the
// update thread, the hooks on the main thread.
std::mutex g_mutex;
bool g_modeKnown = false;
bool g_vanilla = false;
bool g_keys = false;
bool g_anchored = false;
std::vector<skip::Ruler> g_rulers;
// The mod jumped the cursor: the next frame opens remote view if the screen does not hold it.
bool g_jumped = false;
// When to say the zoom and a view the player entered, and what the mod says is in hand, to say with
// that view.
viewannounce::Announcer g_announcer;
std::string g_hand;
// When the view was last drawn. A view entered is said only while frames come, so that without
// them (a game without graphics) the mod says the hand itself.
std::chrono::steady_clock::time_point g_lastViewFrame{};
constexpr auto kViewFramesGone = std::chrono::seconds(1);

struct StepRecord {
   Position position;
   Step step;
   int tiles;
   bool nextOfKind;
};
std::deque<StepRecord> g_steps;
constexpr size_t kMaxSteps = 64;

void record(Position position, Step step, int tiles = 1, bool nextOfKind = false) {
   std::scoped_lock lock(g_mutex);
   g_steps.push_back({position, step, tiles, nextOfKind});
   if (g_steps.size() > kMaxSteps) g_steps.pop_front();
}

// Whether a step from `from` to `to` leaves an entity bigger than a tile for another of its kind,
// by what the mouse selects on each tile. In the game's input handling.
bool nextOfKind(Position from, Position to) {
   return facts::nextOfKind(reader::selectedAt(tileOf(from.x), tileOf(from.y)),
                            reader::selectedAt(tileOf(to.x), tileOf(to.y)));
}

std::optional<Position> currentCursor() {
   auto position = world::cursorPosition();
   if (!position) return std::nullopt;
   return Position{position->x, position->y};
}

void place(Position position) { world::placeCursor({position.x, position.y}); }

// What was in hand and in view when the build was pinned: holding anything else, or another surface
// in view, re-attaches it. Under g_mutex.
struct Pin {
   bool blueprint;
   int entity;
   uint32_t surface;
};
std::optional<Pin> g_pin;

// Whether the building or blueprint in hand goes where the cursor is: one is held and not pinned.
bool buildFollowsCursor(int playerIndex) { return world::heldBuild(playerIndex) && !world::pinnedBuild(); }

// Ctrl with a movement key while the build is pinned: the build goes a tile that way, as long as
// its north-west corner stays on the screen. Once its preview has drawn there, how it fits and the
// piece now on the cursor's tile follow.
void nudgeForKey(const MoveKey& key, const View& view, Position cursor, Position pinned) {
   const int32_t x = tileOf(pinned.x) + key.dx, y = tileOf(pinned.y) + key.dy;
   const Position target = tileCentre(x, y);
   if (fixedCamera(view.controller) && !screen(view).holds(target)) {
      record(cursor, Step::Edge);
      return;
   }
   world::pinBuild({target.x, target.y});
   const int eighth = key.dx > 0 ? 2 : key.dx < 0 ? 6 : key.dy > 0 ? 4 : 0;
   speech::say(vocab::kNudged(vocab::direction(eighth)), true);
   footprint::announce({}, tileOf(cursor.x), tileOf(cursor.y));
   record(cursor, Step::Read);
}

// Before the reading of a tile the cursor reached: `lead` (how far a skip went), then the pinned
// build's piece there (footprint.h).
std::string withPiece(std::string lead, int32_t tileX, int32_t tileY) {
   const std::string piece = footprint::tilePrefix(tileX, tileY);
   if (!piece.empty()) lead += (lead.empty() ? "" : ", ") + piece;
   return lead;
}

// A skip key, or Ctrl with a movement key: the cursor goes on to the next tile that differs, or by
// the size of the building or blueprint in hand, and the reader reads it after how far it went.
void skipForKey(const MoveKey& key, const View& view, Position cursor) {
   const int playerIndex = at<uint16_t>(view.player, layout.playerIndex) + 1;
   const auto allowed = [&](int32_t x, int32_t y) {
      return !fixedCamera(view.controller) || screen(view).holds(tileCentre(x, y));
   };
   const int32_t fromX = tileOf(cursor.x), fromY = tileOf(cursor.y);
   if (key.kind == KeyKind::BySize) {
      if (const auto pinned = world::pinnedBuild()) {
         nudgeForKey(key, view, cursor, Position{pinned->x, pinned->y});
         return;
      }
      const auto held = world::heldBuild(playerIndex);
      const int32_t shift = cursormoves::bySizeShift(key.dx, held ? held->width : 0, held ? held->height : 0);
      const int32_t x = fromX + key.dx * shift, y = fromY + key.dy * shift;
      if (!allowed(x, y)) {
         record(cursor, Step::Edge);
         return;
      }
      const Position target = tileCentre(x, y);
      const bool next = nextOfKind(cursor, target);
      place(target);
      reader::read(x, y, vocab::kSkippedByPreview(shift));
      record(target, Step::Read, 1, next);
      return;
   }
   std::vector<skip::Ruler> rulers;
   {
      std::scoped_lock lock(g_mutex);
      rulers = g_rulers;
   }
   const skip::Result found = skip::find(fromX, fromY, key.dx, key.dy, kSkipLimit, allowed, rulers);
   const Position target = tileCentre(found.x, found.y);
   place(target);
   if (found.kind == skip::Result::Kind::Edge) {
      record(target, Step::Edge);
      return;
   }
   const bool limit = found.kind == skip::Result::Kind::Limit;
   std::string prefix;
   if (limit)
      prefix = vocab::kSkippedNoChange(kSkipLimit);
   else if (found.tiles > 1)
      prefix = vocab::kSkipped(found.tiles);
   reader::read(found.x, found.y, withPiece(std::move(prefix), found.x, found.y));
   record(target, Step::Skipped, limit ? -1 : found.tiles);
}

// The cycle key: the cursor goes to the next point inside its tile where the mouse selects
// something else (reader::stopsIn), and the game selects it there, as a sighted player picks among
// overlapping things by moving the mouse within the tile. The last stop wraps to the first. A
// building in hand keeps the cursor at the tile's centre, where it builds, unless it is pinned.
void cycleForKey(const View& view, Position cursor) {
   const int playerIndex = at<uint16_t>(view.player, layout.playerIndex) + 1;
   if (buildFollowsCursor(playerIndex)) {
      speech::say(std::string(vocab::kCycleHoldingBuild), true);
      return;
   }
   const std::vector<reader::Stop> stops = reader::stopsIn(tileOf(cursor.x), tileOf(cursor.y));
   if (stops.empty()) return;
   const void* current = reader::selectedAtPoint(cursor.x, cursor.y);
   const size_t next = cyclestops::nextStop(stops, current);
   const reader::Stop& stop = stops[next];
   place(Position{stop.x, stop.y});
   reader::readPoint(stop.x, stop.y, vocab::kPosition(next + 1, stops.size()));
}

void moveForKey(const void* event, const void* context) {
   if (!event) return;
   {
      std::scoped_lock lock(g_mutex);
      if (!g_keys) return;
   }
   const std::optional<Position> cursor = currentCursor();
   if (!cursor) return;
   const MoveKey* key = triggeredMove(event, context);
   if (!key) return;
   const std::optional<View> view = localView();
   if (!view) return;
   if (key->kind == KeyKind::Cycle) {
      cycleForKey(*view, *cursor);
      return;
   }
   if (key->kind != KeyKind::Step) {
      skipForKey(*key, *view, *cursor);
      return;
   }
   const int32_t size = cellSize(*view);
   const Position target = cursormoves::stepTarget(*cursor, key->dx, key->dy, size);
   if (size == 0 && fixedCamera(view->controller) && !screen(*view).holds(target)) {
      record(*cursor, Step::Edge);
      return;
   }
   place(target);
   // A drag build builds on the tile after this; the game's build preview tells what blocks it.
   const int playerIndex = at<uint16_t>(view->player, layout.playerIndex) + 1;
   if (size > 0 || world::dragBuild(playerIndex)) {
      record(target, Step::Moved);
      return;
   }
   reader::read(tileOf(target.x), tileOf(target.y), withPiece({}, tileOf(target.x), tileOf(target.y)));
   record(target, Step::Read, 1, nextOfKind(*cursor, target));
}

// The remote view's camera, kept on the cursor as a sighted player's map drag keeps it: by
// MoveOnPan, which the server applies some ticks later. Positions panned to and not yet reached.
std::deque<Position> g_pans;
constexpr size_t kMaxPans = 32;
// The map editor's camera, kept on the cursor by the mod's Lua, which moves the editor onto the
// cursor's tile after each key: the game ignores MoveOnPan there (Controller::canUseCursorToMove).
// The tiles the cursor stood on that the camera may yet reach.
std::deque<std::pair<int32_t, int32_t>> g_editorTiles;

alignas(16) std::byte g_actionBuffer[256];

// Sends an input action of `type` whose data `fill` writes, as the game sends a sighted player's.
template <class Fill>
void send(void* source, uint32_t type, Fill fill) {
   if (layout.inputActionSize > sizeof(g_actionBuffer)) return;
   std::byte* action = g_actionBuffer;
   std::memset(action, 0, layout.inputActionSize);
   put<uint16_t>(action, layout.inputActionType, static_cast<uint16_t>(type));
   put<uint16_t>(action, layout.inputActionPlayer, 0xffff);
   fill(action + layout.inputActionData);
   virtualAt<bool (*)(void*, void*)>(source, layout.inputSourceProcess)(source, action);
}

void pan(void* source, const View& view, Position target) {
   send(source, layout.actionMoveOnPan, [&](std::byte* data) {
      put<Position>(data, layout.moveOnPanPosition, target);
      put<double>(data, layout.moveOnPanZoom, 0.0); // keep the zoom
      put<uint32_t>(data, layout.moveOnPanSurface, at<uint32_t>(view.view, layout.gameViewSurfaceIndex));
   });
   g_pans.push_back(target);
   if (g_pans.size() > kMaxPans) g_pans.pop_front();
}

void zoomTo(void* source, double zoom) {
   send(source, layout.actionSetZoom, [&](std::byte* data) { put<double>(data, 0, zoom); });
}

// std::optional<StrongTypedef<double, ZoomTag>>, which MSVC passes by address. The game reads it
// with MOVAPS, which faults unless it is on a 16-byte boundary, as the compiler's own copies are.
struct alignas(16) OptionalZoom {
   double value;
   bool hasValue;
};

// The zoom of the character or god view a jump left, for the remote view it opens.
double g_jumpZoom = 0;

void openRemoteView(void* source, const View& view, Position target) {
   const void* surface = reinterpret_cast<const void* (*)(const void*)>(layout.gameViewSurface)(view.view);
   g_jumpZoom = at<double>(view.view, layout.gameViewZoom);
   const OptionalZoom zoom{g_jumpZoom, true};
   reinterpret_cast<void (*)(void*, const void*, const Position*, const OptionalZoom*)>(layout.openRemoteView)(
      source, surface, &target, &zoom);
}

// What the frames before saw.
std::optional<uint8_t> g_lastController;
// The tile the cursor last arrived on.
std::optional<std::pair<int32_t, int32_t>> g_landedTile;
Position g_lastCentre{};
// The centre of the character or god view before remote view opened: remote view opened there (the
// map key) comes to the cursor, opened elsewhere (an alert, a pin, the scanner) brings the cursor.
Position g_fixedCentre{};
// The turns of the drag under way the player has heard; nothing outside a drag.
std::optional<int> g_heardDragTurns;
// Frames to wait after the controller changed, for the view to show the new one.
int g_settling = 0;
// Frames left for a remote view the mod's jump asked for to open, while the cursor stays where it is.
int g_awaitingRemote = 0;
constexpr int kSettleFrames = 2;
constexpr int kRemoteWaitFrames = 300;

void unpin() {
   world::unpinBuild();
   footprint::clear();
   std::scoped_lock lock(g_mutex);
   g_pin.reset();
}

// The pinned build re-attaches once the hand holds anything else or another surface is in view.
void checkPin(const View& view, int playerIndex) {
   if (!world::pinnedBuild()) return;
   std::optional<Pin> pin;
   {
      std::scoped_lock lock(g_mutex);
      pin = g_pin;
   }
   const std::optional<world::HeldBuild> held = world::heldBuild(playerIndex);
   if (pin && held && held->blueprint == pin->blueprint && held->entity == pin->entity &&
       at<uint32_t>(view.view, layout.gameViewSurfaceIndex) == pin->surface)
      return;
   unpin();
   speech::say(std::string(vocab::kAttachedCursor), false);
}

// Says the direction a belt drag turns to when the game makes the turn, at the step after rotate.
void sayDragTurn(int playerIndex) {
   const std::optional<world::DragBuild> drag = world::dragBuild(playerIndex);
   const std::optional<int> heard =
      std::exchange(g_heardDragTurns, drag ? std::optional<int>(drag->turns) : std::nullopt);
   if (!drag || !heard || *heard == drag->turns) return;
   const int direction = world::buildDirection(playerIndex);
   if (direction >= 0) speech::say(vocab::kDragTurned(std::string(vocab::kDirection(direction))), true);
}

void reset() {
   g_heardDragTurns.reset();
   g_lastController.reset();
   g_landedTile.reset();
   g_pans.clear();
   g_editorTiles.clear();
   g_settling = 0;
   g_awaitingRemote = 0;
}

void remoteFrame(void* source, const View& view, Position cursor) {
   if (view.centre != g_lastCentre) {
      auto ours = std::find(g_pans.begin(), g_pans.end(), view.centre);
      if (ours == g_pans.end()) {
         // The game moved the camera: an alert, a pin, a search result, an entity it follows.
         g_pans.clear();
         place(tileCentre(view.centre));
         return;
      }
      g_pans.erase(g_pans.begin(), ours + 1);
   }
   const bool settled = g_pans.empty() ? sameTile(cursor, view.centre) : g_pans.back() == cursor;
   if (!settled) pan(source, view, cursor);
}

void editorFrame(const View& view, Position cursor) {
   const std::pair cursorTile{tileOf(cursor.x), tileOf(cursor.y)};
   if (g_editorTiles.empty() || g_editorTiles.back() != cursorTile) {
      g_editorTiles.push_back(cursorTile);
      if (g_editorTiles.size() > kMaxPans) g_editorTiles.pop_front();
   }
   if (view.centre == g_lastCentre) return;
   const std::pair centreTile{tileOf(view.centre.x), tileOf(view.centre.y)};
   auto ours = std::find(g_editorTiles.begin(), g_editorTiles.end(), centreTile);
   if (ours == g_editorTiles.end()) {
      // The arrows fly the editor, or the game moved it: a search result, a surface switch.
      g_editorTiles.clear();
      place(tileCentre(view.centre));
      return;
   }
   g_editorTiles.erase(g_editorTiles.begin(), ours);
}

void fixedFrame(void* source, const View& view, Position cursor, bool anchored, bool jumped) {
   const Screen tiles = screen(view);
   if (jumped && !tiles.holds(cursor)) {
      openRemoteView(source, view, cursor);
      g_awaitingRemote = kRemoteWaitFrames;
      return;
   }
   // In multiplayer the view shows the remote camera, on the cursor, some frames before the
   // controller changes: keep waiting for the controller, which restores the zoom.
   if (g_awaitingRemote > 0) {
      --g_awaitingRemote;
      return;
   }
   if (tiles.holds(cursor)) return;
   // The screen left the cursor behind: walking, zooming in.
   place(cursormoves::pulledOnto(tiles, cursor, view.centre, anchored));
}

// The view's word, for the views the mod names as the player enters them.
const vocab::Word* viewWord(uint8_t controller) {
   if (controller == layout.controllerRemote) return &vocab::kViewRemote;
   if (controller == layout.controllerCharacter) return &vocab::kViewCharacter;
   if (controller == layout.controllerGod) return &vocab::kViewGod;
   if (controller == layout.controllerEditor) return &vocab::kViewEditor;
   return nullptr;
}

// Says the zoom once it holds still after a change, or the view the player entered with the zoom it
// opens at and what is in hand. Followed in vanilla mode too, and said only outside it, as the mod
// says nothing there.
void announceView(const View& view, bool speak) {
   viewannounce::Say say;
   std::string hand;
   {
      std::scoped_lock lock(g_mutex);
      g_lastViewFrame = std::chrono::steady_clock::now();
      say = g_announcer.frame(view.tilesPerPixel);
      if (say == viewannounce::Say::View) hand = std::exchange(g_hand, {});
   }
   if (say == viewannounce::Say::Nothing || !speak) return;
   const int64_t tiles = std::llround(std::max(view.size.x, view.size.y) * view.tilesPerPixel);
   const int32_t cell = cellSize(view);
   const vocab::Word* word = say == viewannounce::Say::View ? viewWord(view.controller) : nullptr;
   if (!word) {
      speech::say(vocab::kZoomSet(tiles, cell), true);
      return;
   }
   std::string text = (*word)(tiles, cell);
   if (!hand.empty()) text += ", " + hand;
   speech::say(std::move(text), true);
}

void frame(void* source) {
   bool known, vanilla, anchored;
   {
      std::scoped_lock lock(g_mutex);
      known = g_modeKnown;
      vanilla = g_vanilla;
      anchored = g_anchored;
   }
   const std::optional<View> view = localView();
   if (view) announceView(*view, known && !vanilla);
   if (view) editoractions::frame(view->player);
   if (!view || !known || vanilla) {
      // The mouse is the cursor in vanilla mode, and the build follows it.
      if (vanilla && world::pinnedBuild()) unpin();
      reset();
      return;
   }
   if (!world::drivesCursor()) place(tileCentre(view->centre));
   const int playerIndex = at<uint16_t>(view->player, layout.playerIndex) + 1;
   checkPin(*view, playerIndex);
   sayDragTurn(playerIndex);
   // The scanner keeps LuaSurface::index, the engine's index + 1.
   scanner::frame(playerIndex, at<uint32_t>(view->view, layout.gameViewSurfaceIndex) + 1);
   // A building in hand builds from the tile's centre, wherever the cycle key left the cursor in it.
   if (const std::optional<Position> cursor = currentCursor();
       cursor && *cursor != tileCentre(*cursor) && buildFollowsCursor(playerIndex))
      place(tileCentre(*cursor));
   // Arriving on a tile at its centre, the cursor rests on what lies there away from the centre
   // (reader::landingIn), as a sighted player puts the mouse on it. A building in hand and a drag
   // build stay at the centre, where they build.
   if (const std::optional<Position> cursor = currentCursor(); cursor && world::drivesCursor()) {
      const std::pair tile{tileOf(cursor->x), tileOf(cursor->y)};
      if (tile != g_landedTile) {
         g_landedTile = tile;
         std::vector<skip::Ruler> rulers;
         {
            std::scoped_lock lock(g_mutex);
            rulers = g_rulers;
         }
         cues::rulerTone(skip::rulerAlignment(rulers, tile.first, tile.second));
         if (*cursor == tileCentre(*cursor) && !buildFollowsCursor(playerIndex) && !world::dragBuild(playerIndex)) {
            const reader::Point landing = reader::landingIn(tile.first, tile.second);
            if (landing.x != cursor->x || landing.y != cursor->y) place(Position{landing.x, landing.y});
         }
      }
   }

   if (view->controller != g_lastController) {
      if (view->controller == layout.controllerRemote && g_lastController && fixedCamera(*g_lastController))
         g_fixedCentre = g_lastCentre;
      g_lastController = view->controller;
      g_pans.clear();
      g_editorTiles.clear();
      g_settling = kSettleFrames;
   }
   if (g_settling > 0 && --g_settling == 0) {
      if (view->controller == layout.controllerRemote) {
         // Within a tile of where the character view was: the map key, which leaves the cursor.
         const bool atCharacter =
            std::abs(view->centre.x - g_fixedCentre.x) < kScale && std::abs(view->centre.y - g_fixedCentre.y) < kScale;
         if (!atCharacter && g_awaitingRemote == 0) place(tileCentre(view->centre));
         // Remote view opens on the full map whatever zoom it is given; a jump keeps the scale it
         // left, where the game selects and builds.
         if (g_awaitingRemote > 0) zoomTo(source, g_jumpZoom);
         g_awaitingRemote = 0;
      } else if (fixedCamera(view->controller) || view->controller == layout.controllerEditor) {
         // Leaving remote view brings the cursor back to the character, as the camera comes back;
         // the map editor opens with the cursor on its camera, which then follows it.
         place(tileCentre(view->centre));
      }
   }
   if (g_settling == 0) {
      bool jumped;
      {
         std::scoped_lock lock(g_mutex);
         jumped = std::exchange(g_jumped, false);
      }
      const Position cursor = *currentCursor();
      if (view->controller == layout.controllerRemote)
         remoteFrame(source, *view, cursor);
      else if (fixedCamera(view->controller))
         fixedFrame(source, *view, cursor, anchored, jumped);
      else if (view->controller == layout.controllerEditor)
         editorFrame(*view, cursor);
   }
   g_lastCentre = view->centre;
}

using CustomInputFunction = bool (*)(void* source, const void* event, const void* context);
CustomInputFunction g_customInputOriginal = nullptr;

bool detourCustomInput(void* source, const void* event, const void* context) {
   moveForKey(event, context);
   return g_customInputOriginal(source, event, context);
}

using RenderStateFunction = void (*)(void* source);
RenderStateFunction g_renderStateOriginal = nullptr;

void detourRenderState(void* source) {
   g_renderStateOriginal(source);
   const std::byte* context = globalContext();
   if (context && at<void*>(context, layout.globalPlayerInputSource) == source) frame(source);
}

} // namespace

void setMode(int playerIndex, bool vanilla, bool keys, bool anchored, std::vector<Tile> rulers) {
   if (!isLocalPlayer(playerIndex)) return;
   {
      std::scoped_lock lock(g_mutex);
      g_modeKnown = true;
      g_vanilla = vanilla;
      g_keys = keys && !vanilla;
      g_anchored = anchored;
      g_rulers.clear();
      for (const Tile& ruler : rulers) g_rulers.push_back({ruler.x, ruler.y});
   }
   if (vanilla) world::releaseCursor(playerIndex);
}

void viewEntered(int playerIndex, std::string hand) {
   if (!isLocalPlayer(playerIndex)) return;
   std::scoped_lock lock(g_mutex);
   if (std::chrono::steady_clock::now() - g_lastViewFrame > kViewFramesGone) return;
   g_announcer.entered();
   g_hand = std::move(hand);
}

bool viewHand(int playerIndex, std::string hand) {
   if (!isLocalPlayer(playerIndex)) return false;
   std::scoped_lock lock(g_mutex);
   if (!g_announcer.viewPending()) return false;
   g_hand = std::move(hand);
   return true;
}

void toggleDetach(int playerIndex) {
   if (!isLocalPlayer(playerIndex)) return;
   const std::optional<Position> cursor = currentCursor();
   const std::optional<View> view = localView();
   if (!cursor || !view) return;
   if (const auto pinned = world::pinnedBuild()) {
      unpin();
      place(Position{pinned->x, pinned->y});
      reader::read(tileOf(pinned->x), tileOf(pinned->y), std::string(vocab::kAttachedCursor));
      return;
   }
   const std::optional<world::HeldBuild> held = world::heldBuild(playerIndex);
   if (!held) {
      speech::say(std::string(vocab::kNothingToDetach), true);
      return;
   }
   // The build goes on from the tile's centre, where it is built now.
   const Position corner = tileCentre(*cursor);
   world::pinBuild({corner.x, corner.y});
   {
      std::scoped_lock lock(g_mutex);
      g_pin = Pin{held->blueprint, held->entity, at<uint32_t>(view->view, layout.gameViewSurfaceIndex)};
   }
   speech::say(std::string(vocab::kDetachedCursor), true);
   footprint::announce({}, tileOf(corner.x), tileOf(corner.y));
}

void jump(int playerIndex, double x, double y) {
   if (!isLocalPlayer(playerIndex)) return;
   const Position position = fromTiles(x, y);
   world::setCursor(playerIndex, (tileOf(position.x) + 0.5), (tileOf(position.y) + 0.5));
   std::scoped_lock lock(g_mutex);
   g_jumped = true;
}

std::optional<StepResult> step(int playerIndex, double x, double y) {
   if (!isLocalPlayer(playerIndex)) return std::nullopt;
   const Position position = fromTiles(x, y);
   std::scoped_lock lock(g_mutex);
   auto found = std::find_if(g_steps.begin(), g_steps.end(),
                             [&](const StepRecord& record) { return record.position == position; });
   if (found == g_steps.end()) return std::nullopt;
   const StepResult result{found->step, found->tiles, found->nextOfKind};
   // Steps before it were said already, or never will be.
   g_steps.erase(g_steps.begin(), found + 1);
   return result;
}

std::optional<Tile> position(int playerIndex) {
   if (!isLocalPlayer(playerIndex)) return std::nullopt;
   const std::optional<Position> cursor = currentCursor();
   if (!cursor) return std::nullopt;
   return Tile{tileOf(cursor->x), tileOf(cursor->y)};
}

std::string describe() {
   const std::optional<View> view = localView();
   if (!view) return "view=none\n";
   const char* controller = view->controller == layout.controllerCharacter ? "character"
                            : view->controller == layout.controllerGod     ? "god"
                            : view->controller == layout.controllerRemote  ? "remote"
                            : view->controller == layout.controllerEditor  ? "editor"
                                                                           : "other";
   const Screen tiles = screen(*view);
   std::string out = std::format(
      "controller={}\ncentre={},{}\nzoom={}\nscreen={},{},{},{}\ncell={}\n", controller,
      static_cast<double>(view->centre.x) / kScale, static_cast<double>(view->centre.y) / kScale,
      at<double>(view->view, layout.gameViewZoom), tiles.left, tiles.top, tiles.right, tiles.bottom, cellSize(*view));
   if (const std::optional<Position> cursor = currentCursor())
      out += std::format("cursor={},{}\n", tileOf(cursor->x), tileOf(cursor->y));
   else
      out += "cursor=none\n";
   if (const auto pinned = world::pinnedBuild())
      out += std::format("pinned={},{}\n", tileOf(pinned->x), tileOf(pinned->y));
   std::scoped_lock lock(g_mutex);
   out += std::format("anchored={}\n", g_anchored);
   return out;
}

void* customInputDetour() { return reinterpret_cast<void*>(&detourCustomInput); }
void** customInputOriginal() { return reinterpret_cast<void**>(&g_customInputOriginal); }
void* renderStateDetour() { return reinterpret_cast<void*>(&detourRenderState); }
void** renderStateOriginal() { return reinterpret_cast<void**>(&g_renderStateOriginal); }

} // namespace fa::cursor
