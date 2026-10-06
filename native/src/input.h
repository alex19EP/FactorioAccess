#pragma once

#include <cstdint>
#include <vector>

// Keyboard input taken from the game before it sees it (graph-a11y-spec P4). The game polls SDL
// for events; a key event matching the claim set is removed from that stream and queued for the
// navigator instead. Everything else, the mouse included, reaches the game untouched.
namespace fa::input {

// SDL3 keycodes for the keys the navigator uses.
namespace keys {
inline constexpr uint32_t Backspace = 0x08;
inline constexpr uint32_t Tab = 0x09;
inline constexpr uint32_t Return = 0x0d;
inline constexpr uint32_t Escape = 0x1b;
inline constexpr uint32_t Space = 0x20;
inline constexpr uint32_t F1 = 0x4000003a;
inline constexpr uint32_t Home = 0x4000004a;
inline constexpr uint32_t End = 0x4000004d;
inline constexpr uint32_t Right = 0x4000004f;
inline constexpr uint32_t Left = 0x40000050;
inline constexpr uint32_t Down = 0x40000051;
inline constexpr uint32_t Up = 0x40000052;
inline constexpr uint32_t KeypadEnter = 0x40000058;
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

} // namespace fa::input
