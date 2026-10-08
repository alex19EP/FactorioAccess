#include "luabridge.h"

#include "game.h"
#include "log.h"
#include "movement.h"
#include "parts.h"
#include "selectedinfo.h"
#include "speech.h"
#include "world.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <regex>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace fa::luabridge {

namespace {

using game::layout;

struct lua_State;
// Factorio's Lua 5.2: lua_Integer is ptrdiff_t and lua_Number double.
using lua_Integer = ptrdiff_t;
using lua_Number = double;
using lua_CFunction = int (*)(lua_State*);

void createTable(lua_State* L, int arrays, int records) {
   reinterpret_cast<void (*)(lua_State*, int, int)>(layout.luaCreateTable)(L, arrays, records);
}
void pushCClosure(lua_State* L, lua_CFunction function, int upvalues) {
   reinterpret_cast<void (*)(lua_State*, lua_CFunction, int)>(layout.luaPushCClosure)(L, function, upvalues);
}
void setField(lua_State* L, int index, const char* name) {
   reinterpret_cast<void (*)(lua_State*, int, const char*)>(layout.luaSetField)(L, index, name);
}
void setGlobal(lua_State* L, const char* name) {
   reinterpret_cast<void (*)(lua_State*, const char*)>(layout.luaSetGlobal)(L, name);
}
// The check functions raise a Lua error on a bad argument, which leaves through our frame; the
// functions below hold nothing that needs destroying.
lua_Integer checkInteger(lua_State* L, int argument) {
   return reinterpret_cast<lua_Integer (*)(lua_State*, int)>(layout.luaCheckInteger)(L, argument);
}
lua_Number checkNumber(lua_State* L, int argument) {
   return reinterpret_cast<lua_Number (*)(lua_State*, int)>(layout.luaCheckNumber)(L, argument);
}
int getTop(lua_State* L) { return reinterpret_cast<int (*)(lua_State*)>(layout.luaGetTop)(L); }
void setTop(lua_State* L, int index) { reinterpret_cast<void (*)(lua_State*, int)>(layout.luaSetTop)(L, index); }
void pushString(lua_State* L, std::string_view text) {
   reinterpret_cast<const char* (*)(lua_State*, const char*, size_t)>(layout.luaPushLString)(L, text.data(),
                                                                                             text.size());
}
void rawSetI(lua_State* L, int index, int n) {
   reinterpret_cast<void (*)(lua_State*, int, int)>(layout.luaRawSetI)(L, index, n);
}
void pushByte(lua_State* L, uint8_t value) {
   reinterpret_cast<void (*)(lua_State*, uint8_t)>(layout.luaPushByte)(L, value);
}
void pushInt(lua_State* L, int value) { reinterpret_cast<void (*)(lua_State*, int)>(layout.luaPushInt)(L, value); }
void pushBoolean(lua_State* L, bool value) {
   reinterpret_cast<void (*)(lua_State*, int)>(layout.luaPushBoolean)(L, value ? 1 : 0);
}

struct MsvcString {
   union {
      char buffer[16];
      const char* pointer;
   };
   size_t size;
   size_t capacity;
};

// Translates the LocalisedString at `index` the way localised_print does, in the game's current
// locale. A malformed string raises a Lua error before anything here needs destroying.
std::string translate(lua_State* L, int index) {
   alignas(8) std::byte localised[256];
   if (layout.localisedStringSize > sizeof(localised)) {
      log::error("LocalisedString is {} bytes, more than the {} we hold", layout.localisedStringSize,
                 sizeof(localised));
      return {};
   }
   reinterpret_cast<void* (*)(void*, lua_State*, int, bool)>(layout.parseLocalisedString)(localised, L, index,
                                                                                          true);
   struct Destroy {
      void* object;
      ~Destroy() { reinterpret_cast<void (*)(void*)>(layout.localisedStringDestroy)(object); }
   } destroy{localised};
   using Str = const MsvcString* (*)(const void* localisedString, const void* localeProvider);
   const MsvcString* text = reinterpret_cast<Str>(layout.localisedStringStr)(localised, nullptr);
   return {text->capacity >= sizeof(text->buffer) ? text->pointer : text->buffer, text->size};
}

// Rich text tags the screen reader would read out as markup.
std::string stripRichText(const std::string& text) {
   static const std::regex tag(R"(\[/?(font|color|img|item|entity|technology|recipe|item-group|fluid|tile|)"
                               R"(virtual-signal|achievement|gps|special-item|armor|train|train-stop|tooltip)[^\]]*\])");
   return std::regex_replace(text, tag, "");
}

bool isSeparator(char c) { return c == '"' || c == ' ' || (c >= '\t' && c <= '\r'); }

// The name of a key the text mentions as a lone character, such as "[" in "press [": the mod's
// control-keys locale names the ones a screen reader would skip or misread.
std::string keyName(lua_State* L, std::string_view key) {
   const int top = getTop(L);
   createTable(L, 3, 0); // {"?", {"control-keys.<key>"}, "<key>"}
   pushString(L, "?");
   rawSetI(L, -2, 1);
   createTable(L, 1, 0);
   // A locale key cannot be "[", which opens a section.
   pushString(L, "control-keys." + std::string(key == "[" ? "left-bracket" : key));
   rawSetI(L, -2, 1);
   rawSetI(L, -2, 2);
   pushString(L, key);
   rawSetI(L, -2, 3);
   std::string name = translate(L, -1);
   setTop(L, top);
   return name;
}

std::string nameKeys(lua_State* L, const std::string& text) {
   std::string out;
   out.reserve(text.size());
   size_t i = 0;
   while (i < text.size()) {
      // One UTF-8 character.
      const auto lead = static_cast<unsigned char>(text[i]);
      size_t length = lead < 0x80 ? 1 : lead >= 0xf0 ? 4 : lead >= 0xe0 ? 3 : lead >= 0xc0 ? 2 : 1;
      length = std::min(length, text.size() - i);
      const size_t next = i + length;
      if (i > 0 && next < text.size() && isSeparator(text[i - 1]) && isSeparator(text[next]) &&
          !isSeparator(text[i]))
         out += keyName(L, std::string_view(text).substr(i, length));
      else
         out.append(text, i, length);
      i = next;
   }
   return out;
}

int speak(lua_State* L) {
   if (!world::mayBeLocalPlayer(static_cast<int>(checkInteger(L, 1)))) return 0;
   speech::say(nameKeys(L, stripRichText(translate(L, 2))), true);
   return 0;
}

int setCursor(lua_State* L) {
   world::setCursor(static_cast<int>(checkInteger(L, 1)), checkNumber(L, 2), checkNumber(L, 3));
   return 0;
}

int releaseCursor(lua_State* L) {
   world::releaseCursor(static_cast<int>(checkInteger(L, 1)));
   return 0;
}

// The game's build direction for the item in hand, or nil for another client's player. Only this
// client knows it, so the mod may speak it but must not change the game by it.
int buildDirection(lua_State* L) {
   const int direction = world::buildDirection(static_cast<int>(checkInteger(L, 1)));
   if (direction < 0) return 0;
   pushByte(L, static_cast<uint8_t>(direction));
   return 1;
}

// The entity or blueprint in hand as this client builds it (see world::HeldBuild), as a table
// {blueprint, direction, width, height, flippable, mirrored, flip_horizontal, flip_vertical}, or
// nil. Only this client knows it, so the mod may speak it but must not change the game by it.
int heldBuild(lua_State* L) {
   const auto held = world::heldBuild(static_cast<int>(checkInteger(L, 1)));
   if (!held) return 0;
   createTable(L, 0, 8);
   const auto field = [&](const char* name, auto value) {
      if constexpr (std::is_same_v<decltype(value), bool>)
         pushBoolean(L, value);
      else
         pushInt(L, value);
      setField(L, -2, name);
   };
   field("blueprint", held->blueprint);
   field("direction", held->direction);
   field("width", held->width);
   field("height", held->height);
   field("flippable", held->flippable);
   field("mirrored", held->mirrored);
   field("flip_horizontal", held->flipHorizontal);
   field("flip_vertical", held->flipVertical);
   return 1;
}

// The game's drag building while the build control is held (see world::DragBuild), as a table
// {turn_pending, turns}, or nil. Only this client knows it, so the mod may speak it but must not
// change the game by it.
int dragBuild(lua_State* L) {
   const auto drag = world::dragBuild(static_cast<int>(checkInteger(L, 1)));
   if (!drag) return 0;
   createTable(L, 0, 2);
   pushBoolean(L, drag->turnPending);
   setField(L, -2, "turn_pending");
   pushInt(L, drag->turns);
   setField(L, -2, "turns");
   return 1;
}

// How this client's character's walking went this tick or the last: "full", "partial" (slid along
// something) or "none" (blocked), then a step count that wraps at 256; nothing for another
// client's player or when it did not walk. The mod may play sounds by it but must not change the
// game by it.
int walkingStep(lua_State* L) {
   movement::Step step;
   uint8_t count = 0;
   if (!movement::recentStep(static_cast<int>(checkInteger(L, 1)), step, count)) return 0;
   pushString(L, step == movement::Step::Full ? "full" : step == movement::Step::Partial ? "partial" : "none");
   pushByte(L, count);
   return 2;
}

// Ctrl+Tab in the world, which the mod hands over when none of its own menus takes it.
int nextPart(lua_State* L) {
   if (!world::mayBeLocalPlayer(static_cast<int>(checkInteger(L, 1)))) return 0;
   parts::cycle(static_cast<int>(checkInteger(L, 2)));
   return 0;
}

// The Y key in the world: the game's info panel for what the cursor points at (see selectedinfo.h).
int openSelectedInfo(lua_State* L) {
   if (!world::mayBeLocalPlayer(static_cast<int>(checkInteger(L, 1)))) return 0;
   selectedinfo::request();
   return 0;
}

struct Function {
   const char* name;
   lua_CFunction function;
};

constexpr Function kFunctions[] = {
   {"set_cursor", &setCursor},
   {"release_cursor", &releaseCursor},
   {"speak", &speak},
   {"next_part", &nextPart},
   {"build_direction", &buildDirection},
   {"held_build", &heldBuild},
   {"drag_build", &dragBuild},
   {"walking_step", &walkingStep},
   {"open_selected_info", &openSelectedInfo},
};

using InitLuaState = void (*)(lua_State*);
InitLuaState g_original = nullptr;

void detour(lua_State* L) {
   g_original(L);
   createTable(L, 0, static_cast<int>(std::size(kFunctions)));
   for (const Function& entry : kFunctions) {
      pushCClosure(L, entry.function, 0);
      setField(L, -2, entry.name);
   }
   setGlobal(L, "fa_native");
   log::info("fa_native added to a Lua state");
}

} // namespace

void* initLuaStateDetour() { return reinterpret_cast<void*>(&detour); }

void** initLuaStateOriginal() { return reinterpret_cast<void**>(&g_original); }

} // namespace fa::luabridge
