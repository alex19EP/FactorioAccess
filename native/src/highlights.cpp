#include "highlights.h"

#include "game.h"
#include "log.h"
#include "speech.h"
#include "text.h"
#include "vocab.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fa::highlights {

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

struct Position {
   int32_t x;
   int32_t y;
   bool operator==(const Position&) const = default;
};

// The corners of a BoundingBox, which goes on with its orientation.
struct Box {
   Position leftTop;
   Position rightBottom;
};

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

// What a highlight says about its entity, in the order they are said.
enum class Kind { Replaces, Turns, Changes, Keeps, Pair, Power, Wire, Covers, WorksWith, Link, Other };

std::string_view verb(Kind kind) {
   switch (kind) {
   case Kind::Replaces: return vocab::kReplaces;
   case Kind::Turns: return vocab::kTurns;
   case Kind::Changes: return vocab::kChanges;
   case Kind::Keeps: return vocab::kKeeps;
   case Kind::Pair: return vocab::kPairsWith;
   case Kind::Power: return vocab::kPowerFrom;
   case Kind::Wire: return vocab::kWireTo;
   case Kind::Covers: return vocab::kCovers;
   case Kind::WorksWith: return vocab::kWorksWith;
   case Kind::Link: return vocab::kLinksTo;
   case Kind::Other: return vocab::kHighlights;
   }
   return vocab::kHighlights;
}

struct Item {
   Kind kind;
   std::string name;
   Position position;
};

// A highlight box drawn while collecting, named once the preview's surface is known.
struct DrawnBox {
   Kind kind;
   uint8_t type;
   Box box;
};

struct Report {
   std::string meaning;
   bool drag = false;
   // The preview draws the poles that would power it: it runs on electricity.
   bool electric = false;
   bool pole = false;
   // The logistic network the preview would join, as it draws it: its name, or "" for none.
   std::optional<std::string> network;
   Position position{};
   const void* surface = nullptr;
   std::vector<Item> items;
};

// What this thread collects while the preview in hand draws.
struct Collector {
   bool active = false;
   bool havePreview = false;
   // Inside DrawAdapter::renderCursorBox, whose box is named from its entity.
   bool inAdapter = false;
   // Inside DrawAdapter::destroy or setDirectionAndMirroring.
   std::optional<Kind> adapterAction;
   bool inPoleConnections = false;
   std::vector<DrawnBox> boxes;
   Report report;
};
thread_local Collector t_collector;

// A frame's report waits for the roboport lines drawn after every renderer has prepared.
std::mutex g_pendingMutex;
std::optional<Report> g_pending;
// Set on the thread drawing the roboport lines while a report waits for them.
thread_local Report* t_lines = nullptr;

using LocalisedStr = const MsvcString* (*)(const void* localisedString, const void* localeProvider);

std::string entityName(const std::byte* entity) {
   const std::byte* prototype = at<const std::byte*>(entity, layout.entityPrototypeOf);
   const MsvcString* name =
      reinterpret_cast<LocalisedStr>(layout.localisedStringStr)(prototype + layout.prototypeLocalisedName, nullptr);
   return {name->capacity >= sizeof(name->buffer) ? name->pointer : name->buffer, name->size};
}

Position entityPosition(const std::byte* entity) { return at<Position>(entity, layout.entityPosition); }

using IteratorFunction = void (*)(std::byte* iterator);

// The first entity on `surface` whose position lies in the area from `leftTop` to `rightBottom`
// for which `match` holds, or null.
template <class Match>
const std::byte* findEntity(const void* surface, Position leftTop, Position rightBottom, Match&& match) {
   struct AdvancedTile {
      int32_t x;
      int32_t y;
   };
   alignas(8) std::byte iterator[game::kEntityIteratorCapacity]{};
   const AdvancedTile first{leftTop.x >> 9, leftTop.y >> 9};
   const AdvancedTile last{rightBottom.x >> 9, rightBottom.y >> 9};
   std::memcpy(iterator + layout.iteratorSurface, &surface, sizeof(surface));
   std::memcpy(iterator + layout.iteratorLeftTop, &first, sizeof(first));
   std::memcpy(iterator + layout.iteratorRightBottom, &last, sizeof(last));
   std::memcpy(iterator + layout.iteratorCurrentTile, &first, sizeof(first));
   reinterpret_cast<IteratorFunction>(layout.iteratorStartTile)(iterator);
   for (reinterpret_cast<IteratorFunction>(layout.iteratorMove)(iterator);;
        reinterpret_cast<IteratorFunction>(layout.iteratorMove)(iterator)) {
      const std::byte* entity = at<const std::byte*>(iterator, layout.iteratorCurrentEntity);
      if (!entity) return nullptr;
      if (match(entity)) return entity;
   }
}

using SelectionBoxFunction = Box* (*)(const void* entity, std::byte* out, const void* context);

Box selectionBox(const std::byte* entity) {
   // A BoundingBox, and a SelectionContext the call ignores.
   alignas(8) std::byte out[32]{};
   alignas(8) std::byte context[64]{};
   reinterpret_cast<SelectionBoxFunction>(layout.entitySelectionBox)(entity, out, context);
   return at<Box>(out, 0);
}

// The entity a highlight box was drawn on: the one with that selection box.
const std::byte* entityWithBox(const void* surface, const Box& box) {
   constexpr int32_t kMargin = 2 * game::kMapPositionScale;
   return findEntity(surface, {box.leftTop.x - kMargin, box.leftTop.y - kMargin},
                     {box.rightBottom.x + kMargin, box.rightBottom.y + kMargin}, [&](const std::byte* entity) {
                        const Box candidate = selectionBox(entity);
                        return candidate.leftTop == box.leftTop && candidate.rightBottom == box.rightBottom;
                     });
}

const std::byte* entityAt(const void* surface, Position position) {
   return findEntity(surface, position, position,
                     [&](const std::byte* entity) { return entityPosition(entity) == position; });
}

void add(Report& report, Kind kind, const std::byte* entity) {
   Item item{kind, entityName(entity), entityPosition(entity)};
   for (const Item& other : report.items)
      if (other.kind == kind && other.position == item.position && other.name == item.name) return;
   report.items.push_back(std::move(item));
}

Kind boxKind(uint8_t type) {
   switch (type) {
   case game::kCursorBoxPair: return Kind::Pair;
   case game::kCursorBoxNotAllowed: return Kind::Changes;
   case game::kCursorBoxElectricity: return Kind::Covers;
   case game::kCursorBoxTrainVisualization: return Kind::WorksWith;
   default: return Kind::Other;
   }
}

void recordBox(uint8_t type, const Box* box) {
   Collector& collector = t_collector;
   if (!collector.active || collector.inAdapter) return;
   const Kind kind = collector.inPoleConnections ? Kind::Power : boxKind(type);
   collector.boxes.push_back({kind, type, *box});
}

// "3 tiles NorthEast", as the mod says where something is: the distance rounded up, and the
// direction from the preview's tile to the other's, along an axis unless it is well off it.
std::string whereFrom(Position from, Position to) {
   const double dx = static_cast<double>(to.x - from.x) / game::kMapPositionScale;
   const double dy = static_cast<double>(to.y - from.y) / game::kMapPositionScale;
   const int tiles = static_cast<int>(std::ceil(std::hypot(dx, dy)));
   if (tiles == 0) return {};
   const int tileX = (to.x >> 8) - (from.x >> 8);
   const int tileY = (to.y >> 8) - (from.y >> 8);
   int direction = 0;
   if (std::abs(tileX) > 4 * std::abs(tileY))
      direction = tileX > 0 ? 2 : 6;
   else if (std::abs(tileY) > 4 * std::abs(tileX))
      direction = tileY > 0 ? 4 : 0;
   else if (tileX > 0)
      direction = tileY > 0 ? 3 : 1;
   else if (tileX < 0)
      direction = tileY > 0 ? 5 : 7;
   return std::format("{} {} {}", tiles, vocab::kTiles, vocab::kDirections[direction]);
}

// At most this many of a kind are named; the rest are counted.
constexpr size_t kNamedPerKind = 5;

std::string text(Report& report) {
   std::string out = std::move(report.meaning);
   auto append = [&](std::string_view part) {
      if (part.empty()) return;
      if (!out.empty()) out += ", ";
      out += part;
   };
   if (report.drag) return out;
   if (report.network)
      append(report.network->empty() ? std::string(vocab::kNoNetwork)
                                     : std::format("{} {}", vocab::kInNetwork, *report.network));

   std::stable_sort(report.items.begin(), report.items.end(), [&](const Item& a, const Item& b) {
      if (a.kind != b.kind) return a.kind < b.kind;
      auto distance = [&](Position p) {
         return std::hypot(static_cast<double>(p.x - report.position.x), static_cast<double>(p.y - report.position.y));
      };
      return distance(a.position) < distance(b.position);
   });
   bool power = false;
   bool wires = false;
   for (size_t i = 0; i < report.items.size();) {
      const Kind kind = report.items[i].kind;
      power |= kind == Kind::Power;
      wires |= kind == Kind::Wire;
      std::string part(verb(kind));
      size_t count = 0;
      for (; i < report.items.size() && report.items[i].kind == kind; ++i, ++count) {
         if (count == kNamedPerKind) continue;
         const Item& item = report.items[i];
         part += count == 0 ? " " : ", ";
         part += item.name;
         if (std::string where = whereFrom(report.position, item.position); !where.empty()) part += " " + where;
      }
      if (count > kNamedPerKind)
         part += std::format(", {} {} {}", vocab::kAnd, count - kNamedPerKind, vocab::kMore);
      append(part);
   }
   if (report.electric && !power) append(vocab::kNoPower);
   if (report.pole && !wires) append(vocab::kNoWires);
   return out;
}

void speak(Report& report) {
   if (std::string said = text(report); !said.empty()) speech::say(std::move(said), false);
}

// Says a report the roboport lines never came for.
void flushPending() {
   std::optional<Report> pending;
   {
      std::lock_guard lock(g_pendingMutex);
      pending.swap(g_pending);
   }
   if (pending) speak(*pending);
}

// ---- detours ----

using RenderCursorBoxFunction = void (*)(uint32_t type, const Box* box, void* drawQueue, uint64_t layer, uint64_t flag,
                                         double scale, const void* color);
RenderCursorBoxFunction g_renderCursorBoxOriginal = nullptr;

void detourRenderCursorBox(uint32_t type, const Box* box, void* drawQueue, uint64_t layer, uint64_t flag, double scale,
                           const void* color) {
   recordBox(static_cast<uint8_t>(type), box);
   g_renderCursorBoxOriginal(type, box, drawQueue, layer, flag, scale, color);
}

using RenderDoubleCursorBoxFunction = void (*)(uint32_t type, const Box* box, const Box* second, void* drawQueue,
                                               uint64_t layer, const void* color);
RenderDoubleCursorBoxFunction g_renderDoubleCursorBoxOriginal = nullptr;

void detourRenderDoubleCursorBox(uint32_t type, const Box* box, const Box* second, void* drawQueue, uint64_t layer,
                                 const void* color) {
   recordBox(static_cast<uint8_t>(type), box);
   g_renderDoubleCursorBoxOriginal(type, box, second, drawQueue, layer, color);
}

// NamedBool<SkipSurfaceCheckTag> is one byte, passed by value.
using AdapterRenderCursorBoxFunction = void (*)(const void* adapter, const std::byte* entity, uint8_t skipSurfaceCheck,
                                                uint32_t type);
AdapterRenderCursorBoxFunction g_adapterRenderCursorBoxOriginal = nullptr;

void detourAdapterRenderCursorBox(const void* adapter, const std::byte* entity, uint8_t skipSurfaceCheck,
                                  uint32_t type) {
   Collector& collector = t_collector;
   if (!collector.active) return g_adapterRenderCursorBoxOriginal(adapter, entity, skipSurfaceCheck, type);
   const auto boxType = static_cast<uint8_t>(type);
   const Kind kind = collector.adapterAction           ? *collector.adapterAction
                     : boxType == game::kCursorBoxEntity ? Kind::Keeps
                                                         : boxKind(boxType);
   add(collector.report, kind, entity);
   collector.inAdapter = true;
   g_adapterRenderCursorBoxOriginal(adapter, entity, skipSurfaceCheck, type);
   collector.inAdapter = false;
}

using AdapterDestroyFunction = void (*)(const void* adapter, void* entity);
AdapterDestroyFunction g_adapterDestroyOriginal = nullptr;

void detourAdapterDestroy(const void* adapter, void* entity) {
   Collector& collector = t_collector;
   collector.adapterAction = Kind::Replaces;
   g_adapterDestroyOriginal(adapter, entity);
   collector.adapterAction.reset();
}

// Returns an ActionResult through a hidden pointer; Direction and NamedBool<MirroringTag> are one
// byte each.
using AdapterSetDirectionFunction = void* (*)(const void* adapter, void* out, void* entity, uint8_t direction,
                                              uint8_t mirroring);
AdapterSetDirectionFunction g_adapterSetDirectionOriginal = nullptr;

void* detourAdapterSetDirection(const void* adapter, void* out, void* entity, uint8_t direction, uint8_t mirroring) {
   Collector& collector = t_collector;
   collector.adapterAction = Kind::Turns;
   void* result = g_adapterSetDirectionOriginal(adapter, out, entity, direction, mirroring);
   collector.adapterAction.reset();
   return result;
}

using DrawPoleConnectionsFunction = void (*)(void* drawQueue, const void* surface, const Box* box);
DrawPoleConnectionsFunction g_drawPoleConnectionsOriginal = nullptr;

void detourDrawPoleConnections(void* drawQueue, const void* surface, const Box* box) {
   Collector& collector = t_collector;
   if (!collector.active) return g_drawPoleConnectionsOriginal(drawQueue, surface, box);
   collector.report.electric = true;
   collector.inPoleConnections = true;
   g_drawPoleConnectionsOriginal(drawQueue, surface, box);
   collector.inPoleConnections = false;
}

using FindMatchingNetworkFunction = const std::byte* (*)(void* manager, const Position* position);
FindMatchingNetworkFunction g_findMatchingNetworkOriginal = nullptr;

// The custom name, else the number, as the mod names a network.
std::string networkName(const std::byte* network) {
   const auto& name = *reinterpret_cast<const MsvcString*>(network + layout.logisticNetworkName);
   std::string_view custom{name.capacity >= sizeof(name.buffer) ? name.pointer : name.buffer, name.size};
   if (!custom.empty()) return text::speakable(custom);
   return std::to_string(at<uint32_t>(network, layout.logisticNetworkId));
}

const std::byte* detourFindMatchingNetwork(void* manager, const Position* position) {
   const std::byte* network = g_findMatchingNetworkOriginal(manager, position);
   Collector& collector = t_collector;
   if (collector.active && !collector.report.network)
      collector.report.network = network ? networkName(network) : std::string();
   return network;
}

using PostPrepareFunction = void (*)(void* renderer, const void* drawHelpers);
PostPrepareFunction g_postPrepareOriginal = nullptr;

void detourPostPrepare(void* renderer, const void* drawHelpers) {
   std::optional<Report> pending;
   {
      std::lock_guard lock(g_pendingMutex);
      pending.swap(g_pending);
   }
   if (!pending) return g_postPrepareOriginal(renderer, drawHelpers);
   t_lines = pending->drag ? nullptr : &*pending;
   g_postPrepareOriginal(renderer, drawHelpers);
   t_lines = nullptr;
   speak(*pending);
}

// RenderLayer::Enum and Color go on the stack.
using DrawOnTilesBetweenFunction = void (*)(void* drawQueue, const void* sprite, const Position* from,
                                            const Position* to, const void* orientation, uint64_t layer,
                                            const void* color);
DrawOnTilesBetweenFunction g_drawOnTilesBetweenOriginal = nullptr;

void detourDrawOnTilesBetween(void* drawQueue, const void* sprite, const Position* from, const Position* to,
                              const void* orientation, uint64_t layer, const void* color) {
   if (Report* report = t_lines) {
      const Position* other = *from == report->position ? to : *to == report->position ? from : nullptr;
      if (other)
         if (const std::byte* entity = entityAt(report->surface, *other)) add(*report, Kind::Link, entity);
   }
   g_drawOnTilesBetweenOriginal(drawQueue, sprite, from, to, orientation, layer, color);
}

} // namespace

Collecting::Collecting() {
   flushPending();
   t_collector = {};
   t_collector.active = true;
}

Collecting::~Collecting() {
   Collector& collector = t_collector;
   collector.active = false;
   if (!collector.havePreview) return;
   Report& report = collector.report;
   if (!report.drag) {
      for (const DrawnBox& drawn : collector.boxes) {
         const std::byte* entity = entityWithBox(report.surface, drawn.box);
         if (!entity) {
            log::info("Highlight box type {} at {},{} is on no entity", drawn.type, drawn.box.leftTop.x,
                      drawn.box.leftTop.y);
            continue;
         }
         if (drawn.kind == Kind::Other)
            log::info("Highlight box type {} on {}", drawn.type, entityName(entity));
         add(report, drawn.kind, entity);
      }
   }
   std::lock_guard lock(g_pendingMutex);
   g_pending = std::move(report);
}

void preview(const void* settings, const void* entity, std::string meaning, bool drag) {
   Collector& collector = t_collector;
   if (!collector.active) {
      if (!meaning.empty()) speech::say(std::move(meaning), false);
      return;
   }
   const auto* preview = static_cast<const std::byte*>(entity);
   Report& report = collector.report;
   report.meaning = std::move(meaning);
   report.drag = drag;
   report.position = entityPosition(preview);
   report.surface = at<const void*>(preview, layout.entitySurface);
   const std::byte* prototype = at<const std::byte*>(preview, layout.entityPrototypeOf);
   using AsPoleFunction = const void* (*)(const void* prototype);
   report.pole = virtualAt<AsPoleFunction>(prototype, layout.entityPrototypeAsPole)(prototype) != nullptr;
   collector.havePreview = true;
   if (drag) return;

   // The wires a pole would get, to each pole once.
   const auto& wires = at<MsvcVector<const std::byte>>(settings, layout.settingsAddedWires);
   for (const std::byte* wire = wires.first; wire < wires.last; wire += layout.wireSize) {
      const std::byte* source = at<const std::byte*>(wire, layout.wireSource);
      const std::byte* target = at<const std::byte*>(wire, layout.wireTarget);
      const std::byte* other = source == preview ? target : source;
      if (other && other != preview) add(report, Kind::Wire, other);
   }
}

void* renderCursorBoxDetour() { return reinterpret_cast<void*>(&detourRenderCursorBox); }
void** renderCursorBoxOriginal() { return reinterpret_cast<void**>(&g_renderCursorBoxOriginal); }
void* renderDoubleCursorBoxDetour() { return reinterpret_cast<void*>(&detourRenderDoubleCursorBox); }
void** renderDoubleCursorBoxOriginal() { return reinterpret_cast<void**>(&g_renderDoubleCursorBoxOriginal); }
void* adapterRenderCursorBoxDetour() { return reinterpret_cast<void*>(&detourAdapterRenderCursorBox); }
void** adapterRenderCursorBoxOriginal() { return reinterpret_cast<void**>(&g_adapterRenderCursorBoxOriginal); }
void* adapterDestroyDetour() { return reinterpret_cast<void*>(&detourAdapterDestroy); }
void** adapterDestroyOriginal() { return reinterpret_cast<void**>(&g_adapterDestroyOriginal); }
void* adapterSetDirectionDetour() { return reinterpret_cast<void*>(&detourAdapterSetDirection); }
void** adapterSetDirectionOriginal() { return reinterpret_cast<void**>(&g_adapterSetDirectionOriginal); }
void* drawPoleConnectionsDetour() { return reinterpret_cast<void*>(&detourDrawPoleConnections); }
void** drawPoleConnectionsOriginal() { return reinterpret_cast<void**>(&g_drawPoleConnectionsOriginal); }
void* findMatchingNetworkDetour() { return reinterpret_cast<void*>(&detourFindMatchingNetwork); }
void** findMatchingNetworkOriginal() { return reinterpret_cast<void**>(&g_findMatchingNetworkOriginal); }
void* roboportPostPrepareDetour() { return reinterpret_cast<void*>(&detourPostPrepare); }
void** roboportPostPrepareOriginal() { return reinterpret_cast<void**>(&g_postPrepareOriginal); }
void* drawOnTilesBetweenDetour() { return reinterpret_cast<void*>(&detourDrawOnTilesBetween); }
void** drawOnTilesBetweenOriginal() { return reinterpret_cast<void**>(&g_drawOnTilesBetweenOriginal); }

} // namespace fa::highlights
