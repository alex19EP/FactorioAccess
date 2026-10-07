#include "selectedinfo.h"

#include "game.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

namespace fa::selectedinfo {

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

// MapPosition, which the game passes by value in one register.
struct Position {
   int32_t x;
   int32_t y;
};
// Optional<MapPosition>'s empty value.
constexpr int32_t kNoPosition = 0x7fffffff;

// std::optional<GuiContext>: GuiContext is a Player*. Passed by value, which MSVC does through a
// pointer to a copy.
struct OptionalContext {
   const void* player;
   bool hasValue;
};

enum class Kind { None, Entity, Tile };

constexpr std::align_val_t kAlignment{16};

std::atomic<bool> g_requested{false};

// The panel, which only the main thread touches.
Kind g_kind = Kind::None;
const void* g_object = nullptr; // the Entity or Tile it describes
std::byte* g_panel = nullptr;

const std::byte* localPlayer() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   auto* game = context ? at<const std::byte*>(context, layout.globalGame) : nullptr;
   return game ? at<const std::byte*>(game, layout.gameLocalPlayer) : nullptr;
}

bool onTheSide() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   auto* settings = at<const std::byte*>(context, layout.globalInterfaceSettings);
   return settings && at<bool>(settings, layout.tooltipOnTheSide);
}

const void* selectedEntity(const std::byte* player) {
   const std::byte* adapter = at<const std::byte*>(player, layout.playerLatencyAdapter);
   if (!adapter) adapter = player + layout.playerGameStateAdapter;
   using GetSelector = const std::byte* (*)(const void*);
   const std::byte* selector = virtualAt<GetSelector>(adapter, layout.adapterEntitySelector)(adapter);
   return selector ? at<const void*>(selector, layout.selectorEntity) : nullptr;
}

const void* selectedTile(const std::byte* player) {
   Position position;
   reinterpret_cast<Position* (*)(const void*, Position*)>(layout.playerCursorPosition)(player, &position);
   const std::byte* controller = at<const std::byte*>(player, layout.playerController);
   if (position.x == kNoPosition || !controller) return nullptr;
   using DeduceTile = const void* (*)(const void*, const Position*);
   return virtualAt<DeduceTile>(controller, layout.controllerSelectedTile)(controller, &position);
}

// What the player points at, as GameView::update finds it: the selected entity, else the tile.
Kind pointedAt(const std::byte* player, const void*& object) {
   if ((object = selectedEntity(player))) return Kind::Entity;
   if ((object = selectedTile(player))) return Kind::Tile;
   return Kind::None;
}

void fill() {
   if (g_kind == Kind::Entity) {
      using Update = void (*)(void*, const void* const*, bool);
      reinterpret_cast<Update>(layout.entityInfoUpdate)(g_panel, &g_object, false);
   } else {
      using Change = void (*)(void*, const void*, bool);
      reinterpret_cast<Change>(layout.tileInfoChange)(g_panel, g_object, false);
   }
}

} // namespace

void request() { g_requested.store(true); }

bool takeRequest() { return g_requested.exchange(false); }

const agui::Widget* open() {
   close();
   const std::byte* player = localPlayer();
   if (!player) return nullptr;
   const void* object = nullptr;
   const Kind kind = pointedAt(player, object);
   if (kind == Kind::None) return nullptr;

   const uint32_t size = kind == Kind::Entity ? layout.entityInfoSize : layout.tileInfoSize;
   auto* panel = static_cast<std::byte*>(::operator new(size, kAlignment));
   // The callee owns a by-value argument's copy.
   OptionalContext context{player, true};
   const bool side = onTheSide();
   if (kind == Kind::Entity) {
      using Construct = void* (*)(void*, OptionalContext*, const void* const*, bool);
      reinterpret_cast<Construct>(layout.entityInfoConstruct)(panel, &context, &object, side);
   } else {
      using Construct = void* (*)(void*, OptionalContext*, const void*, bool);
      reinterpret_cast<Construct>(layout.tileInfoConstruct)(panel, &context, object, side);
   }
   g_kind = kind;
   g_object = object;
   g_panel = panel;
   fill();
   return window();
}

const agui::Widget* window() { return reinterpret_cast<const agui::Widget*>(g_panel); }

bool check() {
   if (!g_panel) return false;
   const std::byte* player = localPlayer();
   const void* object = nullptr;
   if (!player || pointedAt(player, object) != g_kind || object != g_object) {
      close();
      return false;
   }
   return true;
}

void close() {
   if (!g_panel) return;
   // Destroys without freeing: the memory is ours.
   const uintptr_t destroy = g_kind == Kind::Entity ? layout.entityInfoDestroy : layout.tileInfoDestroy;
   reinterpret_cast<void* (*)(void*, unsigned)>(destroy)(g_panel, 0);
   ::operator delete(g_panel, kAlignment);
   g_panel = nullptr;
   g_object = nullptr;
   g_kind = Kind::None;
}

} // namespace fa::selectedinfo
