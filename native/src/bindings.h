#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

// The keys the player bound to a mod's control (a custom-input prototype), as the game keeps them
// in its control settings: so that the DLL answers a key wherever the player moved it.
namespace fa::bindings {

struct Key {
   uint32_t key = 0; // SDL keycode, as the key events carry it
   bool shift = false;
   bool ctrl = false;
   bool alt = false;
};

// The keyboard bindings of the custom input `name` ("fa-k"), up to two; empty when it has none or
// there is no game. Main thread.
std::vector<Key> customInput(std::string_view name);

} // namespace fa::bindings
