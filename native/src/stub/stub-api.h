#pragma once

// What winmm.dll (the stub, src/stub/stub.cpp) and fa_native.dll (the core) hand each other. The
// stub stays loaded for the whole run, because the game imports winmm; the core can be unloaded and
// loaded again while the game runs. So nothing the game keeps may point into the core: Lua holds the
// stub's functions, and they call the core's through bindLua.

#include <cstdint>

struct lua_State;

namespace fa::stub {

using LuaFunction = int (*)(lua_State*);

inline constexpr uint32_t kApiVersion = 1;

struct Api {
   uint32_t version;
   // The game's bin/x64 directory, where the stub, the core and their files live.
   const wchar_t* directory;
   // 0 for the core loaded as the game starts, then one more for each reload.
   uint32_t generation;
   // The function Lua is given for `name`. It calls `target` while this core is loaded, waits while
   // a reload is under way, and returns no values while no core has it.
   LuaFunction (*bindLua)(const char* name, LuaFunction target);
};

} // namespace fa::stub

// The core's exports. fa_core_start returns false when the core stood down (an unsupported game
// version, a hook that failed); fa_core_stop is then still called before unloading.
extern "C" {
using FaCoreStart = bool (*)(const fa::stub::Api* api);
using FaCoreStop = void (*)();
}
