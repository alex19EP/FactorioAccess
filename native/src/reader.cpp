#include "reader.h"

#include "agui.h"
#include "facts.h"
#include "game.h"
#include "mappicture.h"
#include "graph/GraphAnnouncer.hpp"
#include "graph/GraphBuilder.hpp"
#include "screens/AguiNodes.hpp"
#include "speech.h"
#include "text.h"
#include "vocab.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fa::reader {

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

struct MsvcString {
   union {
      char buffer[16];
      char* pointer;
   };
   size_t size;
   size_t capacity;

   char* data() { return capacity >= sizeof(buffer) ? pointer : buffer; }
   std::string_view view() const { return {capacity >= sizeof(buffer) ? pointer : buffer, size}; }
};

template <class T>
struct MsvcVector {
   T* first;
   T* last;
   T* end;
};

// A node of an MSVC std::set<std::string>; the set starts with its head node, whose parent is the
// root.
struct StringNode {
   const StringNode* left;
   const StringNode* parent;
   const StringNode* right;
   char color;
   char isNil;
   MsvcString value;
};

void collect(const StringNode* node, std::vector<std::string>& out) {
   if (node->isNil) return;
   collect(node->left, out);
   out.push_back(text::speakable(node->value.view()));
   collect(node->right, out);
}

// MapPosition and ChunkPosition: two ints.
struct Position {
   int32_t x;
   int32_t y;
};

constexpr int32_t kScale = game::kMapPositionScale;
static_assert(cyclestops::kScale == kScale);
constexpr int32_t kChunkScale = kScale * 32;

// std::optional<GuiContext>: GuiContext is a Player*. Passed by value, which MSVC does through a
// pointer to a copy the callee owns.
struct OptionalContext {
   const void* player;
   bool hasValue;
};

constexpr std::align_val_t kAlignment{16};

// An entity the mod's Lua named, kept by what finds it again: Lua runs on the update thread, which
// may destroy it before the Gui's tick. `entity` is only compared with the entities still near
// `position`, never read, so one that moved since (a fish) is found and one freed is not.
struct Target {
   uint32_t surface;
   const void* prototype;
   Position position;
   const void* entity;
};

// A tile waiting for the Gui's tick, read as the mouse at `point` (fixed point) sees it, or the
// entity `target` on it.
struct Pending {
   Position tile;
   std::optional<Position> point; // where the cursor rests, without one

   std::string prefix;
   std::string suffix;
   std::optional<Target> target;
};
std::mutex g_mutex;
std::optional<Pending> g_pending;

const std::byte* currentGame() {
   const auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   return context ? at<const std::byte*>(context, layout.globalGame) : nullptr;
}

std::string localisedName(const std::byte* prototype) {
   using Str = const MsvcString* (*)(const void* localisedString, const void* localeProvider);
   return text::speakable(
      reinterpret_cast<Str>(layout.localisedStringStr)(prototype + layout.prototypeLocalisedName, nullptr)->view());
}

const std::byte* prototypeAt(uintptr_t list, uint32_t id) {
   const auto& prototypes = *reinterpret_cast<const MsvcVector<const std::byte*>*>(list);
   const size_t count = static_cast<size_t>(prototypes.last - prototypes.first);
   return id < count ? prototypes.first[id] : nullptr;
}

std::string_view typeName(const std::byte* prototype) {
   return virtualAt<const char* (*)(const void*)>(prototype, layout.prototypeGetType)(prototype);
}

// What the mouse at `position` selects, as the game finds it each frame for the local player; null
// in a chunk the player's force does not see into while the view respects fog of war.
const std::byte* entityAt(const std::byte* player, const std::byte* surface, Position position) {
   const std::byte* adapter = at<const std::byte*>(player, layout.playerLatencyAdapter);
   if (!adapter) adapter = player + layout.playerGameStateAdapter;
   using GetSelector = void* (*)(const void*);
   void* selector = virtualAt<GetSelector>(adapter, layout.adapterEntitySelector)(adapter);
   if (!selector) return nullptr;
   if (reinterpret_cast<bool (*)(const void*, bool)>(layout.respectsFogOfWar)(player, true)) {
      const std::byte* map = at<const std::byte*>(player, layout.playerMap);
      const std::byte* force =
         at<const std::byte* const*>(map, layout.mapForces)[at<uint8_t>(player, layout.playerForce)];
      using ChartFunction = const void* (*)(const void* force, uint32_t surfaceIndex);
      const void* chart =
         reinterpret_cast<ChartFunction>(layout.forceChart)(force, at<uint32_t>(surface, layout.surfaceIndex));
      const Position chunk{position.x >> 13, position.y >> 13};
      using CoveredFunction = bool (*)(const void* chart, const Position* chunk);
      if (!chart || reinterpret_cast<CoveredFunction>(layout.chartChunkCovered)(chart, &chunk)) return nullptr;
   }
   using Deduce = const std::byte* (*)(void* selector, const void* surface, const Position* position, bool myCharacter,
                                       void* latency, const void* only, uint32_t mode);
   return reinterpret_cast<Deduce>(layout.deduceSelectedEntity)(selector, surface, &position, false,
                                                                at<void*>(player, layout.playerLatencyState), nullptr,
                                                                layout.selectionModeNormal);
}

// The tooltip's labels of lines that read alike on every entity of a kind, as the game translates
// them now.
std::vector<std::string> fixedLabels() {
   static constexpr std::array kKeys{
      "description.crafting-speed",
      "description.mining-speed",
      "description.mining-area",
      "description.mining-time",
      "description.mined-by",
      "description.max-energy-consumption",
      "description.min-energy-consumption",
      "description.constant-energy-consumption",
      "description.maximum-temperature",
      "description.minimum-temperature",
      "description.wire-reach",
      "description.supply-area",
      "description.rotation-speed",
      "description.can-filter-items",
      "description.belt-speed",
      "description.module-slots",
      "description.storage-size",
      "description.fluid-capacity",
      "description.last-user",
      "description.force",
      "description.range",
      "description.shooting-speed",
      "description.chain-shooting-speed",
      "description.damage",
      "description.stack-size",
      "description.rocket-capacity",
      "description.grid-size",
      "description.shield-capacity",
      "description.available-weapons",
      "description.max-speed",
      "description.vehicle-enter-instruction",
      "description.stop-priority",
      "description.construction-area",
      "description.radar-coverage-distance",
      "description.robot-recharge-rate",
      // Said in the mod's own shorter words (facts.cpp).
      "description.turret-priority-list",
      "description.turret-ignores-unprioritised",
      "description.player-description",
      "airborne-pollutant-name.pollution",
      "airborne-pollutant-name.spores",
      // The energy source's buffer; a shortage is said as the status.
      "description.electricity",
      // Of the pole's network, only how well demand is met is said.
      "gui-electric-network.production-label",
      "gui-electric-network.accumulator-charge",
      // Too much on every step (the user, 2026-10-09); the Y key reads them.
      "description.products-finished",
      "description.energy-consumption",
      "description.fluid-consumption",
      "description.fluid-output",
      "description.temperature",
      "description.pipeline-extent",
      "description.storage",
      // What mining gives, on every tree and rock (the user, 2026-10-10).
      "description.expected-resources",
   };
   std::vector<std::string> labels;
   for (const char* key : kKeys) labels.push_back(vocab::translate(key, {}));
   return labels;
}

bool isProblem(const std::byte* entity) {
   std::array<std::byte, 32> status{};
   if (layout.entityStatusSize > status.size()) return false;
   virtualAt<void* (*)(const void*, void*)>(entity, layout.entityGetStatus)(entity, status.data());
   const auto value = static_cast<uint32_t>(at<uint8_t>(status.data(), 0));
   for (uint32_t problem : layout.problemStatuses)
      if (problem == value) return true;
   return false;
}

// The status line's text: the diode the game draws before it ("[img=utility/status_working] ") is
// a colour, which the reader does not say. Shortened in place, so the game's allocator keeps it.
constexpr std::string_view kDiode = "[img=utility/status_";

void dropDiode(MsvcString& label) {
   const std::string_view text = label.view();
   if (!text.starts_with(kDiode)) return;
   size_t start = text.find(']');
   if (start == std::string_view::npos) return;
   ++start;
   while (start < text.size() && text[start] == ' ') ++start;
   char* data = label.data();
   std::memmove(data, data + start, label.size - start + 1);
   label.size -= start;
}

// Whether alt mode draws the entity's contents as icons: a chest's. A machine draws its recipe
// there, and its contents are in the hover panel only (the Y key).
bool contentsDrawn(const std::byte* entity) {
   const std::string_view type = typeName(at<const std::byte*>(entity, layout.entityPrototypeOf));
   return type == "container" || type == "logistic-container" || type == "infinity-container" ||
          type == "linked-container";
}

// Whether the entity is an ore drawn by how much it holds, which the reader says as how rich it looks
// (facts.cpp) rather than the amount only the hover panel shows.
bool amountDrawn(const std::byte* entity) {
   const std::byte* prototype = at<const std::byte*>(entity, layout.entityPrototypeOf);
   if (typeName(prototype) != "resource") return false;
   const auto& counts = *reinterpret_cast<const MsvcVector<uint32_t>*>(prototype + layout.resourceStageCounts);
   return counts.last - counts.first >= 2;
}

// A ghost's tooltip shows the health of the building it stands for.
bool damaged(const std::byte* entity) {
   if (typeName(at<const std::byte*>(entity, layout.entityPrototypeOf)) == "entity-ghost")
      entity = at<const std::byte*>(entity, layout.scanGhostInner);
   return entity && at<float>(entity, layout.entityHealth) < 1.0f;
}

// Drops from the description the lines the step does not say: what reads alike on every entity of
// the kind, the status unless it is a problem, health unless the entity is damaged, what alt mode
// draws as icons (a chest's contents, fluids), which the mod says from the icons in alt mode only,
// the contents and an ore's amount, which only the hover panel shows elsewhere, the surface
// conditions and debug lines, and the categories' headings.
void prune(const std::byte* entity, std::byte* description) {
   static const std::vector<std::string> fixed = fixedLabels();
   const std::string health = vocab::translate("description.health", {});
   const std::string contents = vocab::translate("description.contents", {});
   const bool chest = contentsDrawn(entity);
   const std::string amount = amountDrawn(entity) ? vocab::translate("description.amount", {}) : "";
   // A fluid's own temperature line ("Temperature: Steam").
   const std::string fluidTemperature = vocab::translate("description.fluid-temperature", {""});
   auto& categories = *reinterpret_cast<MsvcVector<std::byte*>*>(description + layout.descriptionCategories);
   for (std::byte** category = categories.first; category < categories.last; ++category) {
      const uint32_t kind = at<uint32_t>(*category, layout.categoryKind);
      const bool dropAll = kind == layout.categoryDebug || kind == layout.categoryCraftingConditions ||
                           kind == layout.categoryConstructionConditions;
      // Without a category, Description::fillCategory draws the lines with no heading
      // ("Consumes water", "Electric network").
      std::memcpy(*category + layout.categoryKind, &layout.categoryNone, sizeof(uint32_t));
      auto& items = *reinterpret_cast<MsvcVector<std::byte*>*>(*category + layout.categoryProperties);
      std::byte** kept = items.first;
      bool droppedLast = false;
      // The unlabelled lines after "Contents" are its items.
      bool inContents = false;
      for (std::byte** item = items.first; item < items.last; ++item) {
         auto& label = *reinterpret_cast<MsvcString*>(*item + layout.propertyLabel);
         std::string_view name = label.view();
         if (!name.empty() && name.back() == ':') name.remove_suffix(1);
         if (!name.empty()) inContents = name == contents;
         const auto vtable = *reinterpret_cast<const uintptr_t*>(*item);
         bool keep = !dropAll;
         if (keep && vtable == layout.recipeItemVtable) {
            // The alt-mode icons say the recipe.
            keep = false;
         } else if (keep && vtable == layout.fluidBoxItemVtable) {
            // The fluid and its amount: alt mode draws the fluid, which the mod says from the icons.
            keep = false;
         } else if (keep && !fluidTemperature.empty() && name.starts_with(fluidTemperature)) {
            keep = false;
         } else if (keep && name.empty() && vtable == layout.progressBarItemVtable) {
            // An unlabelled bar shows the line before it.
            keep = !droppedLast;
         } else if (keep && name.starts_with(kDiode)) {
            keep = isProblem(entity);
            if (keep) dropDiode(label);
         } else if (keep && name == health) {
            keep = damaged(entity);
         } else if (keep && !amount.empty() && name == amount) {
            keep = false;
         } else if (keep && (inContents || (chest && name.empty()))) {
            // A chest's contents are its inventory's counts (Inventory::addCountsToDescription), a
            // line without a label.
            keep = false;
         } else if (keep) {
            for (const std::string& fixedLabel : fixed)
               if (name == fixedLabel) keep = false;
         }
         droppedLast = !keep;
         if (keep)
            *kept++ = *item;
         else
            virtualAt<void* (*)(void*, unsigned)>(*item, layout.propertyDestroy)(*item, 1);
      }
      items.last = kept;
   }
}

// Empties a string of the description in place.
void clear(std::byte* description, uint32_t offset) {
   auto& string = *reinterpret_cast<MsvcString*>(description + offset);
   string.size = 0;
   string.data()[0] = '\0';
}

// The icon a fluid box's line shows before its name and amount ("Steam: 100 of 100"): a slot with
// nothing counted on it, beside the text that names the fluid again.
void fluidIcons(const agui::Widget* widget, std::vector<const agui::Widget*>& out) {
   // A window keeps its contents among its private children.
   for (const auto children : {agui::children(widget), agui::privateChildren(widget)}) {
      for (const agui::Widget* child : children) {
         if (!agui::isSlotButton(child)) {
            fluidIcons(child, out);
            continue;
         }
         const agui::SlotButton slot = agui::slotButton(child);
         if (slot.count != 0 || slot.name.empty()) continue;
         for (const agui::Widget* sibling : children)
            if (sibling != child && screens::OwnText(sibling).starts_with(slot.name)) out.push_back(child);
      }
   }
}

// The panel's lines as the Y key's screen reads them: labels and values, without the roles and
// positions a navigator adds.
std::vector<std::string> lines(const agui::Widget* panel, const std::vector<std::string>& skip) {
   const text::WithoutDecorations quiet;
   std::vector<const agui::Widget*> icons;
   fluidIcons(panel, icons);
   graph::GraphBuilder builder;
   builder.BeginStop("reader");
   screens::AddSubtree(builder, "reader", panel, std::move(icons));
   const std::unique_ptr<graph::GraphRender> render = builder.Build();
   std::vector<std::string> out;
   // Nothing is built when the description kept no line.
   if (!render) return out;
   for (const graph::GraphNode* node : render->Order) {
      std::string line;
      for (const graph::NodeAnnouncement& part : graph::GraphAnnouncer::EffectiveAnnouncements(node)) {
         if (part.Kind != graph::AnnouncementKinds::Label && part.Kind != graph::AnnouncementKinds::Value &&
             !part.Kind.empty())
            continue;
         std::string piece = part.Text ? part.Text() : std::string();
         if (piece.empty()) continue;
         if (!line.empty()) line += ' ';
         line += piece;
      }
      if (line.empty()) continue;
      bool skipped = false;
      for (const std::string& text : skip)
         if (line == text) skipped = true;
      if (!skipped) out.push_back(std::move(line));
   }
   return out;
}

// The entity's name as the tooltip titles it, with its quality as the title shows it, and FA's
// words for a ghost, whose tooltip is its inner entity's.
std::string nameOf(const std::byte* entity, const std::byte* description) {
   std::string name =
      text::speakable(reinterpret_cast<const MsvcString*>(description + layout.descriptionName)->view());
   const uint8_t quality = at<uint8_t>(description, layout.descriptionQuality);
   const std::byte* qualityPrototype = quality ? prototypeAt(layout.qualityPrototypes, quality) : nullptr;
   if (qualityPrototype &&
       reinterpret_cast<const MsvcString*>(qualityPrototype + layout.prototypeName)->view() != "normal")
      name += " (" + localisedName(qualityPrototype) + ")";
   const std::byte* prototype = at<const std::byte*>(entity, layout.entityPrototypeOf);
   if (typeName(prototype) == "entity-ghost")
      return vocab::translate("fa.ent-info-ghost", {localisedName(prototype), name});
   return name;
}

std::string describe(const std::byte* player, const std::byte* entity, Position tile) {
   const void* object = entity;
   auto* panel = static_cast<std::byte*>(::operator new(layout.entityInfoSize, kAlignment));
   OptionalContext panelContext{player, true};
   using PanelConstruct = void* (*)(void*, OptionalContext*, const void* const*, bool);
   reinterpret_cast<PanelConstruct>(layout.entityInfoConstruct)(panel, &panelContext, &object, false);

   auto* description = static_cast<std::byte*>(::operator new(layout.descriptionSize, kAlignment));
   OptionalContext context{player, true};
   reinterpret_cast<void* (*)(void*, OptionalContext*)>(layout.descriptionConstruct)(description, &context);
   std::memcpy(description + layout.descriptionSourceEntity, &object, sizeof(object));
   virtualAt<void (*)(const void*, void*)>(entity, layout.entityAddToDescription)(entity, description);

   std::vector<std::string> parts{nameOf(entity, description)};
   for (std::string& fact : facts::describe(entity, tile.x, tile.y)) parts.push_back(std::move(fact));
   std::vector<std::string> modLists;
   const auto* head = *reinterpret_cast<const StringNode* const*>(description + layout.descriptionModLists);
   collect(head->parent, modLists);
   prune(entity, description);
   clear(description, layout.descriptionName);
   clear(description, layout.descriptionText);
   reinterpret_cast<void (*)(const void*, void*, bool)>(layout.descriptionFill)(description, panel, false);
   for (std::string& line : lines(reinterpret_cast<const agui::Widget*>(panel), modLists))
      parts.push_back(std::move(line));
   for (std::string& icon : facts::drawnIcons(entity)) parts.push_back(std::move(icon));

   reinterpret_cast<void* (*)(void*, unsigned)>(layout.entityInfoDestroy)(panel, 0);
   ::operator delete(panel, kAlignment);
   reinterpret_cast<void (*)(void*)>(layout.descriptionDestroy)(description);
   ::operator delete(description, kAlignment);

   std::string out;
   for (const std::string& part : parts) {
      if (part.empty()) continue;
      if (!out.empty()) out += ", ";
      out += part;
   }
   return out;
}

using TileAtFunction = const std::byte* (*)(const void* surface, const Position* position);

const std::byte* tilePrototypeAt(const std::byte* surface, Position tile) {
   const std::byte* found = reinterpret_cast<TileAtFunction>(layout.surfaceTileAt)(surface, &tile);
   return found ? prototypeAt(layout.tilePrototypes, at<uint16_t>(found, 0)) : nullptr;
}

// The mod's water tiles (Consts.WATER_TILE_NAMES).
bool isWater(const std::byte* prototype) {
   static constexpr std::array<std::string_view, 7> kWater{
      "water", "deepwater", "water-green", "deepwater-green", "water-shallow", "water-mud", "water-wube"};
   if (!prototype) return false;
   const std::string_view name = reinterpret_cast<const MsvcString*>(prototype + layout.prototypeName)->view();
   return std::ranges::find(kWater, name) != kWater.end();
}

// A water tile's shape, by which of its four sides are water (FaUtils.identify_water_shores).
std::string waterShape(const std::byte* surface, Position tile) {
   auto water = [&](int32_t dx, int32_t dy) {
      return isWater(tilePrototypeAt(surface, Position{tile.x + dx, tile.y + dy}));
   };
   const bool north = water(0, -1), south = water(0, 1), east = water(1, 0), west = water(-1, 0);
   switch (north + south + east + west) {
   case 0: return vocab::kWaterCrevicePit;
   case 1: return vocab::kWaterCreviceEnd;
   case 2: return (north && south) || (east && west) ? vocab::kWaterCrevice : vocab::kWaterShoreCorner;
   case 3: return vocab::kWaterShore;
   default: return vocab::kWaterOpen;
   }
}

std::string tileName(const std::byte* surface, Position tile) {
   const std::byte* prototype = tilePrototypeAt(surface, tile);
   if (!prototype) return {};
   if (isWater(prototype)) return localisedName(prototype) + " " + waterShape(surface, tile);
   return localisedName(prototype);
}

// This client's player and the surface its view shows, while a game with a view is up.
struct Local {
   const std::byte* player;
   const std::byte* surface;
};

std::optional<Local> local() {
   const std::byte* game = currentGame();
   if (!game) return std::nullopt;
   const std::byte* player = at<const std::byte*>(game, layout.gameLocalPlayer);
   const std::byte* view = at<const std::byte*>(game, layout.gameView);
   if (!player || !view || at<const std::byte*>(view, layout.gameViewPlayer) != player) return std::nullopt;
   return Local{player, reinterpret_cast<const std::byte* (*)(const void*)>(layout.gameViewSurface)(view)};
}

Position centreOf(Position tile) { return {tile.x * kScale + kScale / 2, tile.y * kScale + kScale / 2}; }

// What the view draws on the tile's chunk: the live world; nothing, where the player's force has
// not charted it; or under fog of war the chart's picture of it, where the force charted it but
// does not see into it now. Only remote view and the map draw fog: the character and god views
// show the live world everywhere (Player::shouldRespectFogOfWar).
enum class Sight { Live, Uncharted, Fogged };
struct Seen {
   Sight sight;
   const void* chart = nullptr;
};

Seen sightOf(const Local& here, Position tile) {
   if (!reinterpret_cast<bool (*)(const void*, bool)>(layout.respectsFogOfWar)(here.player, true)) return {Sight::Live};
   const std::byte* map = at<const std::byte*>(here.player, layout.playerMap);
   const std::byte* force =
      at<const std::byte* const*>(map, layout.mapForces)[at<uint8_t>(here.player, layout.playerForce)];
   const uint32_t surfaceIndex = at<uint32_t>(here.surface, layout.surfaceIndex);
   const Position centre = centreOf(tile);
   using ChartedFunction = bool (*)(const void* force, uint32_t surfaceIndex, const Position* position);
   if (!reinterpret_cast<ChartedFunction>(layout.forceIsChunkCharted)(force, surfaceIndex, &centre))
      return {Sight::Uncharted};
   using ChartFunction = const void* (*)(const void* force, uint32_t surfaceIndex);
   const void* chart = reinterpret_cast<ChartFunction>(layout.forceChart)(force, surfaceIndex);
   const Position chunk{centre.x >> 13, centre.y >> 13};
   using CoveredFunction = bool (*)(const void* chart, const Position* chunk);
   if (!chart) return {Sight::Uncharted};
   if (reinterpret_cast<CoveredFunction>(layout.chartChunkCovered)(chart, &chunk)) return {Sight::Fogged, chart};
   return {Sight::Live};
}

// What the chart's picture shows on the tile (mappicture.h), if anything.
std::optional<mappicture::Picture> pictureAt(const Local& here, const void* chart, Position tile) {
   const std::optional<mappicture::Shown> shown =
      mappicture::at(chart, tile.x, tile.y, tilePrototypeAt(here.surface, tile));
   return shown ? shown->picture : std::nullopt;
}

std::string pictureOf(const Local& here, const void* chart, Position tile) {
   const std::optional<mappicture::Picture> shown = pictureAt(here, chart, tile);
   if (!shown) return {};
   const mappicture::Picture& picture = *shown;
   switch (picture.kind) {
   case mappicture::Picture::Kind::Tile:
   case mappicture::Picture::Kind::Entity: return localisedName(static_cast<const std::byte*>(picture.prototype));
   case mappicture::Picture::Kind::Building: return vocab::kMapBuilding;
   case mappicture::Picture::Kind::Enemy: return vocab::kMapEnemy;
   case mappicture::Picture::Kind::Ghost: return vocab::kMapGhost;
   case mappicture::Picture::Kind::TileGhost: return vocab::kMapTileGhost;
   }
   return {};
}

// What the tile `tile` holds where the cursor rests in it (landingIn), or with `where` what the
// mouse at that point selects, as the player's view shows it, or nothing outside a game with a view.
std::optional<std::string> readTile(Position tile, std::optional<Position> where) {
   const std::optional<Local> here = local();
   if (!here) return std::nullopt;
   const Seen seen = sightOf(*here, tile);
   // The view shows black there.
   if (seen.sight == Sight::Uncharted) return std::string(vocab::kUncharted);
   if (seen.sight == Sight::Fogged) {
      const std::string picture = pictureOf(*here, seen.chart, tile);
      const std::string blurred = vocab::kBlurred;
      return picture.empty() ? blurred : picture + ", " + blurred;
   }
   const Point landing = where ? Point{where->x, where->y} : landingIn(tile.x, tile.y);
   const Position point{landing.x, landing.y};
   const std::byte* entity = entityAt(here->player, here->surface, point);
   // A tile the map has not generated says so, and no more.
   if (!entity && !reinterpret_cast<TileAtFunction>(layout.surfaceTileAt)(here->surface, &tile))
      return vocab::translate("fa.tile-uncharted-out-of-range", {});
   std::string text = entity ? describe(here->player, entity, tile) : tileName(here->surface, tile);
   // The view shows the player's own character standing on its tile, though the mouse never
   // selects it there: the character is said first, then what the mouse selects.
   const auto* character = reinterpret_cast<const std::byte* (*)(const void*)>(layout.playerGetCharacter)(here->player);
   const std::byte* self = character ? agui::objectAsBase(character, ".?AVEntity@@") : nullptr;
   if (self && at<const std::byte*>(self, layout.entitySurface) == here->surface) {
      const Position feet = at<Position>(self, layout.entityPosition);
      if (feet.x >> 8 == tile.x && feet.y >> 8 == tile.y) text = describe(here->player, self, tile) + ", " + text;
   }
   return text;
}

// The entity `target` as the tile reader says it on its own tile, wherever the view is; empty when
// it has gone or is on a surface the view does not show, and nothing outside a game with a view.
std::optional<std::string> readTarget(const Target& target) {
   const std::optional<Local> here = local();
   if (!here) return std::nullopt;
   if (at<uint32_t>(here->surface, layout.surfaceIndex) != target.surface) return std::string();
   const Position where = target.position;
   // A box around the position, wide enough for an entity whose own box is empty and for one that
   // moved less than a tile since.
   for (const void* found :
        facts::entitiesIn(here->surface, where.x - kScale, where.y - kScale, where.x + kScale, where.y + kScale)) {
      const auto* entity = static_cast<const std::byte*>(found);
      if (found != target.entity || at<const void*>(entity, layout.entityPrototypeOf) != target.prototype) continue;
      const Position now = at<Position>(entity, layout.entityPosition);
      return describe(here->player, entity, Position{now.x >> 8, now.y >> 8});
   }
   return std::string();
}

} // namespace

void read(int32_t tileX, int32_t tileY, std::string prefix) {
   const Position tile{tileX, tileY};
   std::scoped_lock lock(g_mutex);
   g_pending = Pending{tile, std::nullopt, std::move(prefix), {}};
}

void readPoint(int32_t x, int32_t y, std::string suffix) {
   std::scoped_lock lock(g_mutex);
   g_pending = Pending{Position{x >> 8, y >> 8}, Position{x, y}, {}, std::move(suffix)};
}

void readEntity(const void* entity, std::string prefix, std::string suffix) {
   const auto* surface = at<const std::byte*>(entity, layout.entitySurface);
   const Target target{at<uint32_t>(surface, layout.surfaceIndex), at<const void*>(entity, layout.entityPrototypeOf),
                       at<Position>(entity, layout.entityPosition), entity};
   std::scoped_lock lock(g_mutex);
   g_pending = Pending{Position{target.position.x >> 8, target.position.y >> 8}, target.position, std::move(prefix),
                       std::move(suffix), target};
}

void tick() {
   std::optional<Pending> pending;
   {
      std::scoped_lock lock(g_mutex);
      pending = std::exchange(g_pending, std::nullopt);
   }
   if (!pending) return;
   std::optional<std::string> text =
      pending->target ? readTarget(*pending->target) : readTile(pending->tile, pending->point);
   if (!text) return;
   // The mod's own words may end in their own comma ("Unanchored cursor,").
   if (!pending->prefix.empty()) {
      const char* separator = text->empty() ? "" : pending->prefix.ends_with(',') ? " " : ", ";
      *text = pending->prefix + separator + *text;
   }
   if (!pending->suffix.empty()) *text += (text->empty() ? "" : ", ") + pending->suffix;
   speech::say(std::move(*text), true);
}

const void* selectedAt(int32_t tileX, int32_t tileY) {
   const Position centre = centreOf(Position{tileX, tileY});
   return selectedAtPoint(centre.x, centre.y);
}

std::optional<Shown> shownAt(int32_t tileX, int32_t tileY) {
   const std::optional<Local> here = local();
   if (!here) return std::nullopt;
   const Position tile{tileX, tileY};
   const Seen seen = sightOf(*here, tile);
   switch (seen.sight) {
   case Sight::Live: return std::nullopt;
   case Sight::Uncharted: return Shown{false, std::nullopt};
   case Sight::Fogged: return Shown{true, pictureAt(*here, seen.chart, tile)};
   }
   return std::nullopt;
}

const void* selectedAtPoint(int32_t x, int32_t y) {
   const std::optional<Local> here = local();
   return here ? entityAt(here->player, here->surface, Position{x, y}) : nullptr;
}

bool waterAt(int32_t tileX, int32_t tileY) {
   const std::optional<Local> here = local();
   return here && isWater(tilePrototypeAt(here->surface, Position{tileX, tileY}));
}

std::vector<Stop> stopsIn(int32_t tileX, int32_t tileY) {
   const std::optional<Local> here = local();
   if (!here) return {};
   const int32_t left = tileX * kScale, top = tileY * kScale;
   const Position centre = centreOf(Position{tileX, tileY});

   // A box may reach past its entity's own (a tree's crown): the search looks a tile around.
   std::vector<cyclestops::Box> boxes;
   for (const void* entity :
        facts::entitiesIn(here->surface, left - kScale, top - kScale, left + 2 * kScale, top + 2 * kScale)) {
      const facts::SelectionBox box = facts::selectionBox(entity);
      boxes.push_back({box.left, box.top, box.right, box.bottom});
   }

   alignas(8) std::byte context[32]{};
   const bool ordered = layout.selectionContextSize <= sizeof(context);
   if (ordered) {
      std::memcpy(context + layout.selectionContextPosition, &centre, sizeof(centre));
      context[layout.selectionContextMode] = static_cast<std::byte>(layout.selectionModeNormal);
   }
   using Less = bool (*)(const void* a, const void* b, const void* context);
   const auto less = reinterpret_cast<Less>(layout.selectPriorityLess);
   return cyclestops::stopsIn<const void*>(
      tileX, tileY, boxes, [&](int32_t x, int32_t y) { return entityAt(here->player, here->surface, Position{x, y}); },
      // The game's "loses to": the one that takes the mouse comes first.
      [&](const void* a, const void* b) { return ordered && less(b, a, context); });
}

Point landingIn(int32_t tileX, int32_t tileY) {
   const Position centre = centreOf(Position{tileX, tileY});
   for (const Stop& stop : stopsIn(tileX, tileY)) {
      const auto* entity = static_cast<const std::byte*>(stop.entity);
      if (!entity || facts::skipsOver(entity) ||
          typeName(at<const std::byte*>(entity, layout.entityPrototypeOf)) == "resource")
         continue;
      // Away from the centre, only what lies on this tile: a small box reaches over its edges.
      const Position position = at<Position>(entity, layout.entityPosition);
      const bool centred = stop.x == centre.x && stop.y == centre.y;
      if (!centred && (position.x >> 8 != tileX || position.y >> 8 != tileY)) continue;
      return Point{stop.x, stop.y};
   }
   return Point{centre.x, centre.y};
}

} // namespace fa::reader
