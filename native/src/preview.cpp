#include "preview.h"

#include "agui.h"
#include "facts.h"
#include "game.h"
#include "text.h"
#include "vocab.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include <vector>

namespace fa::preview {

namespace {

using game::layout;

template <class T>
const T& at(const void* object, uint32_t offset) {
   return *reinterpret_cast<const T*>(static_cast<const std::byte*>(object) + offset);
}

// MSVC std::string: a 16-byte small buffer or a heap pointer, then size and capacity.
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

using VirtualTable = void* const*;

template <class Function>
Function virtualAt(const void* object, uint32_t slot) {
   return reinterpret_cast<Function>((*reinterpret_cast<const VirtualTable*>(object))[slot]);
}

struct MapPosition {
   int32_t x;
   int32_t y;
};

struct TileBox {
   int32_t left;
   int32_t top;
   int32_t right;
   int32_t bottom;
};

// Calls return classes through a hidden pointer after `this`.
using SelectionFunction = std::byte* (*)(const void* blueprint, std::byte* out, const MapPosition* at,
                                         const void* parameters);
using TileSelectionFunction = std::byte* (*)(const void* tiles, std::byte* out, const MapPosition* at);
using TileBoxFunction = TileBox* (*)(const void* blueprint, TileBox* out);
using PixelShiftFunction = MapPosition* (*)(const void* picture, MapPosition* out, const MapPosition* absolute);
using LocalisedStr = const MsvcString* (*)(const void* localisedString, const void* localeProvider);

constexpr int32_t kScale = game::kMapPositionScale;

// The BlueprintWidget a picture is.
const std::byte* self(const agui::Widget* picture) { return agui::objectAsBase(picture, ".?AVBlueprintWidget@@"); }

const std::byte* blueprintOf(const std::byte* widget) { return at<const std::byte*>(widget, layout.pictureBlueprint); }

const std::byte* parametersOf(const std::byte* widget) { return widget + layout.pictureParameters; }

// The prototype an ID indexes in a PrototypeList<T>::indexToPrototype vector, or null.
const std::byte* prototypeAt(uintptr_t list, size_t index) {
   const auto& prototypes = *reinterpret_cast<const MsvcVector<const std::byte* const>*>(list);
   return index < static_cast<size_t>(prototypes.last - prototypes.first) ? prototypes.first[index] : nullptr;
}

std::string localisedName(const std::byte* prototype) {
   const MsvcString* name =
      reinterpret_cast<LocalisedStr>(layout.localisedStringStr)(prototype + layout.prototypeLocalisedName, nullptr);
   return text::speakable(view(*name));
}

// A quality's name and a space before the name it qualifies, or nothing for normal quality or none.
std::string qualityPrefix(uint8_t quality) {
   const std::byte* prototype = quality ? prototypeAt(layout.qualityPrototypes, quality) : nullptr;
   if (!prototype || view(at<MsvcString>(prototype, layout.prototypeName)) == "normal") return {};
   return localisedName(prototype) + " ";
}

std::string named(uintptr_t list, uint16_t id, uint8_t quality) {
   const std::byte* prototype = id ? prototypeAt(list, id) : nullptr;
   return prototype ? qualityPrefix(quality) + localisedName(prototype) : std::string();
}

std::string join(const std::vector<std::string>& parts, std::string_view separator = ", ") {
   std::string joined;
   for (const std::string& part : parts) {
      if (part.empty()) continue;
      if (!joined.empty()) joined += separator;
      joined += part;
   }
   return joined;
}

// The blueprint's EntityData at `index`, or null.
const std::byte* entityDataAt(const std::byte* blueprint, uint32_t index) {
   const auto& entities = at<MsvcVector<const std::byte>>(blueprint, layout.blueprintEntityList);
   const std::byte* data = entities.first + size_t{index} * layout.entityDataSize;
   return data < entities.last ? data : nullptr;
}

// Whether the entity or tile at `index` of the picture's removal list is removed.
bool removed(const std::byte* widget, uint32_t list, uint32_t index) {
   const auto& items = at<MsvcVector<const uint8_t>>(parametersOf(widget), list);
   return index < static_cast<size_t>(items.last - items.first) && items.first[index];
}

MapPosition centreOf(int x, int y) { return {x * kScale + kScale / 2, y * kScale + kScale / 2}; }

// What the mouse at `point` would pick in the picture, written to `out` (a
// BlueprintSelectionResult). False where it picks nothing.
bool selectAt(const std::byte* widget, MapPosition point, std::byte* out) {
   if (layout.selectionResultSize > 64) return false;
   reinterpret_cast<SelectionFunction>(layout.blueprintSelectionAt)(blueprintOf(widget), out, &point,
                                                                    parametersOf(widget));
   return at<const std::byte*>(out, layout.selectionEntity) || at<uint16_t>(out, layout.selectionTile);
}

// What the mouse would pick at the tile's centre.
bool select(const std::byte* widget, int x, int y, std::byte* out) { return selectAt(widget, centreOf(x, y), out); }

// The entity the mouse would pick at a map position in the picture, or null.
const std::byte* entityAtPoint(const std::byte* widget, MapPosition point) {
   alignas(16) std::byte result[64] = {};
   if (!selectAt(widget, point, result)) return nullptr;
   return at<const std::byte*>(result, layout.selectionEntity);
}

// Whether the picture keeps the blueprint's entity `entity`: false for one the player removed.
bool keeps(const std::byte* widget, const void* entity) {
   const auto& entities = at<MsvcVector<const std::byte>>(blueprintOf(widget), layout.blueprintEntityList);
   uint32_t index = 0;
   for (const std::byte* data = entities.first; data < entities.last; data += layout.entityDataSize, ++index)
      if (at<const void*>(data, layout.entityDataEntity) == entity)
         return !removed(widget, layout.parametersEntities, index);
   return true;
}

// The blueprint as the facts find what lies around an entity of the picture (its underground
// belts' partners, what its inserters and drills reach, what its belts and pipes join), leaving
// out what the player removed, which is not built.
facts::Design designOf(const std::byte* widget) {
   return {[widget](int32_t x, int32_t y) -> const void* {
              alignas(16) std::byte result[64] = {};
              if (!selectAt(widget, MapPosition{x, y}, result)) return nullptr;
              const auto* entity = at<const std::byte*>(result, layout.selectionEntity);
              if (!entity || removed(widget, layout.parametersEntities, at<uint32_t>(result, layout.selectionIndex)))
                 return nullptr;
              return entity;
           },
           [widget](const void* entity) { return keeps(widget, entity); }};
}

// The floor under an entity, which the picture draws beneath it while the blueprint's tiles are
// included: "on Refined concrete".
std::string floorUnder(const std::byte* widget, int x, int y) {
   if (!at<bool>(parametersOf(widget), layout.parametersIncludeTiles)) return {};
   alignas(16) std::byte result[64] = {};
   if (layout.selectionResultSize > sizeof(result)) return {};
   const MapPosition centre = centreOf(x, y);
   reinterpret_cast<TileSelectionFunction>(layout.blueprintTileAt)(blueprintOf(widget) + layout.blueprintTiles, result,
                                                                   &centre);
   const std::string name = named(layout.tilePrototypes, at<uint16_t>(result, layout.selectionTile), 0);
   return name.empty() ? name : vocab::kOnFloor(name);
}

std::string describeEntity(const std::byte* widget, const std::byte* entity, uint32_t index, int x, int y) {
   const std::byte* renderParameters = widget + layout.pictureRenderParameters;
   std::vector<std::string> parts{facts::blueprintEntityName(entity, renderParameters)};
   if (removed(widget, layout.parametersEntities, index)) parts.emplace_back(vocab::kRemoved);
   for (std::string& fact : facts::describeDesign(entity, x, y, designOf(widget))) parts.push_back(std::move(fact));
   for (std::string& drawn :
        facts::drawnOnBlueprintEntity(entity, renderParameters, entityDataAt(blueprintOf(widget), index)))
      parts.push_back(std::move(drawn));
   parts.push_back(floorUnder(widget, x, y));
   return join(parts);
}

// The tile's centre in the picture's own coordinates, where a click on it lands: the inverse of
// what BlueprintWidget::handleMouseEvent makes of a click.
MapPosition pixelOf(const std::byte* widget, int x, int y) {
   MapPosition origin{0, 0};
   MapPosition shift{};
   reinterpret_cast<PixelShiftFunction>(layout.picturePixelShift)(widget, &shift, &origin);
   double scale = at<double>(widget, layout.pictureScale) * 32.0 / 256.0;
   MapPosition leftTop = at<MapPosition>(widget, layout.pictureViewLeftTop);
   return {shift.x + static_cast<int32_t>((x * 256 + 128 - leftTop.x) * scale),
           shift.y + static_cast<int32_t>((y * 256 + 128 - leftTop.y) * scale)};
}

// The picture as the skip keys see a world (skiprules.h): what the mouse picks on each tile, and
// where it picks no entity the floor the picture draws there, in the place of the fog of war's
// picture, so a skip stops where the floor changes too.
struct PictureWorld {
   using Entity = const void*;
   const std::byte* widget;
   facts::Design design;

   Entity selectedAt(int32_t x, int32_t y) const { return entityAtPoint(widget, centreOf(x, y)); }
   bool waterAt(int32_t, int32_t) const { return false; }
   std::optional<uint16_t> shownAt(int32_t x, int32_t y) const {
      alignas(16) std::byte result[64] = {};
      if (!select(widget, x, y, result) || at<const std::byte*>(result, layout.selectionEntity)) return std::nullopt;
      return at<uint16_t>(result, layout.selectionTile);
   }
   bool skipsOver(Entity entity) const { return facts::skipsOver(entity); }
   facts::Likeness likenessOf(Entity entity) const { return facts::likenessOf(entity); }
   std::optional<facts::UndergroundEnd> undergroundEnd(Entity entity, uint8_t direction) const {
      return facts::undergroundEnd(entity, direction, &design);
   }
};

static_assert(skip::SkipWorld<PictureWorld>);

} // namespace

std::optional<Box> extent(const agui::Widget* picture) {
   const std::byte* widget = self(picture);
   if (!widget || !blueprintOf(widget)) return std::nullopt;
   // The blueprint's own box: the one the settings give leaves removed entities out, which the
   // picture draws still, as ghosts, and with snapping on is measured from the grid.
   alignas(16) std::byte out[64] = {};
   const TileBox* box =
      reinterpret_cast<TileBoxFunction>(layout.blueprintTileBox)(blueprintOf(widget), reinterpret_cast<TileBox*>(out));
   if (box->right <= box->left || box->bottom <= box->top) return std::nullopt;
   return Box{box->left, box->top, box->right, box->bottom};
}

std::string describe(const agui::Widget* picture, int x, int y) {
   const std::byte* widget = self(picture);
   alignas(16) std::byte result[64] = {};
   if (!widget || !blueprintOf(widget) || !select(widget, x, y, result)) return std::string(vocab::kEmpty);
   uint32_t index = at<uint32_t>(result, layout.selectionIndex);
   if (const std::byte* entity = at<const std::byte*>(result, layout.selectionEntity))
      return describeEntity(widget, entity, index, x, y);
   std::vector<std::string> parts{named(layout.tilePrototypes, at<uint16_t>(result, layout.selectionTile), 0)};
   if (removed(widget, layout.parametersTiles, index)) parts.emplace_back(vocab::kRemoved);
   return join(parts);
}

skip::Result skipFrom(const agui::Widget* picture, int x, int y, int dx, int dy) {
   const std::byte* widget = self(picture);
   const std::optional<Box> box = extent(picture);
   if (!widget || !box) return {skip::Result::Kind::Edge, x, y, 0};
   const auto inside = [&](int32_t atX, int32_t atY) {
      return atX >= box->left && atX < box->right && atY >= box->top && atY < box->bottom;
   };
   // The box's edge comes before any limit.
   const int limit = (box->right - box->left) + (box->bottom - box->top);
   const PictureWorld world{widget, designOf(widget)};
   return skip::find(world, x, y, dx, dy, limit, inside, {});
}

bool nextOfKind(const agui::Widget* picture, int fromX, int fromY, int toX, int toY) {
   const std::byte* widget = self(picture);
   if (!widget || !blueprintOf(widget)) return false;
   return facts::nextOfKind(entityAtPoint(widget, centreOf(fromX, fromY)), entityAtPoint(widget, centreOf(toX, toY)));
}

bool editable(const agui::Widget* picture) {
   const std::byte* widget = self(picture);
   return widget && at<bool>(widget, layout.pictureEditEnabled);
}

bool click(const agui::Widget* picture, int x, int y, bool right) {
   const std::byte* widget = self(picture);
   alignas(16) std::byte result[64] = {};
   if (!widget || !blueprintOf(widget) || !select(widget, x, y, result)) return false;
   // The picture acts on what its last paint found under the mouse; the press lands before the
   // next paint, so it acts on this tile.
   std::memcpy(const_cast<std::byte*>(widget + layout.pictureSelection), result, layout.selectionResultSize);
   MapPosition pixel = pixelOf(widget, x, y);
   agui::pressAt(picture, pixel.x, pixel.y, right ? agui::MouseButton::Right : agui::MouseButton::Left, false, false);
   return true;
}

void setGridPosition(const agui::Widget* picture, int x, int y) {
   const std::byte* widget = self(picture);
   if (!widget) return;
   MapPosition pixel = pixelOf(widget, x, y);
   agui::pressAt(picture, pixel.x, pixel.y, agui::MouseButton::Left, true, false);
}

} // namespace fa::preview
