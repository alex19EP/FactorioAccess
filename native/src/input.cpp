#include "input.h"

#include <windows.h>

#include <cstddef>
#include <cstring>
#include <deque>
#include <mutex>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace fa::input {

namespace {

// SDL 3.2 event layout (SDL_Event is a 128-byte union; SDL_KeyboardEvent for key events).
constexpr uint32_t kEventKeyDown = 0x300;
constexpr uint32_t kEventKeyUp = 0x301;
constexpr uint32_t kEventWindowFirst = 0x202;
constexpr uint32_t kEventWindowLast = 0x21f;
constexpr uint32_t kEventMouseFirst = 0x400;
constexpr uint32_t kEventMouseLast = 0x403;
constexpr size_t kEventSize = 128;
constexpr size_t kTimestampOffset = 0x08; // Uint64 timestamp
constexpr size_t kWindowOffset = 0x10;    // SDL_WindowID windowID
constexpr size_t kScancodeOffset = 0x18;  // SDL_Scancode scancode
constexpr size_t kKeyOffset = 0x1c;    // SDL_Keycode key
constexpr size_t kModOffset = 0x20;    // SDL_Keymod mod (16 bits)
constexpr size_t kDownOffset = 0x24;   // bool down
constexpr size_t kRepeatOffset = 0x25; // bool repeat
constexpr uint16_t kModShift = 0x0003;
constexpr uint16_t kModCtrl = 0x00c0;
constexpr uint16_t kModAlt = 0x0300;
constexpr uint16_t kModLeftShift = 0x0001;
constexpr uint16_t kModLeftCtrl = 0x0040;
constexpr uint16_t kModLeftAlt = 0x0100;

// Claims not renewed for this long are ignored.
constexpr ULONGLONG kClaimLifetimeMs = 500;

using PollEvent = bool (*)(void* event);
PollEvent g_original = nullptr;

struct Channel {
   std::vector<Claim> claims;
   ULONGLONG aliveAt = 0;
   std::vector<KeyEvent> queue;
};

std::mutex g_mutex;
Channel g_navigator;
Channel g_world;
// Keys whose press we took, and the channel that took it. Their repeats and release go there too,
// even if the claim went away in between: the game must never see a release for a press it did
// not get, nor miss the release of a press we let through.
std::unordered_map<uint32_t, Channel*> g_held;

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

bool claimed(const Channel& channel, uint32_t key, uint8_t modifier) {
   if (GetTickCount64() - channel.aliveAt > kClaimLifetimeMs) return false;
   for (const Claim& claim : channel.claims)
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
   Channel* channel = nullptr;
   if (auto held = g_held.find(key); held != g_held.end()) {
      channel = held->second;
      if (!down) g_held.erase(held);
   } else {
      if (!down) return false;
      uint8_t modifier = modifierClass(mod);
      if (claimed(g_navigator, key, modifier))
         channel = &g_navigator;
      else if (claimed(g_world, key, modifier))
         channel = &g_world;
      else
         return false;
      g_held.emplace(key, channel);
   }
   // Nobody is draining (the owner stopped mid-press); keep taking the key but drop the event.
   if (channel->queue.size() >= 256) return true;
   channel->queue.push_back({key == keys::KeypadEnter ? keys::Return : key, down, field<bool>(event, kRepeatOffset),
                             (mod & kModShift) != 0, (mod & kModCtrl) != 0, (mod & kModAlt) != 0});
   return true;
}

// The window and clock of the newest event the game got, so an injected key reads as one of its
// own. Only the game thread touches these.
uint32_t g_windowId = 0;
uint64_t g_timestamp = 0;

void remember(const void* event) {
   auto type = field<uint32_t>(event, 0);
   g_timestamp = field<uint64_t>(event, kTimestampOffset);
   // Window, keyboard and mouse events all carry the window id at the same place.
   if ((type >= kEventWindowFirst && type <= kEventWindowLast) || type == kEventKeyDown || type == kEventKeyUp ||
       (type >= kEventMouseFirst && type <= kEventMouseLast))
      g_windowId = field<uint32_t>(event, kWindowOffset);
}

template <class T>
void put(void* event, size_t offset, T value) {
   std::memcpy(static_cast<std::byte*>(event) + offset, &value, sizeof(value));
}

// SDL scancodes for the keycodes injectKey takes.
uint32_t scancode(uint32_t key) {
   if (key >= 'a' && key <= 'z') return 4 + (key - 'a');
   if (key >= '1' && key <= '9') return 30 + (key - '1');
   switch (key) {
   case '0': return 39;
   case keys::Return: return 40;
   case keys::Escape: return 41;
   case keys::Backspace: return 42;
   case keys::Tab: return 43;
   case keys::Space: return 44;
   case keys::LeftBracket: return 47;
   case keys::RightBracket: return 48;
   case keys::Backslash: return 49;
   case keys::LeftCtrl: return 224;
   case keys::LeftShift: return 225;
   case keys::LeftAlt: return 226;
   case keys::F1: return 58;
   case keys::Home: return 74;
   case keys::PageUp: return 75;
   case keys::Delete: return 76;
   case keys::End: return 77;
   case keys::PageDown: return 78;
   case keys::Right: return 79;
   case keys::Left: return 80;
   case keys::Down: return 81;
   case keys::Up: return 82;
   default: return 0;
   }
}

struct Injected {
   uint32_t key;
   uint16_t mod;
   bool down;
};

std::deque<Injected> g_injected; // under g_mutex

// Fills `event` with the next injected key, if any; a null event only asks whether one is waiting.
bool nextInjected(void* event) {
   std::scoped_lock lock(g_mutex);
   if (g_injected.empty()) return false;
   if (!event) return true;
   Injected next = g_injected.front();
   g_injected.pop_front();
   std::memset(event, 0, kEventSize);
   put(event, 0, next.down ? kEventKeyDown : kEventKeyUp);
   put(event, kTimestampOffset, g_timestamp);
   put(event, kWindowOffset, g_windowId);
   put(event, kScancodeOffset, scancode(next.key));
   put(event, kKeyOffset, next.key);
   put(event, kModOffset, next.mod);
   put(event, kDownOffset, next.down);
   return true;
}

bool detour(void* event) {
   for (;;) {
      if (!nextInjected(event)) {
         if (!g_original(event)) return false;
         if (event) remember(event);
      }
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
   g_navigator.claims = std::move(claims);
   g_navigator.aliveAt = GetTickCount64();
}

void clearClaims() {
   std::scoped_lock lock(g_mutex);
   g_navigator.claims.clear();
}

void keepAlive() {
   std::scoped_lock lock(g_mutex);
   g_navigator.aliveAt = GetTickCount64();
}

std::vector<KeyEvent> drain() {
   std::scoped_lock lock(g_mutex);
   return std::exchange(g_navigator.queue, {});
}

void injectKey(uint32_t key, bool shift, bool ctrl, bool alt) {
   uint16_t mod = (shift ? kModLeftShift : 0) | (ctrl ? kModLeftCtrl : 0) | (alt ? kModLeftAlt : 0);
   std::scoped_lock lock(g_mutex);
   g_injected.push_back({key, mod, true});
   g_injected.push_back({key, mod, false});
}

void injectModifiers(bool shift, bool ctrl, bool alt, bool down) {
   std::scoped_lock lock(g_mutex);
   for (auto [held, key, mod] : {std::tuple{shift, keys::LeftShift, kModLeftShift},
                                 std::tuple{ctrl, keys::LeftCtrl, kModLeftCtrl}, std::tuple{alt, keys::LeftAlt, kModLeftAlt}})
      if (held) g_injected.push_back({key, down ? mod : uint16_t{0}, down});
}

void setWorldClaims(std::vector<Claim> claims) {
   std::scoped_lock lock(g_mutex);
   g_world.claims = std::move(claims);
   g_world.aliveAt = GetTickCount64();
}

void clearWorldClaims() {
   std::scoped_lock lock(g_mutex);
   g_world.claims.clear();
}

void keepWorldAlive() {
   std::scoped_lock lock(g_mutex);
   g_world.aliveAt = GetTickCount64();
}

std::vector<KeyEvent> drainWorld() {
   std::scoped_lock lock(g_mutex);
   return std::exchange(g_world.queue, {});
}

} // namespace fa::input
