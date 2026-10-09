#include "cursor.h"

#include "game.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <limits>
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>

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
struct Position {
   int32_t x;
   int32_t y;
   bool operator==(const Position&) const = default;
};

struct Pixel {
   int32_t x;
   int32_t y;
};

constexpr int32_t kScale = game::kMapPositionScale;

int32_t tileOf(int32_t coordinate) { return coordinate >> 8; } // floors negative coordinates too
Position tileCentre(int32_t tileX, int32_t tileY) { return {tileX * kScale + kScale / 2, tileY * kScale + kScale / 2}; }
Position tileCentre(Position position) { return tileCentre(tileOf(position.x), tileOf(position.y)); }
bool sameTile(Position a, Position b) { return tileOf(a.x) == tileOf(b.x) && tileOf(a.y) == tileOf(b.y); }

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

// The tiles wholly on the screen, inclusive, as the view's centre and zoom place them.
struct Screen {
   int32_t left;
   int32_t top;
   int32_t right;
   int32_t bottom;

   bool holds(Position position) const {
      const int32_t x = tileOf(position.x), y = tileOf(position.y);
      return x >= left && x <= right && y >= top && y <= bottom;
   }
   Position clamp(Position position) const {
      return tileCentre(std::clamp(tileOf(position.x), left, right), std::clamp(tileOf(position.y), top, bottom));
   }
};

Screen screen(const View& view) {
   const double centreX = static_cast<double>(view.centre.x) / kScale;
   const double centreY = static_cast<double>(view.centre.y) / kScale;
   const double halfWidth = view.size.x * view.tilesPerPixel / 2;
   const double halfHeight = view.size.y * view.tilesPerPixel / 2;
   return {static_cast<int32_t>(std::ceil(centreX - halfWidth)), static_cast<int32_t>(std::ceil(centreY - halfHeight)),
           static_cast<int32_t>(std::floor(centreX + halfWidth)) - 1,
           static_cast<int32_t>(std::floor(centreY + halfHeight)) - 1};
}

// On the full map a movement key moves a cell: a square of tiles about a thirtieth of the screen
// across, rounded to a power of two, on a grid of its own size (the mod's map-cells.lua reads it so
// too). Zero off the full map.
int32_t cellSize(const View& view) {
   if (at<uint8_t>(view.player, layout.playerRenderMode) != game::kRenderModeChart) return 0;
   const double tiles = std::floor(std::max(view.size.x, view.size.y) * view.tilesPerPixel + 0.5);
   const double exponent = std::floor(std::log2(tiles / 30.0) + 0.5);
   return exponent < 0 ? 1 : int32_t{1} << static_cast<int>(exponent);
}

// The movement keys, by the mod's custom input names.
struct MoveKey {
   std::string_view name;
   int32_t dx;
   int32_t dy;
};
constexpr std::array<MoveKey, 4> kMoveKeys{{{"fa-w", 0, -1}, {"fa-a", -1, 0}, {"fa-s", 0, 1}, {"fa-d", 1, 0}}};

// Their ControlInputs in the game's control settings, found again when the settings are rebuilt.
struct MoveInputs {
   const std::byte* first = nullptr;
   std::array<const std::byte*, kMoveKeys.size()> inputs{};
};
MoveInputs g_moveInputs;

const MoveInputs& moveInputs() {
   const std::byte* settings = at<const std::byte*>(globalContext(), layout.globalControlSettings);
   const auto inputs = settings ? at<MsvcVector<const std::byte>>(settings, layout.customInputs)
                                : MsvcVector<const std::byte>{};
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
// The mod jumped the cursor: the next frame opens remote view if the screen does not hold it.
bool g_jumped = false;

struct StepRecord {
   Position position;
   Step step;
};
std::deque<StepRecord> g_steps;
constexpr size_t kMaxSteps = 64;

void record(Position position, Step step) {
   std::scoped_lock lock(g_mutex);
   g_steps.push_back({position, step});
   if (g_steps.size() > kMaxSteps) g_steps.pop_front();
}

std::optional<Position> currentCursor() {
   auto position = world::cursorPosition();
   if (!position) return std::nullopt;
   return Position{position->x, position->y};
}

void place(Position position) { world::placeCursor({position.x, position.y}); }

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
   const int32_t size = cellSize(*view);
   Position target;
   if (size > 0) {
      // The cell beyond, on the grid of cells.
      const auto cell = [&](int32_t tile, int32_t step) {
         const int32_t next = tile + step * size;
         const int32_t left = next >= 0 ? next / size * size : -((-next + size - 1) / size) * size;
         return left + size / 2;
      };
      target = tileCentre(cell(tileOf(cursor->x), key->dx), cell(tileOf(cursor->y), key->dy));
   } else {
      target = tileCentre(tileOf(cursor->x) + key->dx, tileOf(cursor->y) + key->dy);
      if (fixedCamera(view->controller) && !screen(*view).holds(target)) {
         record(*cursor, Step::Edge);
         return;
      }
   }
   place(target);
   record(target, Step::Moved);
}

// The remote view's camera, kept on the cursor as a sighted player's map drag keeps it: by
// MoveOnPan, which the server applies some ticks later. Positions panned to and not yet reached.
std::deque<Position> g_pans;
constexpr size_t kMaxPans = 32;

alignas(16) std::byte g_actionBuffer[256];

void pan(void* source, const View& view, Position target) {
   if (layout.inputActionSize > sizeof(g_actionBuffer)) return;
   std::byte* action = g_actionBuffer;
   std::memset(action, 0, layout.inputActionSize);
   put<uint16_t>(action, layout.inputActionType, static_cast<uint16_t>(layout.actionMoveOnPan));
   put<uint16_t>(action, layout.inputActionPlayer, 0xffff);
   std::byte* data = action + layout.inputActionData;
   put<Position>(data, layout.moveOnPanPosition, target);
   put<double>(data, layout.moveOnPanZoom, 0.0); // keep the zoom
   put<uint32_t>(data, layout.moveOnPanSurface, at<uint32_t>(view.view, layout.gameViewSurfaceIndex));
   virtualAt<bool (*)(void*, void*)>(source, layout.inputSourceProcess)(source, action);
   g_pans.push_back(target);
   if (g_pans.size() > kMaxPans) g_pans.pop_front();
}

// std::optional<StrongTypedef<double, ZoomTag>>, which MSVC passes by address.
struct OptionalZoom {
   double value;
   bool hasValue;
};

void openRemoteView(void* source, const View& view, Position target) {
   const void* surface = reinterpret_cast<const void* (*)(const void*)>(layout.gameViewSurface)(view.view);
   const OptionalZoom zoom{at<double>(view.view, layout.gameViewZoom), true};
   reinterpret_cast<void (*)(void*, const void*, const Position*, const OptionalZoom*)>(layout.openRemoteView)(
      source, surface, &target, &zoom);
}

// What the frames before saw.
std::optional<uint8_t> g_lastController;
Position g_lastCentre{};
// The centre of the character or god view before remote view opened: remote view opened there (the
// map key) comes to the cursor, opened elsewhere (an alert, a pin, the scanner) brings the cursor.
Position g_fixedCentre{};
// Frames to wait after the controller changed, for the view to show the new one.
int g_settling = 0;
// Frames left for a remote view the mod's jump asked for to open, while the cursor may stay off the
// screen.
int g_awaitingRemote = 0;
constexpr int kSettleFrames = 2;
constexpr int kRemoteWaitFrames = 300;

void reset() {
   g_lastController.reset();
   g_pans.clear();
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

void fixedFrame(void* source, const View& view, Position cursor, bool anchored, bool jumped) {
   const Screen tiles = screen(view);
   if (tiles.holds(cursor)) {
      g_awaitingRemote = 0;
      return;
   }
   if (jumped) {
      openRemoteView(source, view, cursor);
      g_awaitingRemote = kRemoteWaitFrames;
      return;
   }
   if (g_awaitingRemote > 0) {
      --g_awaitingRemote;
      return;
   }
   // The screen left the cursor behind: walking, zooming in.
   place(anchored ? tileCentre(view.centre) : tiles.clamp(cursor));
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
   if (!view || !known || vanilla) {
      reset();
      return;
   }
   if (!world::drivesCursor()) place(tileCentre(view->centre));

   if (view->controller != g_lastController) {
      if (view->controller == layout.controllerRemote && g_lastController && fixedCamera(*g_lastController))
         g_fixedCentre = g_lastCentre;
      g_lastController = view->controller;
      g_pans.clear();
      g_settling = kSettleFrames;
   }
   if (g_settling > 0 && --g_settling == 0) {
      if (view->controller == layout.controllerRemote) {
         // Within a tile of where the character view was: the map key, which leaves the cursor.
         const bool atCharacter = std::abs(view->centre.x - g_fixedCentre.x) < kScale &&
                                  std::abs(view->centre.y - g_fixedCentre.y) < kScale;
         if (!atCharacter && g_awaitingRemote == 0) place(tileCentre(view->centre));
         g_awaitingRemote = 0;
      } else if (fixedCamera(view->controller)) {
         // Leaving remote view brings the cursor back to the character, as the camera comes back.
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

void setMode(int playerIndex, bool vanilla, bool keys, bool anchored) {
   if (!isLocalPlayer(playerIndex)) return;
   {
      std::scoped_lock lock(g_mutex);
      g_modeKnown = true;
      g_vanilla = vanilla;
      g_keys = keys && !vanilla;
      g_anchored = anchored;
   }
   if (vanilla) world::releaseCursor(playerIndex);
}

void jump(int playerIndex, double x, double y) {
   if (!isLocalPlayer(playerIndex)) return;
   const Position position = fromTiles(x, y);
   world::setCursor(playerIndex, (tileOf(position.x) + 0.5), (tileOf(position.y) + 0.5));
   std::scoped_lock lock(g_mutex);
   g_jumped = true;
}

std::optional<Step> step(int playerIndex, double x, double y) {
   if (!isLocalPlayer(playerIndex)) return std::nullopt;
   const Position position = fromTiles(x, y);
   std::scoped_lock lock(g_mutex);
   auto found = std::find_if(g_steps.begin(), g_steps.end(),
                             [&](const StepRecord& record) { return record.position == position; });
   if (found == g_steps.end()) return std::nullopt;
   const Step result = found->step;
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

void* customInputDetour() { return reinterpret_cast<void*>(&detourCustomInput); }
void** customInputOriginal() { return reinterpret_cast<void**>(&g_customInputOriginal); }
void* renderStateDetour() { return reinterpret_cast<void*>(&detourRenderState); }
void** renderStateOriginal() { return reinterpret_cast<void**>(&g_renderStateOriginal); }

} // namespace fa::cursor
