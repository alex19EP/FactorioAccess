#include "world.h"

#include "game.h"

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fa::world {

namespace {

using game::layout;

template <class T>
T at(const void* object, uint32_t offset) {
   T value;
   std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
   return value;
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

// The reported cursor, when it belongs to the current game.
bool cursor(const std::byte* game, Position& out) {
   if (!game || !g_hasCursor.load() || g_game.load() != game) return false;
   out = unpack(g_cursor.load());
   return true;
}

int32_t fixedPoint(double tiles) { return static_cast<int32_t>(std::lround(tiles * game::kMapPositionScale)); }

// Both return their result through a hidden pointer, as MSVC does for member functions.
using CursorFunction = Position* (*)(const void* self, Position* out);
CursorFunction g_playerOriginal = nullptr;
CursorFunction g_sourceOriginal = nullptr;

Position* playerDetour(const void* player, Position* out) {
   const std::byte* game = currentGame();
   Position position;
   if (game && player == localPlayer(game) && cursor(game, position)) {
      *out = position;
      return out;
   }
   return g_playerOriginal(player, out);
}

// There is one PlayerInputSource, this client's.
Position* sourceDetour(const void* source, Position* out) {
   Position position;
   if (cursor(currentGame(), position)) {
      *out = position;
      return out;
   }
   return g_sourceOriginal(source, out);
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

bool mayBeLocalPlayer(int playerIndex) {
   const std::byte* game = currentGame();
   const std::byte* player = game ? localPlayer(game) : nullptr;
   return !player || at<uint16_t>(player, layout.playerIndex) + 1 == playerIndex;
}

void* playerCursorDetour() { return reinterpret_cast<void*>(&playerDetour); }
void** playerCursorOriginal() { return reinterpret_cast<void**>(&g_playerOriginal); }
void* sourceCursorDetour() { return reinterpret_cast<void*>(&sourceDetour); }
void** sourceCursorOriginal() { return reinterpret_cast<void**>(&g_sourceOriginal); }

} // namespace fa::world
