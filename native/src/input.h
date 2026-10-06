#pragma once

#include <cstdint>
#include <vector>

// Keyboard input taken from the game before it sees it (graph-a11y-spec P4). The game polls SDL
// for events; a key event matching a claim set is removed from that stream and queued for the
// navigator or the world keys instead. Everything else, the mouse included, reaches the game
// untouched.
namespace fa::input {

// SDL3 keycodes for the keys we claim.
namespace keys {
inline constexpr uint32_t Backspace = 0x08;
inline constexpr uint32_t Tab = 0x09;
inline constexpr uint32_t Return = 0x0d;
inline constexpr uint32_t Escape = 0x1b;
inline constexpr uint32_t Space = 0x20;
inline constexpr uint32_t LeftBracket = 0x5b;
inline constexpr uint32_t Backslash = 0x5c;
inline constexpr uint32_t RightBracket = 0x5d;
inline constexpr uint32_t F1 = 0x4000003a;
inline constexpr uint32_t Delete = 0x7f;
inline constexpr uint32_t Home = 0x4000004a;
inline constexpr uint32_t PageUp = 0x4000004b;
inline constexpr uint32_t End = 0x4000004d;
inline constexpr uint32_t PageDown = 0x4000004e;
inline constexpr uint32_t Right = 0x4000004f;
inline constexpr uint32_t Left = 0x40000050;
inline constexpr uint32_t Down = 0x40000051;
inline constexpr uint32_t Up = 0x40000052;
inline constexpr uint32_t KeypadEnter = 0x40000058;
inline constexpr uint32_t LeftCtrl = 0x400000e0;
inline constexpr uint32_t LeftShift = 0x400000e1;
inline constexpr uint32_t LeftAlt = 0x400000e2;
} // namespace keys

// Which modifier states a claim covers. A key event has exactly one of them: Alt wins over
// Control, which wins over Shift.
namespace mods {
inline constexpr uint8_t None = 1;
inline constexpr uint8_t Shift = 2;
inline constexpr uint8_t Ctrl = 4;
inline constexpr uint8_t Alt = 8;
} // namespace mods

struct Claim {
   uint32_t key;
   uint8_t mods;
};

struct KeyEvent {
   uint32_t key; // SDL keycode
   bool down;
   bool repeat;
   bool shift;
   bool ctrl;
   bool alt;
};

// The SDL_PollEvent_REAL replacement and where MinHook stores the original.
void* pollEventDetour();
void** pollEventOriginal();

// Replaces the claim set. Claims lapse on their own unless renewed with keepAlive() at least every
// few hundred milliseconds, so a navigator that stops ticking (a Gui that is gone, a crash in our
// code) can never keep keys away from the game.
void setClaims(std::vector<Claim> claims);
void clearClaims();
void keepAlive();

// The queued key events, oldest first.
std::vector<KeyEvent> drain();

// Queues a press and release into the game's own event stream, where they are routed exactly like
// typed keys: to the navigator while it claims the key, to the game otherwise. For the dev server.
void injectKey(uint32_t key, bool shift, bool ctrl, bool alt);

// Queues the left modifier keys going down or up, as separate key events: the game reads a held
// modifier from its own key state, not from the flags on the key it modifies.
void injectModifiers(bool shift, bool ctrl, bool alt, bool down);

// A second claim set and queue, for the keys that stand in for mouse buttons in the game world.
// It only takes what the navigator's claims leave, and lapses the same way.
void setWorldClaims(std::vector<Claim> claims);
void clearWorldClaims();
void keepWorldAlive();
std::vector<KeyEvent> drainWorld();

} // namespace fa::input
