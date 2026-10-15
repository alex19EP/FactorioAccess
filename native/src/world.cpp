#include "world.h"

#include "cursor.h"
#include "facts.h"
#include "footprint.h"
#include "game.h"
#include "highlights.h"
#include "railplanner.h"
#include "text.h"
#include "vocab.h"
#include "zoom.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fa::world {

namespace {

using game::layout;

template <class T>
T at(const void* object, uint32_t offset) {
   T value;
   std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
   return value;
}

template <class Function>
Function virtualAt(const void* object, uint32_t slot) {
   auto vtable = *static_cast<void* const* const*>(object);
   return reinterpret_cast<Function>(vtable[slot]);
}

// MapPosition: two ints, small enough that the game passes it by value in one register.
struct Position {
   int32_t x;
   int32_t y;
};

uint64_t pack(Position position) {
   return static_cast<uint32_t>(position.x) | static_cast<uint64_t>(static_cast<uint32_t>(position.y)) << 32;
}

Position unpack(uint64_t packed) {
   return {static_cast<int32_t>(static_cast<uint32_t>(packed)), static_cast<int32_t>(packed >> 32)};
}

// What the mod last reported, and the Game* it reported it in: a report lapses when another game
// loads. Written from Lua, read by the hooks and the main thread.
std::atomic<const void*> g_game{nullptr};
std::atomic<uint64_t> g_cursor{0};
std::atomic<bool> g_hasCursor{false};

const std::byte* currentGame() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   return context ? at<const std::byte*>(context, layout.globalGame) : nullptr;
}

const std::byte* localPlayer(const std::byte* game) { return at<const std::byte*>(game, layout.gameLocalPlayer); }

// The current game, when `playerIndex` (LuaPlayer::index, one more than Player::index) is its local
// player; otherwise null.
const std::byte* gameOfLocalPlayer(int playerIndex) {
   const std::byte* game = currentGame();
   if (!game) return nullptr;
   const std::byte* player = localPlayer(game);
   if (!player || at<uint16_t>(player, layout.playerIndex) + 1 != playerIndex) return nullptr;
   return game;
}

// The pinned build's position and the Game* it was pinned in; see pinBuild.
std::atomic<const void*> g_pinGame{nullptr};
std::atomic<uint64_t> g_pin{0};
std::atomic<bool> g_hasPin{false};

// The reported cursor, when it belongs to the current game.
bool cursor(const std::byte* game, Position& out) {
   if (!game || !g_hasCursor.load() || g_game.load() != game) return false;
   out = unpack(g_cursor.load());
   return true;
}

// The pinned build's position, when it belongs to the current game.
std::optional<Position> pinned(const std::byte* game) {
   if (!game || !g_hasPin.load() || g_pinGame.load() != game) return std::nullopt;
   return unpack(g_pin.load());
}

int32_t fixedPoint(double tiles) { return static_cast<int32_t>(std::lround(tiles * game::kMapPositionScale)); }

// Set while Player::getSimpleBuildInput runs again to build at the anchored position: the cursor
// reads as the centre that puts the building's north-west corner on the cursor tile.
thread_local bool t_anchoring = false;
thread_local Position t_anchor{};
// Set while Player::buildFromCursor runs. It runs for a script in the game state of every client,
// so it must build where the other clients do.
thread_local bool t_inScriptBuild = false;

// Both return their result through a hidden pointer, as MSVC does for member functions.
using CursorFunction = Position* (*)(const void* self, Position* out);
CursorFunction g_playerOriginal = nullptr;
CursorFunction g_sourceOriginal = nullptr;

Position* playerDetour(const void* player, Position* out) {
   const std::byte* game = currentGame();
   Position position;
   if (game && player == localPlayer(game)) {
      if (t_anchoring) {
         *out = t_anchor;
         return out;
      }
      if (cursor(game, position)) {
         *out = position;
         return out;
      }
   }
   return g_playerOriginal(player, out);
}

// Set while the game handles a scanner key: see setKeyCursor. Game thread only.
std::optional<Position> g_keyCursor;

// There is one PlayerInputSource, this client's.
Position* sourceDetour(const void* source, Position* out) {
   if (g_keyCursor && currentGame()) {
      *out = *g_keyCursor;
      return out;
   }
   Position position;
   if (cursor(currentGame(), position)) {
      *out = position;
      return out;
   }
   return g_sourceOriginal(source, out);
}

// PixelPosition: two ints, passed by value in one register.
struct Pixel {
   int32_t x;
   int32_t y;
};

using MapPositionFunction = Position* (*)(const void* view, Position* out, Pixel pixel);
MapPositionFunction g_mapPositionOriginal = nullptr;

// The world under the mouse is under the cursor: the game asks this client's view for the map
// position at the mouse pixel wherever it means the mouse in the world.
Position* detourMapPosition(const void* view, Position* out, Pixel pixel) {
   const std::byte* game = currentGame();
   Position position;
   if (game && view == at<const void*>(game, layout.gameView) && cursor(game, position)) {
      auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
      const std::byte* input = at<const std::byte*>(context, layout.globalInputState);
      if (input && pixel.x == at<int32_t>(input, layout.inputStateMouseX) &&
          pixel.y == at<int32_t>(input, layout.inputStateMouseY)) {
         *out = position;
         return out;
      }
   }
   return g_mapPositionOriginal(view, out, pixel);
}

using SelectionUpdateFunction = void (*)(void* selector, void* surface, const void* position, uint8_t allowCharacter,
                                         void* latency, void* selected);
SelectionUpdateFunction g_selectionUpdateOriginal = nullptr;

// The game selects nothing while the OS mouse is outside its window. While the mod drives the
// cursor the mouse is not what points, so the cursor selects wherever the mouse is: without this, a
// windowed game could neither open, mine nor start a rail plan with the mouse left elsewhere.
void detourSelectionUpdate(void* selector, void* surface, const void* position, uint8_t allowCharacter, void* latency,
                           void* selected) {
   auto* context = *reinterpret_cast<std::byte* const*>(layout.globalContext);
   auto* input = context ? at<std::byte*>(context, layout.globalInputState) : nullptr;
   if (!input || at<bool>(input, layout.inputStateMouseInWindow) || !drivesCursor()) {
      g_selectionUpdateOriginal(selector, surface, position, allowCharacter, latency, selected);
      return;
   }
   *reinterpret_cast<bool*>(input + layout.inputStateMouseInWindow) = true;
   g_selectionUpdateOriginal(selector, surface, position, allowCharacter, latency, selected);
   *reinterpret_cast<bool*>(input + layout.inputStateMouseInWindow) = false;
}

// NamedBool<Tag> is a one-byte struct, passed by value like a uint8_t.
using IsActiveFunction = bool (*)(const void* control, bool, uint8_t, bool, uint8_t);
IsActiveFunction g_isActiveOriginal = nullptr;

bool detourIsActive(const void* control, bool a, uint8_t gui, bool b, uint8_t modifiers) {
   if (zoom::isZoomControl(control)) return zoom::isActive(control, g_isActiveOriginal(control, a, gui, b, modifiers));
   return g_isActiveOriginal(control, a, gui, b, modifiers);
}

// This client's drag building context. There is one PlayerInputSource, this client's.
const std::byte* inputSource() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   return context ? at<const std::byte*>(context, layout.globalPlayerInputSource) : nullptr;
}

// Whether the build control holds a drag: from its first build to the release.
bool dragging(const void* drag) {
   return at<int32_t>(drag, layout.dragStartPosition) != std::numeric_limits<int32_t>::max();
}

bool localDragging() {
   const std::byte* source = inputSource();
   return source && dragging(source + layout.inputSourceDragContext);
}

// Turns the game has made in belt drags, at the cursor's step after rotate: the mod says each new
// one. Written by the drag update on the main thread, read from Lua.
std::atomic<int> g_dragTurns{0};

using DragUpdateFunction = void (*)(void* context, void* source);
DragUpdateFunction g_dragUpdateOriginal = nullptr;

void detourDragUpdate(void* context, void* source) {
   const bool turnPending = at<bool>(context, layout.dragTurnPending);
   g_dragUpdateOriginal(context, source);
   // Released, the drag clears the flag too.
   if (turnPending && !at<bool>(context, layout.dragTurnPending) && dragging(context)) ++g_dragTurns;
}

// The tiles an entity covers: TilePosition, returned through a hidden pointer.
struct TileSize {
   int32_t width;
   int32_t height;
};
using TileGridSizeFunction = TileSize* (*)(const void* prototype, TileSize* out, uint8_t direction);

struct MsvcString {
   union {
      char buffer[16];
      const char* pointer;
   };
   size_t size;
   size_t capacity;
};

template <class T>
struct MsvcVector {
   T* first;
   T* last;
   T* end;
};

const std::byte* entityPrototype(uint16_t index) {
   const auto& prototypes = *reinterpret_cast<const MsvcVector<const std::byte* const>*>(layout.entityPrototypes);
   return index < static_cast<size_t>(prototypes.last - prototypes.first) ? prototypes.first[index] : nullptr;
}

// The centre of a footprint `width` by `height` tiles whose north-west corner is the tile holding
// `corner`. The game rounds a centre to the grid by the footprint's parity, and this centre is
// already there.
Position centreFromCorner(Position corner, int32_t width, int32_t height) {
   // Arithmetic shifts floor negative coordinates too.
   const int32_t left = corner.x >> 8 << 8;
   const int32_t top = corner.y >> 8 << 8;
   return {left + width * game::kMapPositionScale / 2, top + height * game::kMapPositionScale / 2};
}

struct TileBox {
   int32_t left;
   int32_t top;
   int32_t right;
   int32_t bottom;
};

// Calls return classes through a hidden pointer; MapPosition, Direction and Flip go by value.
using GetCursorAdapterFunction = void** (*)(const void* adapter, void** out);
using DeleteReadAdapterFunction = void* (*)(void* reader, unsigned flags);
using BuildableBlueprintFunction = const std::byte* (*)(const void* reader);
using BuildingModifierFunction = void* (*)(const void* blueprint, std::byte* out, Position cursor, uint8_t direction,
                                           uint8_t flip, const Position* gridBase);
using TileBoxFunction = TileBox* (*)(const void* blueprint, TileBox* out);
using TransformFunction = Position* (*)(const void* modifier, Position* out, Position position);
using BuildIdFunction = void* (*)(const void* player, std::byte* out);

// Whether `player` holds the rail planner, which builds the route the game plans rather than a
// piece at the cursor (railplanner.h).
bool holdsRailPlanner(const void* player) {
   alignas(8) std::byte id[game::kBuildIdCapacity];
   reinterpret_cast<BuildIdFunction>(layout.playerBuildId)(player, id);
   return at<bool>(id, layout.buildIdRailPlanner);
}

// The game state's adapter for `player`: the latency one while latency hiding runs, else the
// player's own.
const std::byte* stateAdapter(const std::byte* player) {
   const std::byte* adapter = at<const std::byte*>(player, layout.playerLatencyAdapter);
   return adapter ? adapter : player + layout.playerGameStateAdapter;
}

// Calls `use(blueprint)` with the blueprint in `player`'s hand, or null, and returns its result.
// The blueprint belongs to the item or library record; only the reader that found it is freed.
template <class Use>
auto withHeldBlueprint(const std::byte* player, Use&& use) {
   const std::byte* adapter = stateAdapter(player);
   void* reader = nullptr;
   virtualAt<GetCursorAdapterFunction>(adapter, layout.adapterCursorAdapter)(adapter, &reader);
   if (!reader) return use(static_cast<const std::byte*>(nullptr));
   struct Owner {
      void* reader;
      ~Owner() { virtualAt<DeleteReadAdapterFunction>(reader, layout.readAdapterDestructor)(reader, 1); }
   } owner{reader};
   return use(reinterpret_cast<BuildableBlueprintFunction>(layout.buildableBlueprint)(reader));
}

// The tiles the blueprint's box covers, built facing its rotation. A flip keeps the size.
TileSize blueprintSize(const std::byte* blueprint) {
   TileBox box{};
   reinterpret_cast<TileBoxFunction>(layout.blueprintTileBox)(blueprint, &box);
   TileSize size{box.right - box.left, box.bottom - box.top};
   // Built facing east or west, the box turns on its side.
   if (at<uint8_t>(blueprint, layout.blueprintRotation) % 8 != 0) std::swap(size.width, size.height);
   return size;
}

// How the blueprint in hand is rotated and flipped, or nothing without one.
std::optional<uint32_t> blueprintTurn(const std::byte* player) {
   return withHeldBlueprint(player, [](const std::byte* blueprint) -> std::optional<uint32_t> {
      if (!blueprint) return std::nullopt;
      return at<uint8_t>(blueprint, layout.blueprintRotation) | at<uint8_t>(blueprint, layout.blueprintFlip) << 8u;
   });
}

// How the game places `blueprint`, rotated and flipped as it is held, with the cursor at `cursor`
// and no drag going on.
void freeModifier(const std::byte* blueprint, Position cursor, std::byte* modifier) {
   constexpr Position noGridBase{std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::max()};
   reinterpret_cast<BuildingModifierFunction>(layout.blueprintBuildingModifier)(
      blueprint, modifier, cursor, at<uint8_t>(blueprint, layout.blueprintRotation),
      at<uint8_t>(blueprint, layout.blueprintFlip), &noGridBase);
}

// Where the cursor has to be for a blueprint with relative snapping to have the north-west corner of
// its grid cell, the reference box the game draws, on the tile `cursor` is on. Placed as the game
// places it, the cell's corners are where they land on the map, whatever the rotation and flip; the
// cursor moves by whole tiles, which move the placement as much. A drag then tiles the copies from
// the last one built, one cell apart.
Position gridAnchor(const std::byte* blueprint, Position cursor) {
   alignas(8) std::byte modifier[game::kBuildingModifierCapacity];
   freeModifier(blueprint, cursor, modifier);
   const auto grid = at<TileSize>(blueprint, layout.blueprintGridSize);
   const auto transform = reinterpret_cast<TransformFunction>(layout.buildingModifierTransform);
   Position first{}, second{};
   transform(modifier, &first, {0, 0});
   transform(modifier, &second, {grid.width * game::kMapPositionScale, grid.height * game::kMapPositionScale});
   // Arithmetic shifts floor negative coordinates too.
   const int32_t left = cursor.x >> 8 << 8;
   const int32_t top = cursor.y >> 8 << 8;
   return {cursor.x + left - std::min(first.x, second.x), cursor.y + top - std::min(first.y, second.y)};
}

// Where the cursor has to be for the blueprint in hand, rotated and flipped as it is built, to have
// the north-west corner of its tile box on the tile `cursor` is on; with relative snapping, the
// corner of its grid cell (gridAnchor). False without a blueprint, for one with absolute snapping,
// which goes into the map's grid cell the cursor is in, and for one of only off-grid entities, which
// goes where the cursor is. A blueprint with rails snaps to the two-tile rail grid, which can move it
// a tile south or east.
bool blueprintAnchor(const std::byte* player, Position cursor, Position& out) {
   return withHeldBlueprint(player, [&](const std::byte* blueprint) {
      if (!blueprint) return false;
      if (at<bool>(blueprint, layout.blueprintSnapToGrid)) {
         if (at<bool>(blueprint, layout.blueprintAbsoluteSnapping)) return false;
         out = gridAnchor(blueprint, cursor);
         return true;
      }
      // A grid centre is on a half tile; an off-grid blueprint's is the cursor, here one off the
      // half tiles.
      constexpr Position probe{37, 37};
      alignas(8) std::byte modifier[game::kBuildingModifierCapacity];
      freeModifier(blueprint, probe, modifier);
      const auto centre = at<Position>(modifier, layout.buildingModifierCentre);
      if (centre.x == probe.x && centre.y == probe.y) return false;

      const TileSize size = blueprintSize(blueprint);
      if (size.width <= 0 || size.height <= 0) return false;
      out = centreFromCorner(cursor, size.width, size.height);
      return true;
   });
}

TileSize entitySize(const std::byte* prototype, uint8_t direction) {
   TileSize size{};
   virtualAt<TileGridSizeFunction>(prototype, layout.entityTileGridSize)(prototype, &size, direction);
   return size;
}

// Where the cursor has to be for what a SimpleBuildInput builds to have its north-west corner on
// the tile the cursor was on: for an entity, the centre of its footprint in the direction it is
// built; for a blueprint, see blueprintAnchor. False for what the game places by its own rules:
// tiles, the rail planner, entities placed off the grid (vehicles, rolling stock) or onto rail
// support spots, and diagonal directions.
bool anchoredPosition(const void* player, const std::byte* input, Position& out) {
   if (at<bool>(input, layout.simpleBuildInputRailPlanner)) return false;
   const auto index = at<uint16_t>(input, layout.simpleBuildInputEntity);
   if (index == 0) {
      if (at<const void*>(input, layout.simpleBuildInputTile)) return false;
      const auto cursor = at<Position>(input, layout.simpleBuildInputPosition);
      return cursor.x != std::numeric_limits<int32_t>::max() &&
             blueprintAnchor(static_cast<const std::byte*>(player), cursor, out);
   }
   const std::byte* prototype = entityPrototype(index);
   if (!prototype) return false;
   if (at<uint32_t>(prototype, layout.entityPrototypeFlags) &
       (game::kEntityPlaceableOffGrid | game::kEntitySnapToRailSupportSpot))
      return false;
   const auto direction = at<uint8_t>(input, layout.simpleBuildInputDirection);
   if (direction >= game::kDirectionCount || direction % 4 != 0) return false;
   const Position click = at<Position>(input, layout.simpleBuildInputClick);
   if (click.x == std::numeric_limits<int32_t>::max()) return false;

   const TileSize size = entitySize(prototype, direction);
   if (size.width <= 0 || size.height <= 0) return false;
   out = centreFromCorner(click, size.width, size.height);
   return true;
}

// Whether this client's cursor is the FA cursor, as the mod reported it.
bool modDrivesCursor(const void* player) {
   const std::byte* game = currentGame();
   if (!game || player != localPlayer(game)) return false;
   Position unused;
   return cursor(game, unused);
}

using SimpleBuildInputFunction = void* (*)(const void* player, std::byte* out, const void* drag);
SimpleBuildInputFunction g_simpleBuildInputOriginal = nullptr;

// Reads the input as the game would with the cursor at `position`, or where it is.
void* buildInputAt(const void* player, std::byte* out, const void* drag, std::optional<Position> position) {
   if (!position) return g_simpleBuildInputOriginal(player, out, drag);
   struct Scope {
      Scope(Position position) {
         t_anchor = position;
         t_anchoring = true;
      }
      ~Scope() { t_anchoring = false; }
   } scope(*position);
   return g_simpleBuildInputOriginal(player, out, drag);
}

// Reads the input as the game would, at the pinned build while there is one, then, when it builds
// an entity on the grid, reads it again with the cursor at the anchored position, so the direction
// and every snapping step still come from the game.
void* detourSimpleBuildInput(const void* player, std::byte* out, const void* drag) {
   if (t_inScriptBuild || t_anchoring || !modDrivesCursor(player)) return g_simpleBuildInputOriginal(player, out, drag);
   void* result = buildInputAt(player, out, drag, pinned(currentGame()));
   Position anchor;
   if (!anchoredPosition(player, out, anchor)) return result;
   return buildInputAt(player, out, drag, anchor);
}

// The entity in hand's preview, as drawn this frame: where, which entity and facing which way.
struct PreviewKey {
   Position position;
   uint16_t entity;
   uint8_t direction;
   bool operator==(const PreviewKey& other) const {
      return position.x == other.position.x && position.y == other.position.y && entity == other.entity &&
             direction == other.direction;
   }
};

// Set while BuildingRenderer::prepareBuildingInGame draws the hand of the player the mod drives:
// the first EntityToBeBuiltSettings::draw of the entity in hand reads its tint.
struct CursorPreview {
   bool active = false;
   bool read = false;
   PreviewKey key{};
};
thread_local CursorPreview t_preview;
// The preview last spoken, so that it is spoken once each time it moves, turns or changes entity.
// Only the thread that draws touches it.
std::optional<PreviewKey> g_spokenPreview;
// Whether "out of reach" was said since the preview was last in reach: it is said once on leaving
// reach, not on every move out there.
bool g_outOfReachSaid = false;

using PrepareBuildingFunction = int (*)(void* renderer, const void* player, const Position* cursor, void* drawQueue);
PrepareBuildingFunction g_prepareBuildingOriginal = nullptr;

// The item in hand is drawn where it would be built.
int detourPrepareBuilding(void* renderer, const void* player, const Position* cursor, void* drawQueue) {
   if (t_inScriptBuild || !modDrivesCursor(player)) {
      g_spokenPreview.reset();
      return g_prepareBuildingOriginal(renderer, player, cursor, drawQueue);
   }
   alignas(8) std::byte input[game::kSimpleBuildInputCapacity];
   const std::optional<Position> pin = pinned(currentGame());
   buildInputAt(player, input, nullptr, pin);
   Position anchor;
   const Position* position = anchoredPosition(player, input, anchor) ? &anchor : pin ? &*pin : cursor;
   const auto entity = at<uint16_t>(input, layout.simpleBuildInputEntity);
   // The pieces of the pinned build, or of a blueprint that follows the cursor (footprint.h).
   std::optional<footprint::Placement> placement;
   if (!holdsRailPlanner(player)) {
      if (pin)
         placement = footprint::Placement{pin->x, pin->y, 0, true};
      else if (entity == 0 && !at<const void*>(input, layout.simpleBuildInputTile))
         if (const std::optional<uint32_t> turn = blueprintTurn(static_cast<const std::byte*>(player)))
            placement = footprint::Placement{position->x, position->y, *turn, false};
   }
   // A frame that gathers them: every EntityToBeBuiltSettings::draw and
   // BuildingRenderer::drawTileToBeBuilt adds one.
   struct Gathering {
      bool active = false;
      Gathering(const std::optional<footprint::Placement>& placement, bool blueprint) {
         if (!placement || !footprint::wants(*placement)) return;
         active = true;
         footprint::begin(*placement, blueprint, localDragging());
      }
      ~Gathering() {
         if (active) footprint::end();
      }
   } gathering(placement, entity == 0);
   if (entity == 0) {
      g_spokenPreview.reset();
      return g_prepareBuildingOriginal(renderer, player, position, drawQueue);
   }
   struct Scope {
      Scope(PreviewKey key) { t_preview = {true, false, key}; }
      ~Scope() { t_preview = {}; }
   } scope({*position, entity, at<uint8_t>(input, layout.simpleBuildInputDirection)});
   // What the preview highlights is said with its tint's meaning, when that is said.
   std::optional<highlights::Collecting> collecting;
   if (g_spokenPreview != t_preview.key) collecting.emplace();
   return g_prepareBuildingOriginal(renderer, player, position, drawQueue);
}

using BuildCheckDataFunction = void* (*)(const void* settings, std::byte* out);
using BuildabilityCheckFunction = void* (*)(const void* adapter, std::byte* out, const void* entity,
                                            const std::byte* data);
using BuildCheckMessageFunction = void* (*)(const std::byte* result, std::byte* out);
using LocalisedStr = const MsvcString* (*)(const void* localisedString, const void* localeProvider);

// The text of a LocalisedString in the game's current locale, as the screen reader should read it;
// destroys the string.
std::string takeSpeakable(void* localised) {
   struct Destroy {
      void* object;
      ~Destroy() { reinterpret_cast<void (*)(void*)>(layout.localisedStringDestroy)(object); }
   } destroy{localised};
   const MsvcString* text = reinterpret_cast<LocalisedStr>(layout.localisedStringStr)(localised, nullptr);
   return text::speakable({text->capacity >= sizeof(text->buffer) ? text->pointer : text->buffer, text->size});
}

using EntitySelectorFunction = const std::byte* (*)(const void* adapter);

// The entity the player points at, which the mod has already read out at the cursor.
const void* selectedEntity(const std::byte* adapter) {
   const std::byte* selector = virtualAt<EntitySelectorFunction>(adapter, layout.adapterEntitySelector)(adapter);
   return selector ? at<const void*>(selector, layout.selectorEntity) : nullptr;
}

// What the preview's colour means, never the colour: nothing where it can be built, out of reach
// the first time it leaves reach, already built where the game tints that its own way, else the
// game's reason it cannot be built ("Can't build here" when it gives none). Nothing either when
// what is in the way is the entity at the cursor, which the mod has just named.
// While the build control drags, the mod names nothing at the cursor, and this says only what
// blocks the build: nothing for already built, nor for the entity the drag has just built where
// the preview stands.
// The game's buildability check of `entity` as the preview `settings` draws it, into `result`, as
// EntityToBeBuiltSettings::draw tints it; the result's type.
uint32_t checkBuild(const void* settings, const void* entity, std::byte* result) {
   const std::byte* player = at<const std::byte*>(settings, layout.settingsPlayer);
   const std::byte* adapter = stateAdapter(player);
   alignas(8) std::byte data[game::kBuildCheckDataCapacity];
   reinterpret_cast<BuildCheckDataFunction>(layout.settingsBuildCheckData)(settings, data);
   virtualAt<BuildabilityCheckFunction>(adapter, layout.adapterBuildabilityCheck)(adapter, result, entity, data);
   return at<uint32_t>(result, layout.buildCheckResultType);
}

// The game's reason a check failed, or "Can't build here" when it gives none.
std::string checkReason(const std::byte* result) {
   alignas(8) std::byte message[256];
   if (layout.localisedStringSize > sizeof(message)) return {};
   reinterpret_cast<BuildCheckMessageFunction>(layout.buildCheckMessage)(result, message);
   std::string reason = takeSpeakable(message);
   if (!reason.empty()) return reason;
   reinterpret_cast<void* (*)(void*, const char*)>(layout.localisedStringFromKey)(message,
                                                                                  "cant-build-reason.cant-build-here");
   return takeSpeakable(message);
}

// `inTheWay` gets the entity that blocks the build, if one does, said or not.
std::string previewMeaning(const void* settings, const void* entity, Position position, bool drag,
                           const void*& inTheWay) {
   const std::byte* player = at<const std::byte*>(settings, layout.settingsPlayer);
   const std::byte* adapter = stateAdapter(player);
   alignas(8) std::byte result[game::kBuildCheckResultCapacity];
   checkBuild(settings, entity, result);
   inTheWay = nullptr;

   const bool tooFar = at<bool>(settings, layout.settingsTooFar);
   if (!tooFar) g_outOfReachSaid = false;
   const auto type = at<uint32_t>(result, layout.buildCheckResultType);
   if (type == game::kBuildCheckBuildable) {
      if (!tooFar || g_outOfReachSaid) return {};
      g_outOfReachSaid = true;
      return std::string(vocab::kOutOfReach);
   }
   if (type == game::kBuildCheckIgnorable) return drag ? std::string() : std::string(vocab::kAlreadyBuilt);
   if (type == game::kBuildCheckCollidesWithEntity) {
      const std::byte* blocker = at<const std::byte*>(result, layout.buildCheckResultEntity);
      inTheWay = blocker;
      if (drag) {
         const auto where = at<Position>(blocker, layout.entityPosition);
         if (where.x == position.x && where.y == position.y &&
             at<const void*>(blocker, layout.entityPrototypeOf) == at<const void*>(entity, layout.entityPrototypeOf))
            return {};
      } else if (blocker == selectedEntity(adapter)) {
         return {};
      }
   }
   return checkReason(result);
}

// The blueprint in hand's entities as drawEntitiesCore draws them this frame, a
// BlueprintEntities*; null outside it.
thread_local const std::byte* t_blueprintEntities = nullptr;
// Those entities by the tiles their selection boxes cover, made the first time a piece looks for
// what it reaches in a frame; cleared as the frame's entities go. Only the thread that draws the
// preview touches them; not thread_local, which a core unloaded while the game runs must not leave
// destructors in.
std::unordered_map<uint64_t, std::vector<const void*>> g_blueprintTiles;
bool g_blueprintTilesMade = false;

template <class Visit>
void eachBlueprintEntity(Visit&& visit) {
   struct Vector {
      const std::byte* first;
      const std::byte* last;
      const std::byte* end;
   };
   const auto data = at<Vector>(t_blueprintEntities, layout.blueprintEntitiesData);
   for (const std::byte* entry = data.first; entry < data.last; entry += layout.entityDataSize)
      if (visit(entry, at<const void*>(entry, layout.entityDataEntity))) return;
}

// The blueprint's EntityData of `entity`, or null.
const std::byte* entityDataOf(const void* entity) {
   const std::byte* found = nullptr;
   eachBlueprintEntity([&](const std::byte* entry, const void* each) {
      if (each == entity) found = entry;
      return found != nullptr;
   });
   return found;
}

uint64_t tileKey(int32_t tileX, int32_t tileY) {
   return static_cast<uint32_t>(tileX) | static_cast<uint64_t>(static_cast<uint32_t>(tileY)) << 32;
}

// The blueprint in hand as the facts find what its entities reach (facts.h): among its own
// entities, as the blueprint window's picture finds them, where they would be built.
facts::Design designOfHand() {
   if (!g_blueprintTilesMade) {
      g_blueprintTilesMade = true;
      eachBlueprintEntity([](const std::byte*, const void* entity) {
         const facts::SelectionBox box = facts::selectionBox(entity);
         for (int32_t y = box.top >> 8; y <= (box.bottom - 1) >> 8; ++y)
            for (int32_t x = box.left >> 8; x <= (box.right - 1) >> 8; ++x)
               g_blueprintTiles[tileKey(x, y)].push_back(entity);
         return false;
      });
   }
   return {[](int32_t x, int32_t y) -> const void* {
              const auto found = g_blueprintTiles.find(tileKey(x >> 8, y >> 8));
              if (found == g_blueprintTiles.end()) return nullptr;
              for (const void* entity : found->second) {
                 const facts::SelectionBox box = facts::selectionBox(entity);
                 if (x >= box.left && x < box.right && y >= box.top && y < box.bottom) return entity;
              }
              return nullptr;
           },
           [](const void*) { return true; }};
}

// A blueprint's piece as the blueprint window's picture reads it (footprint::Piece): the facts of
// its design on each of its tiles, and what is drawn on it, with alt mode as `drawQueue` draws it.
void addDetails(footprint::Piece& piece, const void* drawQueue, const void* entity) {
   const void* renderParameters = at<const void*>(drawQueue, layout.drawQueueRenderParameters);
   piece.name = facts::blueprintEntityName(entity, renderParameters);
   const facts::Design design = designOfHand();
   for (int32_t y = piece.top; y <= piece.bottom; ++y)
      for (int32_t x = piece.left; x <= piece.right; ++x) {
         std::string words;
         for (const std::string& fact : facts::describeDesign(entity, x, y, design, false)) {
            if (fact.empty()) continue;
            if (!words.empty()) words += ", ";
            words += fact;
         }
         piece.tileFacts.push_back(std::move(words));
      }
   for (const std::string& part : facts::drawnOnBlueprintEntity(entity, renderParameters, entityDataOf(entity))) {
      if (part.empty()) continue;
      if (!piece.drawn.empty()) piece.drawn += ", ";
      piece.drawn += part;
   }
}

// One piece of the pinned build as its preview draws it: name, facing, tiles and whether it fits,
// and for a blueprint's piece while the build is pinned what else is drawn of it.
footprint::Piece pieceOf(const void* settings, const void* drawQueue, const void* entity) {
   const std::byte* prototype = at<const std::byte*>(entity, layout.entityPrototypeOf);
   const MsvcString* name =
      reinterpret_cast<LocalisedStr>(layout.localisedStringStr)(prototype + layout.prototypeLocalisedName, nullptr);
   footprint::Piece piece{};
   piece.name = text::speakable({name->capacity >= sizeof(name->buffer) ? name->pointer : name->buffer, name->size});
   piece.direction = -1;
   if (virtualAt<bool (*)(const void*)>(entity, layout.entityHasDirection)(entity)) {
      uint8_t direction = 0;
      virtualAt<void* (*)(const void*, uint8_t*)>(entity, layout.entityGetDirection)(entity, &direction);
      piece.direction = direction;
   }
   const facts::SelectionBox box = facts::selectionBox(entity);
   piece.left = box.left >> 8;
   piece.top = box.top >> 8;
   piece.right = std::max(piece.left, (box.right - 1) >> 8);
   piece.bottom = std::max(piece.top, (box.bottom - 1) >> 8);
   alignas(8) std::byte result[game::kBuildCheckResultCapacity];
   const uint32_t type = checkBuild(settings, entity, result);
   piece.fit = type == game::kBuildCheckBuildable   ? footprint::Fit::Fits
               : type == game::kBuildCheckIgnorable ? footprint::Fit::AlreadyBuilt
                                                    : footprint::Fit::Blocked;
   if (piece.fit == footprint::Fit::Blocked) piece.reason = checkReason(result);
   if (t_blueprintEntities && at<const void*>(settings, layout.settingsBlueprint) && footprint::gatheringDetails())
      addDetails(piece, drawQueue, entity);
   return piece;
}

using DrawBlueprintEntitiesFunction = void (*)(void* logic, const void* entities);
DrawBlueprintEntitiesFunction g_drawBlueprintEntitiesOriginal = nullptr;

void detourDrawBlueprintEntities(void* logic, const void* entities) {
   struct Scope {
      Scope(const void* entities) { t_blueprintEntities = static_cast<const std::byte*>(entities); }
      ~Scope() {
         t_blueprintEntities = nullptr;
         g_blueprintTiles.clear();
         g_blueprintTilesMade = false;
      }
   } scope(entities);
   g_drawBlueprintEntitiesOriginal(logic, entities);
}

using SettingsDrawFunction = void (*)(const void* settings, void* drawQueue, const void* entity);
SettingsDrawFunction g_settingsDrawOriginal = nullptr;

// The entity in hand's preview speaks its tint's meaning, then what it highlights, after the mod has
// spoken the cursor's move: the cursor reaches the game in the tick, the preview a frame later.
void detourSettingsDraw(const void* settings, void* drawQueue, const void* entity) {
   g_settingsDrawOriginal(settings, drawQueue, entity);
   if (footprint::gathering()) footprint::add(pieceOf(settings, drawQueue, entity));
   if (!t_preview.active || t_preview.read || at<const void*>(settings, layout.settingsBlueprint) ||
       railplanner::drawing())
      return;
   t_preview.read = true;
   if (g_spokenPreview == t_preview.key) return;
   g_spokenPreview = t_preview.key;
   const bool drag = localDragging();
   const void* inTheWay = nullptr;
   std::string meaning = previewMeaning(settings, entity, t_preview.key.position, drag, inTheWay);
   // What the drag's step found changed under the cursor comes first (cursor.h).
   if (drag) {
      std::string news = cursor::takeDragNews(meaning.empty() ? nullptr : inTheWay);
      if (!news.empty()) meaning = meaning.empty() ? std::move(news) : news + ", " + meaning;
   }
   highlights::preview(settings, entity, std::move(meaning), drag);
}

// TilePosition: two ints, passed by value in one register. NamedBool<Tag> is one byte and
// BuildMode an int, by value too.
struct TilePosition {
   int32_t x;
   int32_t y;
};
using DrawTileFunction = void (*)(const void* check, TilePosition tile, const void* placeAsTile, const void* prototype,
                                  uint8_t tooFar, uint8_t ghostMode, bool flag, bool skipCheck, void* drawQueue,
                                  int32_t buildMode, const void* adapter);
DrawTileFunction g_drawTileOriginal = nullptr;
using TileCheckFunction = void* (*)(const void* check, std::byte* out, const void* prototype, const void* placeAsTile,
                                    const TilePosition* tile, uint8_t ghostMode, bool flag, int32_t buildMode,
                                    uint8_t coverability);

// A blueprint's tile, drawn while the pieces of a pinned build are gathered, is one of them, with
// the check that tints it; one drawn unchecked is tinted as blocked.
void detourDrawTile(const void* check, TilePosition tile, const void* placeAsTile, const void* prototype,
                    uint8_t tooFar, uint8_t ghostMode, bool flag, bool skipCheck, void* drawQueue, int32_t buildMode,
                    const void* adapter) {
   if (footprint::gathering()) {
      const auto* tilePrototype = static_cast<const std::byte*>(prototype);
      const MsvcString* name = reinterpret_cast<LocalisedStr>(layout.localisedStringStr)(
         tilePrototype + layout.prototypeLocalisedName, nullptr);
      footprint::Piece piece{};
      piece.name = text::speakable({name->capacity >= sizeof(name->buffer) ? name->pointer : name->buffer, name->size});
      piece.direction = -1;
      piece.left = piece.right = tile.x;
      piece.top = piece.bottom = tile.y;
      piece.fit = footprint::Fit::Blocked;
      alignas(8) std::byte result[game::kBuildCheckResultCapacity];
      if (!skipCheck) {
         reinterpret_cast<TileCheckFunction>(layout.tileCheckBuildability)(check, result, prototype, placeAsTile, &tile,
                                                                           ghostMode, flag, buildMode, 1);
         const auto type = at<uint32_t>(result, layout.buildCheckResultType);
         piece.fit = type == game::kBuildCheckBuildable   ? footprint::Fit::Fits
                     : type == game::kBuildCheckIgnorable ? footprint::Fit::AlreadyBuilt
                                                          : footprint::Fit::Blocked;
         if (piece.fit == footprint::Fit::Blocked) piece.reason = checkReason(result);
      }
      footprint::add(std::move(piece));
   }
   g_drawTileOriginal(check, tile, placeAsTile, prototype, tooFar, ghostMode, flag, skipCheck, drawQueue, buildMode,
                      adapter);
}

// NamedBool<GhostModeTag> is one byte, passed by value.
using BuildFromCursorFunction = bool (*)(void* player, const void* position, uint8_t ghostMode, const void* drag);
BuildFromCursorFunction g_buildFromCursorOriginal = nullptr;

bool detourBuildFromCursor(void* player, const void* position, uint8_t ghostMode, const void* drag) {
   struct Scope {
      Scope() { t_inScriptBuild = true; }
      ~Scope() { t_inScriptBuild = false; }
   } scope;
   return g_buildFromCursorOriginal(player, position, ghostMode, drag);
}

} // namespace

void setCursor(int playerIndex, double x, double y) {
   const std::byte* game = gameOfLocalPlayer(playerIndex);
   if (!game) return;
   g_cursor.store(pack({fixedPoint(x), fixedPoint(y)}));
   g_game.store(game);
   g_hasCursor.store(true);
}

void releaseCursor(int playerIndex) {
   if (gameOfLocalPlayer(playerIndex)) g_hasCursor.store(false);
}

bool drivesCursor() {
   Position position;
   return cursor(currentGame(), position);
}

std::optional<CursorPosition> cursorPosition() {
   Position position;
   if (!cursor(currentGame(), position)) return std::nullopt;
   return CursorPosition{position.x, position.y};
}

void placeCursor(CursorPosition position) {
   const std::byte* game = currentGame();
   if (!game) return;
   g_cursor.store(pack({position.x, position.y}));
   g_game.store(game);
   g_hasCursor.store(true);
}

void setKeyCursor(std::optional<CursorPosition> position) {
   if (position)
      g_keyCursor = Position{position->x, position->y};
   else
      g_keyCursor.reset();
}

void pinBuild(CursorPosition position) {
   const std::byte* game = currentGame();
   if (!game) return;
   g_pin.store(pack({position.x, position.y}));
   g_pinGame.store(game);
   g_hasPin.store(true);
}

void unpinBuild() { g_hasPin.store(false); }

std::optional<CursorPosition> pinnedBuild() {
   const std::optional<Position> position = pinned(currentGame());
   if (!position) return std::nullopt;
   return CursorPosition{position->x, position->y};
}

bool mayBeLocalPlayer(int playerIndex) {
   const std::byte* game = currentGame();
   const std::byte* player = game ? localPlayer(game) : nullptr;
   return !player || at<uint16_t>(player, layout.playerIndex) + 1 == playerIndex;
}

int buildDirection(int playerIndex) {
   const std::byte* game = gameOfLocalPlayer(playerIndex);
   const std::byte* view = game ? at<const std::byte*>(game, layout.gameView) : nullptr;
   if (!view) return -1;
   const uint8_t direction = at<uint8_t>(view, layout.gameViewBuildDirection);
   return direction < game::kDirectionCount ? direction : -1;
}

bool holdsRailPlanner(int playerIndex) {
   const std::byte* game = gameOfLocalPlayer(playerIndex);
   return game && at<const std::byte*>(game, layout.gameView) && holdsRailPlanner(localPlayer(game));
}

bool railPlanning(int playerIndex) {
   const std::byte* game = gameOfLocalPlayer(playerIndex);
   const std::byte* view = game ? at<const std::byte*>(game, layout.gameView) : nullptr;
   return view && at<bool>(view, layout.gameViewRailPlannerActive) && holdsRailPlanner(localPlayer(game));
}

std::optional<HeldBuild> heldBuild(int playerIndex) {
   const std::byte* game = gameOfLocalPlayer(playerIndex);
   const std::byte* view = game ? at<const std::byte*>(game, layout.gameView) : nullptr;
   if (!view) return std::nullopt;
   const std::byte* player = localPlayer(game);
   alignas(8) std::byte id[game::kBuildIdCapacity];
   reinterpret_cast<BuildIdFunction>(layout.playerBuildId)(player, id);
   if (at<bool>(id, layout.buildIdRailPlanner) || at<const void*>(id, layout.buildIdTile)) return std::nullopt;

   if (const auto index = at<uint16_t>(id, layout.buildIdEntity)) {
      const std::byte* prototype = entityPrototype(index);
      if (!prototype) return std::nullopt;
      HeldBuild held;
      held.entity = index;
      // As Player::getSimpleBuildInput takes it.
      held.direction = at<uint32_t>(prototype, layout.entityPrototypeFlags) & game::kEntityNotRotatable
                          ? 0
                          : at<uint8_t>(view, layout.gameViewBuildDirection) % game::kDirectionCount;
      const TileSize size = entitySize(prototype, static_cast<uint8_t>(held.direction));
      held.width = size.width;
      held.height = size.height;
      const auto flipping = at<uint8_t>(prototype, layout.entityFlipping);
      held.flippable = flipping != game::kEntityFlippingNotAvailable;
      held.mirrored = flipping == game::kEntityFlippingMirroring && at<bool>(view, layout.gameViewEntityMirrored);
      return held;
   }

   return withHeldBlueprint(player, [](const std::byte* blueprint) -> std::optional<HeldBuild> {
      if (!blueprint) return std::nullopt;
      HeldBuild held;
      held.blueprint = true;
      held.direction = at<uint8_t>(blueprint, layout.blueprintRotation) % game::kDirectionCount;
      const TileSize size = blueprintSize(blueprint);
      held.width = size.width;
      held.height = size.height;
      // The flip is kept in the blueprint's own frame; built facing east or west, its horizontal
      // flip runs north to south on the map.
      const auto flip = at<uint8_t>(blueprint, layout.blueprintFlip);
      const bool sideways = held.direction % 8 != 0;
      held.flipHorizontal = (flip & (sideways ? game::kFlipVertical : game::kFlipHorizontal)) != 0;
      held.flipVertical = (flip & (sideways ? game::kFlipHorizontal : game::kFlipVertical)) != 0;
      return held;
   });
}

std::optional<DragBuild> dragBuild(int playerIndex) {
   const std::byte* game = gameOfLocalPlayer(playerIndex);
   const std::byte* source = game ? inputSource() : nullptr;
   if (!source || at<const std::byte*>(source, layout.inputSourcePlayer) != localPlayer(game)) return std::nullopt;
   const std::byte* drag = source + layout.inputSourceDragContext;
   if (!dragging(drag)) return std::nullopt;
   return DragBuild{at<bool>(drag, layout.dragTurnPending), g_dragTurns.load()};
}

void* playerCursorDetour() { return reinterpret_cast<void*>(&playerDetour); }
void** playerCursorOriginal() { return reinterpret_cast<void**>(&g_playerOriginal); }
void* sourceCursorDetour() { return reinterpret_cast<void*>(&sourceDetour); }
void** sourceCursorOriginal() { return reinterpret_cast<void**>(&g_sourceOriginal); }
void* mapPositionDetour() { return reinterpret_cast<void*>(&detourMapPosition); }
void** mapPositionOriginal() { return reinterpret_cast<void**>(&g_mapPositionOriginal); }
void* selectionUpdateDetour() { return reinterpret_cast<void*>(&detourSelectionUpdate); }
void** selectionUpdateOriginal() { return reinterpret_cast<void**>(&g_selectionUpdateOriginal); }
void* dragUpdateDetour() { return reinterpret_cast<void*>(&detourDragUpdate); }
void** dragUpdateOriginal() { return reinterpret_cast<void**>(&g_dragUpdateOriginal); }
void* isActiveDetour() { return reinterpret_cast<void*>(&detourIsActive); }
void** isActiveOriginal() { return reinterpret_cast<void**>(&g_isActiveOriginal); }
void* simpleBuildInputDetour() { return reinterpret_cast<void*>(&detourSimpleBuildInput); }
void** simpleBuildInputOriginal() { return reinterpret_cast<void**>(&g_simpleBuildInputOriginal); }
void* prepareBuildingDetour() { return reinterpret_cast<void*>(&detourPrepareBuilding); }
void** prepareBuildingOriginal() { return reinterpret_cast<void**>(&g_prepareBuildingOriginal); }
void* buildFromCursorDetour() { return reinterpret_cast<void*>(&detourBuildFromCursor); }
void** buildFromCursorOriginal() { return reinterpret_cast<void**>(&g_buildFromCursorOriginal); }
void* settingsDrawDetour() { return reinterpret_cast<void*>(&detourSettingsDraw); }
void** settingsDrawOriginal() { return reinterpret_cast<void**>(&g_settingsDrawOriginal); }
void* drawBlueprintEntitiesDetour() { return reinterpret_cast<void*>(&detourDrawBlueprintEntities); }
void** drawBlueprintEntitiesOriginal() { return reinterpret_cast<void**>(&g_drawBlueprintEntitiesOriginal); }
void* drawTileDetour() { return reinterpret_cast<void*>(&detourDrawTile); }
void** drawTileOriginal() { return reinterpret_cast<void**>(&g_drawTileOriginal); }

} // namespace fa::world
