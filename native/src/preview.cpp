#include "preview.h"

#include "agui.h"
#include "game.h"
#include "text.h"
#include "vocab.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
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
using TileBoxFunction = TileBox* (*)(const void* blueprint, TileBox* out);
using PixelShiftFunction = MapPosition* (*)(const void* picture, MapPosition* out, const MapPosition* absolute);
using DirectionFunction = uint8_t* (*)(const void* entity, uint8_t* out);
using HasDirectionFunction = bool (*)(const void* entity);
using ShowEntityInfoFunction = bool (*)(const void* adapter);
using SignalPrototypeFunction = const std::byte* (*)(const void* signal);
using LocalisedStr = const MsvcString* (*)(const void* localisedString, const void* localeProvider);

// The slots of an inserter's or a loader's std::array of filters.
constexpr size_t kFilterSlots = 5;
// InserterFlags bits, as Inserter::draw reads them in 2.1.21: the filters are in use, and they are
// a blacklist.
constexpr uint16_t kInserterUseFilters = 0x100;
constexpr uint16_t kInserterBlacklist = 0x2;
// What a selector combinator and a constant combinator show at most.
constexpr size_t kConstantSignalsShown = 4;

// The BlueprintWidget a picture is.
const std::byte* self(const agui::Widget* picture) {
   return agui::objectAsBase(picture, ".?AVBlueprintWidget@@");
}

const std::byte* blueprintOf(const std::byte* widget) {
   return at<const std::byte*>(widget, layout.pictureBlueprint);
}

const std::byte* parametersOf(const std::byte* widget) {
   return widget + layout.pictureParameters;
}

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

// An IDWithQualityFilter<ItemID>, empty when unset.
std::string filterName(const std::byte* filter) {
   return named(layout.itemPrototypes, at<uint16_t>(filter, layout.itemFilterId),
                at<uint8_t>(filter, layout.itemFilterQuality));
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

std::vector<std::string> filterNames(const std::byte* filters, size_t count) {
   std::vector<std::string> names;
   for (size_t i = 0; i < count; ++i) {
      std::string name = filterName(filters + i * layout.itemFilterSize);
      if (!name.empty()) names.push_back(std::move(name));
   }
   return names;
}

// "filter A, B" or "blacklist A, B"; a blacklist of nothing is drawn as such, an empty whitelist
// not at all.
void addFilters(std::vector<std::string>& parts, const std::vector<std::string>& names, bool blacklist) {
   if (!names.empty())
      parts.push_back((blacklist ? vocab::kBlacklistOf : vocab::kFilter)(join(names)));
   else if (blacklist)
      parts.emplace_back(vocab::kBlacklist);
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

// What the game draws on an entity at all times besides its picture: which way an underground
// belt or a loader faces, a combinator's operation, a display panel's icon and text.
void addAlwaysShown(std::vector<std::string>& parts, const std::byte* entity, bool alt) {
   bool signals = alt && combinatorSettingsShown();
   if (const std::byte* belt = agui::objectAsBase(entity, ".?AVUndergroundBelt@@")) {
      parts.emplace_back(at<uint8_t>(belt, layout.undergroundType) == layout.undergroundOutput ? vocab::kOutput
                                                                                                : vocab::kInput);
   } else if (const std::byte* loader = agui::objectAsBase(entity, ".?AVLoader@@")) {
      parts.emplace_back(at<uint8_t>(loader, layout.loaderType) == layout.loaderOutput ? vocab::kOutput
                                                                                        : vocab::kInput);
   } else if (const std::byte* arithmetic = agui::objectAsBase(entity, ".?AVArithmeticCombinator@@")) {
      const std::byte* parameters = arithmetic + layout.arithmeticParameters;
      addOperation(parts,
                   wordFor(layout.arithmeticOperations, vocab::kArithmetic,
                           at<uint8_t>(parameters, layout.arithmeticOperation)),
                   operandName(parameters + layout.arithmeticFirst), operandName(parameters + layout.arithmeticSecond),
                   signalName(parameters + layout.arithmeticOutput), signals);
   } else if (const std::byte* decider = agui::objectAsBase(entity, ".?AVDeciderCombinator@@")) {
      const auto& conditions = at<MsvcVector<const std::byte>>(decider, layout.deciderConditions);
      const auto& outputs = at<MsvcVector<const std::byte>>(decider, layout.deciderOutputs);
      std::string output = outputs.first != outputs.last ? signalName(outputs.first + layout.deciderOutputSignal) : "";
      if (conditions.first != conditions.last) {
         const std::byte* condition = conditions.first;
         addOperation(parts,
                      wordFor(layout.comparisons, vocab::kComparisons,
                              at<uint8_t>(condition, layout.conditionComparator)),
                      signalName(condition + layout.conditionFirst), operandName(condition + layout.conditionSecond),
                      output, signals);
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
      if (at<bool>(panel, layout.panelAlwaysShow)) parts.push_back(text::speakable(view(at<MsvcString>(panel, layout.panelText))));
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

// The details alt mode draws on an entity.
void addAltDetails(std::vector<std::string>& parts, const std::byte* widget, const std::byte* entity) {
   if (const std::byte* machine = agui::objectAsBase(entity, ".?AVCraftingMachine@@")) {
      const std::byte* recipe = machine + layout.craftingRecipe;
      std::string name = idName(layout.recipePrototypes, recipe);
      if (!name.empty() && recipeLocked(widget, at<uint16_t>(recipe, layout.idWithQualityBase)))
         name = vocab::kLockedRecipe(name);
      parts.push_back(std::move(name));
   } else if (const std::byte* inserter = agui::objectAsBase(entity, ".?AVInserter@@")) {
      uint16_t flags = at<uint16_t>(inserter, layout.inserterFlags);
      if (flags & kInserterUseFilters)
         addFilters(parts, filterNames(inserter + layout.inserterFilters, kFilterSlots), flags & kInserterBlacklist);
   } else if (const std::byte* logic = [&]() -> const std::byte* {
                 if (const std::byte* splitter = agui::objectAsBase(entity, ".?AVSplitter@@"))
                    return splitter + layout.splitterLogic;
                 if (const std::byte* lane = agui::objectAsBase(entity, ".?AVLaneSplitter@@"))
                    return lane + layout.laneSplitterLogic;
                 return nullptr;
              }()) {
      auto side = [&](uint32_t offset) {
         return at<uint32_t>(logic, offset) == layout.splitterRight ? vocab::kRight : vocab::kLeft;
      };
      if (at<bool>(logic, layout.splitterOutputLocked)) {
         std::string filter = filterName(logic + layout.splitterFilter);
         if (filter.empty())
            parts.push_back(vocab::kOutputPriority(side(layout.splitterGoesTo)));
         else
            parts.push_back(vocab::kFilterToward(filter, side(layout.splitterGoesTo)));
      }
      if (at<bool>(logic, layout.splitterInputLocked))
         parts.push_back(vocab::kInputPriority(side(layout.splitterTakeFrom)));
   } else if (const std::byte* loader = agui::objectAsBase(entity, ".?AVLoader@@")) {
      uint8_t mode = at<uint8_t>(loader, layout.loaderFilterMode);
      if (mode == layout.loaderWhitelist || mode == layout.loaderBlacklist) {
         const std::byte* filters = loader + layout.loaderFilters;
         const std::byte* prototype = agui::objectAsBase(at<const std::byte*>(entity, layout.entityPrototypeOf),
                                                         ".?AVLoaderPrototype@@");
         if (prototype && at<bool>(prototype, layout.loaderPerLane)) {
            std::string left = filterName(filters);
            std::string right = filterName(filters + layout.itemFilterSize);
            if (!left.empty()) parts.push_back(vocab::kLeftLane(left));
            if (!right.empty()) parts.push_back(vocab::kRightLane(right));
         } else {
            addFilters(parts, filterNames(filters, kFilterSlots), mode == layout.loaderBlacklist);
         }
      }
   } else if (const std::byte* constant = agui::objectAsBase(entity, ".?AVConstantCombinator@@")) {
      if (!combinatorSettingsShown()) return;
      const auto& compiled = at<MsvcVector<const std::byte>>(constant, layout.constantSignals);
      std::vector<std::string> names;
      for (const std::byte* entry = compiled.first; entry < compiled.last && names.size() < kConstantSignalsShown;
           entry += layout.compiledFilterSize) {
         std::string name = signalName(entry);
         if (!name.empty()) names.push_back(std::move(name));
      }
      parts.push_back(join(names));
   } else if (const std::byte* pump = agui::objectAsBase(entity, ".?AVPump@@")) {
      std::string fluid = named(layout.fluidPrototypes, at<uint16_t>(pump, layout.pumpFilter), 0);
      if (!fluid.empty()) parts.push_back(vocab::kFilter(fluid));
   } else if (const std::byte* collector = agui::objectAsBase(entity, ".?AVAsteroidCollector@@")) {
      const auto& chunks = at<MsvcVector<const uint16_t>>(collector, layout.collectorFilters);
      std::vector<std::string> names;
      for (const uint16_t* chunk = chunks.first; chunk < chunks.last; ++chunk)
         names.push_back(named(layout.asteroidChunkPrototypes, *chunk, 0));
      addFilters(parts, names, false);
   }
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

std::string describeEntity(const std::byte* widget, const std::byte* entity, uint32_t index) {
   bool alt = altMode(widget);
   std::vector<std::string> parts;
   std::string name = localisedName(at<const std::byte*>(entity, layout.entityPrototypeOf));
   if (alt)
      if (const std::byte* owner = agui::objectAsBase(entity, ".?AVEntityWithOwner@@"))
         name = qualityPrefix(at<uint8_t>(owner, layout.entityQuality)) + name;
   parts.push_back(std::move(name));
   if (virtualAt<HasDirectionFunction>(entity, layout.entityHasDirection)(entity)) {
      uint8_t direction = 0;
      virtualAt<DirectionFunction>(entity, layout.entityGetDirection)(entity, &direction);
      // 16 directions, north 0 clockwise; the mod names eight.
      parts.push_back(vocab::direction((direction / 2) % 8));
   }
   if (removed(widget, layout.parametersEntities, index)) parts.emplace_back(vocab::kRemoved);
   addAlwaysShown(parts, entity, alt);
   if (alt) addAltDetails(parts, widget, entity);
   addDeliveries(parts, blueprintOf(widget), index);
   return join(parts);
}

// What the mouse would pick on the tile, written to `out` (a BlueprintSelectionResult). False for
// an empty tile.
bool select(const std::byte* widget, int x, int y, std::byte* out) {
   if (layout.selectionResultSize > 64) return false;
   MapPosition centre{x * 256 + 128, y * 256 + 128};
   reinterpret_cast<SelectionFunction>(layout.blueprintSelectionAt)(blueprintOf(widget), out, &centre,
                                                                     parametersOf(widget));
   return at<const std::byte*>(out, layout.selectionEntity) || at<uint16_t>(out, layout.selectionTile);
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
      return describeEntity(widget, entity, index);
   std::vector<std::string> parts{named(layout.tilePrototypes, at<uint16_t>(result, layout.selectionTile), 0)};
   if (removed(widget, layout.parametersTiles, index)) parts.emplace_back(vocab::kRemoved);
   return join(parts);
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
