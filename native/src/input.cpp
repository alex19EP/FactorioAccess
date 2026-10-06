#include "input.h"

#include <windows.h>

#include <cstddef>
#include <cstring>
#include <mutex>
#include <unordered_set>
#include <utility>

namespace fa::input {

namespace {

// SDL 3.2 event layout (SDL_Event is a 128-byte union; SDL_KeyboardEvent for key events).
constexpr uint32_t kEventKeyDown = 0x300;
constexpr uint32_t kEventKeyUp = 0x301;
constexpr size_t kKeyOffset = 0x1c;    // SDL_Keycode key
constexpr size_t kModOffset = 0x20;    // SDL_Keymod mod (16 bits)
constexpr size_t kDownOffset = 0x24;   // bool down
constexpr size_t kRepeatOffset = 0x25; // bool repeat
constexpr uint16_t kModShift = 0x0003;
constexpr uint16_t kModCtrl = 0x00c0;
constexpr uint16_t kModAlt = 0x0300;

// Claims not renewed for this long are ignored.
constexpr ULONGLONG kClaimLifetimeMs = 500;

using PollEvent = bool (*)(void* event);
PollEvent g_original = nullptr;

std::mutex g_mutex;
std::vector<Claim> g_claims;
ULONGLONG g_aliveAt = 0;
std::vector<KeyEvent> g_queue;
// Keys whose press we took. Their repeats and release are ours too, even if the claim went away
// in between: the game must never see a release for a press it did not get, nor miss the release
// of a press we let through.
std::unordered_set<uint32_t> g_held;

template <class T>
T field(const void* event, size_t offset) {
   T value;
   std::memcpy(&value, static_cast<const std::byte*>(event) + offset, sizeof(value));
   return value;
}

uint8_t modifierClass(uint16_t mod) {
   if (mod & kModAlt) return mods::Alt;
   if (mod & kModCtrl) return mods::Ctrl;
   if (mod & kModShift) return mods::Shift;
   return mods::None;
}

bool claimed(uint32_t key, uint8_t modifier) {
   if (GetTickCount64() - g_aliveAt > kClaimLifetimeMs) return false;
   for (const Claim& claim : g_claims)
      if (claim.key == key && (claim.mods & modifier)) return true;
   return false;
}

// Decides whether the game loses this event to us; queues it if so.
bool take(const void* event) {
   auto type = field<uint32_t>(event, 0);
   if (type != kEventKeyDown && type != kEventKeyUp) return false;
   auto key = field<uint32_t>(event, kKeyOffset);
   auto mod = field<uint16_t>(event, kModOffset);
   bool down = field<bool>(event, kDownOffset);

   std::scoped_lock lock(g_mutex);
   if (!g_held.contains(key)) {
      if (!down || !claimed(key, modifierClass(mod))) return false;
      g_held.insert(key);
   } else if (!down) {
      g_held.erase(key);
   }
   // Nobody is draining (the navigator stopped mid-press); keep taking the key but drop the event.
   if (g_queue.size() >= 256) return true;
   g_queue.push_back({key == keys::KeypadEnter ? keys::Return : key, down, field<bool>(event, kRepeatOffset),
                      (mod & kModShift) != 0, (mod & kModCtrl) != 0, (mod & kModAlt) != 0});
   return true;
}

bool detour(void* event) {
   for (;;) {
      if (!g_original(event)) return false;
      // A null event only asks whether one is pending; it cannot be filtered without consuming
      // it, so it is answered truthfully and the next real poll filters.
      if (!event || !take(event)) return true;
   }
}

} // namespace

void* pollEventDetour() { return reinterpret_cast<void*>(&detour); }

void** pollEventOriginal() { return reinterpret_cast<void**>(&g_original); }

void setClaims(std::vector<Claim> claims) {
   std::scoped_lock lock(g_mutex);
   g_claims = std::move(claims);
   g_aliveAt = GetTickCount64();
}

void clearClaims() {
   std::scoped_lock lock(g_mutex);
   g_claims.clear();
}

void keepAlive() {
   std::scoped_lock lock(g_mutex);
   g_aliveAt = GetTickCount64();
}

std::vector<KeyEvent> drain() {
   std::scoped_lock lock(g_mutex);
   return std::exchange(g_queue, {});
}

} // namespace fa::input
