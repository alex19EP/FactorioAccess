#include "movement.h"

#include "game.h"

#include <atomic>
#include <cstddef>
#include <cstring>

namespace fa::movement {

namespace {

using game::layout;

template <class T>
T at(const void* object, uint32_t offset) {
   T value;
   std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
   return value;
}

const std::byte* localPlayer() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   const std::byte* game = context ? at<const std::byte*>(context, layout.globalGame) : nullptr;
   return game ? at<const std::byte*>(game, layout.gameLocalPlayer) : nullptr;
}

// The last step, packed as tick << 10 | count << 2 | step, and the Player* it was taken for: a
// step lapses when another game loads or the player changes.
std::atomic<uint64_t> g_step{0};
std::atomic<const void*> g_player{nullptr};
uint8_t g_count = 0; // written by the game's update only

using ChangePosition = bool (*)(void* character, const void* vector);
ChangePosition g_original = nullptr;

bool changePosition(void* character, const void* vector) {
   const uint64_t before = at<uint64_t>(character, layout.characterPosition);
   const bool full = g_original(character, vector);
   const std::byte* controller = at<const std::byte*>(character, layout.characterController);
   const std::byte* player = controller ? at<const std::byte*>(controller, layout.controllerPlayer) : nullptr;
   if (player && player == localPlayer()) {
      Step step = full                                                       ? Step::Full
                  : at<uint64_t>(character, layout.characterPosition) != before ? Step::Partial
                                                                             : Step::None;
      const uint64_t tick = at<uint64_t>(at<const std::byte*>(character, layout.characterMap), layout.mapUpdateTick);
      g_count++;
      g_step.store(tick << 10 | static_cast<uint64_t>(g_count) << 2 | static_cast<uint64_t>(step));
      g_player.store(player);
   }
   return full;
}

} // namespace

bool recentStep(int playerIndex, Step& out, uint8_t& count) {
   const std::byte* player = localPlayer();
   if (!player || at<uint16_t>(player, layout.playerIndex) + 1 != playerIndex || g_player.load() != player)
      return false;
   const uint64_t packed = g_step.load();
   const uint64_t now = at<uint64_t>(at<const std::byte*>(player, layout.playerMap), layout.mapUpdateTick);
   if ((packed >> 10) + 1 < now) return false;
   out = static_cast<Step>(packed & 3);
   count = static_cast<uint8_t>(packed >> 2);
   return true;
}

void* changePositionDetour() { return reinterpret_cast<void*>(&changePosition); }
void** changePositionOriginal() { return reinterpret_cast<void**>(&g_original); }

} // namespace fa::movement
