#include "reader.h"

#include "agui.h"
#include "facts.h"
#include "game.h"
#include "graph/GraphAnnouncer.hpp"
#include "graph/GraphBuilder.hpp"
#include "screens/AguiNodes.hpp"
#include "speech.h"
#include "text.h"
#include "vocab.h"

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
constexpr int32_t kChunkScale = kScale * 32;

// std::optional<GuiContext>: GuiContext is a Player*. Passed by value, which MSVC does through a
// pointer to a copy the callee owns.
struct OptionalContext {
   const void* player;
   bool hasValue;
};

constexpr std::align_val_t kAlignment{16};

// A step waiting for the Gui's tick, and the tile last read with the entity it described, which the
// mod's words must match.
struct Reading {
   Position tile;
   std::optional<Entity> entity;
};
std::mutex g_mutex;
std::optional<Position> g_pending;
std::optional<Reading> g_read;

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
// there, so its contents stay in the reading.
bool contentsDrawn(const std::byte* entity) {
   const std::string_view type = typeName(at<const std::byte*>(entity, layout.entityPrototypeOf));
   return type == "container" || type == "logistic-container" || type == "infinity-container" ||
          type == "linked-container";
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
// the surface conditions and debug lines, and the categories' headings.
void prune(const std::byte* entity, std::byte* description) {
   static const std::vector<std::string> fixed = fixedLabels();
   const std::string health = vocab::translate("description.health", {});
   const std::string contents = contentsDrawn(entity) ? vocab::translate("description.contents", {}) : "";
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
      for (std::byte** item = items.first; item < items.last; ++item) {
         auto& label = *reinterpret_cast<MsvcString*>(*item + layout.propertyLabel);
         std::string_view name = label.view();
         if (!name.empty() && name.back() == ':') name.remove_suffix(1);
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
         } else if (keep && !contents.empty() && (name == contents || name.empty())) {
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

std::string tileName(const std::byte* surface, Position tile) {
   const std::byte* found = reinterpret_cast<TileAtFunction>(layout.surfaceTileAt)(surface, &tile);
   if (!found) return vocab::translate("fa.tile-uncharted-out-of-range", {});
   const std::byte* prototype = prototypeAt(layout.tilePrototypes, at<uint16_t>(found, 0));
   return prototype ? localisedName(prototype) : std::string();
}

// What the tile `tile` holds, for the local player, or nothing outside a game with a view.
std::optional<std::string> read(Position tile, std::optional<Entity>& described) {
   const std::byte* game = currentGame();
   if (!game) return std::nullopt;
   const std::byte* player = at<const std::byte*>(game, layout.gameLocalPlayer);
   const std::byte* view = at<const std::byte*>(game, layout.gameView);
   if (!player || !view || at<const std::byte*>(view, layout.gameViewPlayer) != player) return std::nullopt;
   const auto* surface = reinterpret_cast<const std::byte* (*)(const void*)>(layout.gameViewSurface)(view);
   const Position centre{tile.x * kScale + kScale / 2, tile.y * kScale + kScale / 2};
   const std::byte* entity = entityAt(player, surface, centre);
   if (!entity) return tileName(surface, tile);
   const std::byte* prototype = at<const std::byte*>(entity, layout.entityPrototypeOf);
   const Position position = at<Position>(entity, layout.entityPosition);
   described = Entity{std::string(reinterpret_cast<const MsvcString*>(prototype + layout.prototypeName)->view()),
                      static_cast<double>(position.x) / kScale, static_cast<double>(position.y) / kScale};
   return describe(player, entity, tile);
}

} // namespace

void moved(int32_t tileX, int32_t tileY) {
   std::scoped_lock lock(g_mutex);
   g_pending = Position{tileX, tileY};
}

void tick() {
   std::optional<Position> pending;
   {
      std::scoped_lock lock(g_mutex);
      pending = std::exchange(g_pending, std::nullopt);
   }
   if (!pending) return;
   std::optional<Entity> described;
   std::optional<std::string> text = read(*pending, described);
   if (!text) return;
   {
      std::scoped_lock lock(g_mutex);
      g_read = Reading{*pending, std::move(described)};
   }
   speech::say(std::move(*text), true);
}

void more(int32_t tileX, int32_t tileY, std::string_view text) {
   {
      std::scoped_lock lock(g_mutex);
      if (!g_read || g_read->tile.x != tileX || g_read->tile.y != tileY || g_pending) return;
   }
   if (text.find_first_not_of(" \t\r\n") != std::string_view::npos) speech::say(std::string(text), false);
}

std::optional<Entity> entity(int32_t tileX, int32_t tileY) {
   std::scoped_lock lock(g_mutex);
   if (!g_read || g_read->tile.x != tileX || g_read->tile.y != tileY) return std::nullopt;
   return g_read->entity;
}

} // namespace fa::reader
