#include "preview.h"

#include "agui.h"
#include "facts.h"
#include "game.h"
#include "text.h"
#include "vocab.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
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
using ShowEntityInfoFunction = bool (*)(const void* adapter);
using SignalPrototypeFunction = const std::byte* (*)(const void* signal);
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

// An IDWithQuality<ItemID> or <RecipeID>.
std::string idName(uintptr_t list, const std::byte* id) {
   return named(list, at<uint16_t>(id, layout.idWithQualityBase), at<uint8_t>(id, layout.idWithQualityQuality));
}

// A SignalID (an item, fluid, virtual signal, entity, recipe and so on), empty when unset. The
// game takes a signal as set when the index in its low 16 bits is.
std::string signalName(const std::byte* signal) {
   if ((at<uint32_t>(signal, 0) & 0xffff) == 0) return {};
   const std::byte* prototype = reinterpret_cast<SignalPrototypeFunction>(layout.signalPrototype)(signal);
   if (!prototype) return {};
   return qualityPrefix(at<uint8_t>(signal, layout.signalQuality)) + localisedName(prototype);
}

// A SignalOrConstant's signal; a constant shows no icon.
std::string operandName(const std::byte* operand) {
   if (at<uint8_t>(operand, layout.signalOrConstantType) != layout.signalOrConstantIsSignal) return {};
   return signalName(operand + layout.signalOrConstantSignal);
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

template <size_t N>
std::string wordFor(const uint32_t (&values)[N], const vocab::Word (&words)[N], uint32_t value) {
   for (size_t i = 0; i < N; ++i)
      if (values[i] == value) return words[i];
   return {};
}

bool altMode(const std::byte* widget) {
   const std::byte* player = at<const std::byte*>(widget, layout.picturePlayer);
   if (!player) return false;
   const std::byte* adapter = at<const std::byte*>(player, layout.playerLatencyAdapter);
   if (!adapter) adapter = player + layout.playerGameStateAdapter;
   return virtualAt<ShowEntityInfoFunction>(adapter, layout.adapterShowEntityInfo)(adapter);
}

// The interface setting that adds a combinator's signals to alt mode.
bool combinatorSettingsShown() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   const std::byte* settings = context ? at<const std::byte*>(context, layout.globalInterfaceSettings) : nullptr;
   return settings && at<bool>(settings, layout.showCombinatorSettings);
}

// A combinator's operation, drawn on its display at all times; with alt mode and the setting, the
// signals around it ("Iron plate multiply Copper plate, output Signal A").
void addOperation(std::vector<std::string>& parts, std::string_view operation, const std::string& first,
                  const std::string& second, const std::string& output, bool signals) {
   if (signals && (!first.empty() || !second.empty()))
      parts.push_back(join({first, std::string(operation), second}, " "));
   else if (!operation.empty())
      parts.emplace_back(operation);
   if (signals && !output.empty()) parts.push_back(vocab::kOutputSignal(output));
}

// What the game draws on an entity at all times besides its picture and its icons: which way a
// loader faces, a combinator's operation, a display panel's icon and text.
void addAlwaysShown(std::vector<std::string>& parts, const std::byte* entity, bool alt) {
   bool signals = alt && combinatorSettingsShown();
   if (const std::byte* loader = agui::objectAsBase(entity, ".?AVLoader@@")) {
      parts.emplace_back(at<uint8_t>(loader, layout.loaderType) == layout.loaderOutput ? vocab::kOutput
                                                                                       : vocab::kInput);
   } else if (const std::byte* arithmetic = agui::objectAsBase(entity, ".?AVArithmeticCombinator@@")) {
      const std::byte* parameters = arithmetic + layout.arithmeticParameters;
      addOperation(
         parts,
         wordFor(layout.arithmeticOperations, vocab::kArithmetic, at<uint8_t>(parameters, layout.arithmeticOperation)),
         operandName(parameters + layout.arithmeticFirst), operandName(parameters + layout.arithmeticSecond),
         signalName(parameters + layout.arithmeticOutput), signals);
   } else if (const std::byte* decider = agui::objectAsBase(entity, ".?AVDeciderCombinator@@")) {
      const auto& conditions = at<MsvcVector<const std::byte>>(decider, layout.deciderConditions);
      const auto& outputs = at<MsvcVector<const std::byte>>(decider, layout.deciderOutputs);
      std::string output = outputs.first != outputs.last ? signalName(outputs.first + layout.deciderOutputSignal) : "";
      if (conditions.first != conditions.last) {
         const std::byte* condition = conditions.first;
         addOperation(
            parts, wordFor(layout.comparisons, vocab::kComparisons, at<uint8_t>(condition, layout.conditionComparator)),
            signalName(condition + layout.conditionFirst), operandName(condition + layout.conditionSecond), output,
            signals);
      } else if (signals && !output.empty()) {
         parts.push_back(vocab::kOutputSignal(output));
      }
   } else if (const std::byte* selector = agui::objectAsBase(entity, ".?AVSelectorCombinator@@")) {
      const std::byte* parameters = selector + layout.selectorParameters;
      uint8_t operation = at<uint8_t>(parameters, layout.selectorOperation);
      std::string word;
      if (operation != layout.selectorSelect)
         word = wordFor(layout.selectorOperations, vocab::kSelector, operation);
      else
         word = at<bool>(parameters, layout.selectorMax) ? vocab::kSelectMaximum : vocab::kSelectMinimum;
      if (!word.empty()) parts.push_back(std::move(word));
      // As SelectorCombinator::draw: only with an index signal set, which Count shows its count
      // signal in place of.
      std::string index = signalName(parameters + layout.selectorIndexSignal);
      if (signals && !index.empty()) {
         if (operation == layout.selectorSelect)
            parts.push_back(std::move(index));
         else if (operation == layout.selectorCount)
            parts.push_back(signalName(parameters + layout.selectorCountSignal));
      }
   } else if (const std::byte* panel = agui::objectAsBase(entity, ".?AVDisplayPanel@@")) {
      parts.push_back(signalName(panel + layout.panelIcon));
      if (at<bool>(panel, layout.panelAlwaysShow))
         parts.push_back(text::speakable(view(at<MsvcString>(panel, layout.panelText))));
   }
}

// Whether the picture draws the recipe crossed out: the player's force has not unlocked it, and
// the player is not in the map editor. As CraftingMachine::draw decides it for a blueprint.
bool recipeLocked(const std::byte* widget, uint16_t recipe) {
   const std::byte* player = at<const std::byte*>(widget, layout.picturePlayer);
   uint8_t force = player ? at<uint8_t>(player, layout.playerForce) : 0;
   if (!recipe || !force) return false;
   const std::byte* controller = at<const std::byte*>(player, layout.playerControllerBeforePause);
   if (!controller) controller = at<const std::byte*>(player, layout.playerController);
   if (controller && agui::objectAsBase(controller, ".?AVEditorController@@")) return false;
   const std::byte* map = at<const std::byte*>(player, layout.playerMap);
   const std::byte* forceData = at<const std::byte* const*>(map, layout.mapForces)[force];
   const std::byte* recipes = forceData ? at<const std::byte*>(forceData, layout.forceRecipes) : nullptr;
   if (!recipes) return false;
   const auto& instances = at<MsvcVector<const std::byte>>(recipes, layout.recipeInstances);
   const std::byte* instance = instances.first + size_t{recipe} * layout.recipeSize;
   return instance < instances.last && !at<bool>(instance, layout.recipeEnabled);
}

// A crafting machine's recipe the picture draws crossed out, with alt mode on, as the icons say
// the recipe.
void addLockedRecipe(std::vector<std::string>& parts, const std::byte* widget, const std::byte* entity) {
   const std::byte* machine = agui::objectAsBase(entity, ".?AVCraftingMachine@@");
   if (!machine) return;
   const std::byte* recipe = machine + layout.craftingRecipe;
   const uint16_t id = at<uint16_t>(recipe, layout.idWithQualityBase);
   if (id && recipeLocked(widget, id)) parts.emplace_back(vocab::kRecipeLocked);
}

// The items to be delivered to the blueprint's entity at `index`, which the picture marks on it:
// "with 2 Speed module, 1 Coal".
void addDeliveries(std::vector<std::string>& parts, const std::byte* blueprint, uint32_t index) {
   const auto& entities = at<MsvcVector<const std::byte>>(blueprint, layout.blueprintEntityList);
   const std::byte* data = entities.first + size_t{index} * layout.entityDataSize;
   if (data >= entities.last) return;
   const auto& plan = at<MsvcVector<const std::byte>>(data, layout.entityDataInsertPlan);
   std::vector<std::string> items;
   for (const std::byte* pair = plan.first; pair < plan.last; pair += layout.insertPairSize) {
      const std::byte* positions = pair + layout.insertPairPositions;
      uint64_t count = at<uint32_t>(positions, layout.positionsGridCount);
      const auto& stacks = at<MsvcVector<const std::byte>>(positions, layout.positionsStacks);
      for (const std::byte* stack = stacks.first; stack < stacks.last; stack += layout.stackLocationSize)
         count += at<uint32_t>(stack, layout.stackLocationCount);
      items.push_back(std::format("{} {}", count, idName(layout.itemPrototypes, pair)));
   }
   if (!items.empty()) parts.push_back(vocab::kWith(join(items)));
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
   bool alt = altMode(widget);
   std::vector<std::string> parts;
   std::string name = localisedName(at<const std::byte*>(entity, layout.entityPrototypeOf));
   if (alt)
      if (const std::byte* owner = agui::objectAsBase(entity, ".?AVEntityWithOwner@@"))
         name = qualityPrefix(at<uint8_t>(owner, layout.entityQuality)) + name;
   parts.push_back(std::move(name));
   if (removed(widget, layout.parametersEntities, index)) parts.emplace_back(vocab::kRemoved);
   for (std::string& fact : facts::describeDesign(entity, x, y, designOf(widget))) parts.push_back(std::move(fact));
   addAlwaysShown(parts, entity, alt);
   for (std::string& icon : facts::drawnIcons(entity, widget + layout.pictureRenderParameters))
      parts.push_back(std::move(icon));
   if (alt) addLockedRecipe(parts, widget, entity);
   addDeliveries(parts, blueprintOf(widget), index);
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
