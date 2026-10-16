#include "luabridge.h"

#include "audio.h"
#include "cues.h"
#include "cursor.h"
#include "game.h"
#include "log.h"
#include "modfiles.h"
#include "movement.h"
#include "parts.h"
#include "railplanner.h"
#include "reader.h"
#include "scanner.h"
#include "selectedinfo.h"
#include "speech.h"
#include "world.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
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

void pushNumber(lua_State* L, double value) {
   reinterpret_cast<void (*)(lua_State*, lua_Number)>(layout.luaPushNumber)(L, value);
}
void pushBoolean(lua_State* L, bool value) {
   reinterpret_cast<void (*)(lua_State*, int)>(layout.luaPushBoolean)(L, value ? 1 : 0);
}
void getField(lua_State* L, int index, const char* name) {
   reinterpret_cast<void (*)(lua_State*, int, const char*)>(layout.luaGetField)(L, index, name);
}
void rawGetI(lua_State* L, int index, int n) {
   reinterpret_cast<void (*)(lua_State*, int, int)>(layout.luaRawGetI)(L, index, n);
}
int type(lua_State* L, int index) { return reinterpret_cast<int (*)(lua_State*, int)>(layout.luaType)(L, index); }
lua_Number toNumber(lua_State* L, int index) {
   return reinterpret_cast<lua_Number (*)(lua_State*, int, int*)>(layout.luaToNumberX)(L, index, nullptr);
}
bool toBoolean(lua_State* L, int index) {
   return reinterpret_cast<int (*)(lua_State*, int)>(layout.luaToBoolean)(L, index) != 0;
}
std::string toString(lua_State* L, int index) {
   size_t size = 0;
   const char* text = reinterpret_cast<const char* (*)(lua_State*, int, size_t*)>(layout.luaToLString)(L, index, &size);
   return text ? std::string(text, size) : std::string();
}
size_t rawLen(lua_State* L, int index) {
   return reinterpret_cast<size_t (*)(lua_State*, int)>(layout.luaRawLen)(L, index);
}
constexpr int kNil = 0;
constexpr int kBoolean = 1;
constexpr int kNumber = 3;
constexpr int kString = 4;
constexpr int kTable = 5;

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
   reinterpret_cast<void* (*)(void*, lua_State*, int, bool)>(layout.parseLocalisedString)(localised, L, index, true);
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
   static const std::regex tag(
      R"(\[/?(font|color|img|item|entity|technology|recipe|item-group|fluid|tile|)"
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
      if (i > 0 && next < text.size() && isSeparator(text[i - 1]) && isSeparator(text[next]) && !isSeparator(text[i]))
         out += keyName(L, std::string_view(text).substr(i, length));
      else
         out.append(text, i, length);
      i = next;
   }
   return out;
}

int speak(lua_State* L) {
   if (!world::mayBeLocalPlayer(static_cast<int>(checkInteger(L, 1)))) return 0;
   speech::say(nameKeys(L, stripRichText(translate(L, 2))));
   return 0;
}

// fa_native.cursor_mode(pindex, vanilla, keys, anchored, rulers, next_machine_sound), every tick, the
// rulers' tiles as a flat array x1, y1, x2, y2...: see cursor::setMode and cues::setNextMachine.
int cursorMode(lua_State* L) {
   std::vector<cursor::Tile> rulers;
   if (type(L, 5) == kTable) {
      const int top = getTop(L);
      const size_t count = rawLen(L, 5) / 2;
      rulers.reserve(count);
      for (size_t i = 0; i < count; ++i) {
         rawGetI(L, 5, static_cast<int>(2 * i + 1));
         rawGetI(L, 5, static_cast<int>(2 * i + 2));
         rulers.push_back(
            {static_cast<int>(std::floor(toNumber(L, -2))), static_cast<int>(std::floor(toNumber(L, -1)))});
         setTop(L, top);
      }
   }
   const int playerIndex = static_cast<int>(checkInteger(L, 1));
   cursor::setMode(playerIndex, toBoolean(L, 2), toBoolean(L, 3), toBoolean(L, 4), std::move(rulers));
   if (world::mayBeLocalPlayer(playerIndex)) cues::setNextMachine(toBoolean(L, 6));
   return 0;
}

// The LocalisedString at `index` as speech says it, or empty for nil.
std::string spoken(lua_State* L, int index) {
   return type(L, index) == kNil ? std::string() : nameKeys(L, stripRichText(translate(L, index)));
}

// fa_native.view_entered(pindex, hand): the player entered another view; `hand` is the LocalisedString
// the mod says for what is in hand, or nil. See cursor::viewEntered.
int viewEntered(lua_State* L) {
   const int playerIndex = static_cast<int>(checkInteger(L, 1));
   if (!world::mayBeLocalPlayer(playerIndex)) return 0;
   cursor::viewEntered(playerIndex, spoken(L, 2));
   return 0;
}

// fa_native.view_hand(pindex, hand): true when the view the player entered is still to be said and
// takes `hand` to say with it. For speech only: other clients say the hand themselves.
int viewHand(lua_State* L) {
   const int playerIndex = static_cast<int>(checkInteger(L, 1));
   pushBoolean(L, world::mayBeLocalPlayer(playerIndex) && cursor::viewHand(playerIndex, spoken(L, 2)));
   return 1;
}

// fa_native.cursor_jump(pindex, x, y): the mod's own cursor jumps.
int cursorJump(lua_State* L) {
   cursor::jump(static_cast<int>(checkInteger(L, 1)), checkNumber(L, 2), checkNumber(L, 3));
   return 0;
}

// fa_native.detach_build(pindex): the detach key with a building or blueprint in hand (see
// cursor::toggleDetach).
int detachBuild(lua_State* L) {
   cursor::toggleDetach(static_cast<int>(checkInteger(L, 1)));
   return 0;
}

// fa_native.cursor_step(pindex, x, y): "read", "moved", "edge" or "skipped" for a movement or skip
// key whose event carries the position, and for "skipped" how many tiles (-1: no change within the
// limit); nil when the cursor did not take that key. For "read", third, whether it went onto the
// next entity of a kind bigger than a tile. Only this client knows it.
int cursorStep(lua_State* L) {
   const auto step = cursor::step(static_cast<int>(checkInteger(L, 1)), checkNumber(L, 2), checkNumber(L, 3));
   if (!step) return 0;
   switch (step->step) {
   case cursor::Step::Read:
      pushString(L, "read");
      setTop(L, getTop(L) + 1); // no tile count
      pushBoolean(L, step->nextOfKind);
      return 3;
   case cursor::Step::Moved: pushString(L, "moved"); return 1;
   case cursor::Step::Edge: pushString(L, "edge"); return 1;
   case cursor::Step::Skipped:
      pushString(L, "skipped");
      pushInt(L, step->tiles);
      return 2;
   }
   return 0;
}

// fa_native.cursor_read(pindex, x, y, prefix): the tile reader reads the tile (x, y), after the
// LocalisedString `prefix` when one is given: for the mod's own jumps and announcements.
int cursorRead(lua_State* L) {
   if (!world::mayBeLocalPlayer(static_cast<int>(checkInteger(L, 1)))) return 0;
   const auto tile = [&](int index) { return static_cast<int32_t>(std::floor(checkNumber(L, index))); };
   std::string prefix;
   if (type(L, 4) != kNil) prefix = nameKeys(L, stripRichText(translate(L, 4)));
   reader::read(tile(2), tile(3), std::move(prefix));
   return 0;
}

using ParamEntity = const void* (*)(lua_State*, int index, const char* name, const void* fallback);

// fa_native.entity_read(pindex, entity, prefix, suffix): the tile reader reads the LuaEntity
// `entity` as on its own tile, between the LocalisedStrings `prefix` and `suffix` when they are
// given: an entity the mod names elsewhere than under the cursor, such as a scanner entry.
int entityRead(lua_State* L) {
   if (!world::mayBeLocalPlayer(static_cast<int>(checkInteger(L, 1)))) return 0;
   const void* entity = reinterpret_cast<ParamEntity>(layout.luaParamEntity)(L, 2, "entity", nullptr);
   // A MessageBuilder's words begin with the space before their first part; the reader puts its own
   // separators between them and the entity.
   const auto words = [&](int index) {
      if (type(L, index) == kNil) return std::string();
      std::string text = nameKeys(L, stripRichText(translate(L, index)));
      const size_t first = text.find_first_not_of(' ');
      if (first == std::string::npos) return std::string();
      return text.substr(first, text.find_last_not_of(' ') - first + 1);
   };
   reader::readEntity(entity, words(3), words(4));
   return 0;
}

// fa_native.cursor_position(pindex): the cursor's tile x and y, or nothing. Only this client knows
// it, so the mod may speak and play sounds by it but must not change the game by it.
int cursorPosition(lua_State* L) {
   const auto tile = cursor::position(static_cast<int>(checkInteger(L, 1)));
   if (!tile) return 0;
   pushInt(L, tile->x);
   pushInt(L, tile->y);
   return 2;
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
// {blueprint, direction, width, height, flippable, mirrored, flip_horizontal, flip_vertical}, with
// for a drill that drops items {output_x, output_y} and, when something takes them,
// {output_into, output_lane}; or nil. Only this client knows it, so the mod may speak it but must
// not change the game by it.
int heldBuild(lua_State* L) {
   const auto held = world::heldBuild(static_cast<int>(checkInteger(L, 1)));
   if (!held) return 0;
   createTable(L, 0, 12);
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
   if (held->output) {
      pushNumber(L, held->outputX);
      setField(L, -2, "output_x");
      pushNumber(L, held->outputY);
      setField(L, -2, "output_y");
   }
   if (!held->outputInto.empty()) {
      pushString(L, held->outputInto);
      setField(L, -2, "output_into");
   }
   if (held->outputLane >= 0) field("output_lane", held->outputLane);
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

// The overlays this client's map draws, as its map view options toggle them: a table with each
// overlay that is on set to true (logistic_network, electric_network, turret_range, pollution,
// station_names, player_names, tags, worker_robots, rail_signal_states, recipe_icons, pipelines);
// nil for another client's player. They are this client's config, so the mod may speak them but
// must not change the game by them.
int mapOverlays(lua_State* L) {
   if (!world::mayBeLocalPlayer(static_cast<int>(checkInteger(L, 1)))) return 0;
   const auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   const auto* settings =
      context ? *reinterpret_cast<const std::byte* const*>(context + layout.globalMapViewSettings) : nullptr;
   if (!settings) return 0;
   const auto on = [&](uint32_t item) {
      return *reinterpret_cast<const bool*>(settings + item + layout.configBoolValue);
   };
   // The game hides every overlay but these three while the nonstandard map info is off.
   const bool nonstandard = on(layout.mapViewNonstandardInfo);
   const struct {
      const char* name;
      uint32_t item;
      bool always;
   } overlays[] = {
      {"logistic_network", layout.mapViewLogisticNetwork, false},
      {"electric_network", layout.mapViewElectricNetwork, false},
      {"turret_range", layout.mapViewTurretRange, false},
      {"pollution", layout.mapViewPollution, false},
      {"station_names", layout.mapViewStationNames, true},
      {"player_names", layout.mapViewPlayerNames, true},
      {"tags", layout.mapViewTags, true},
      {"worker_robots", layout.mapViewWorkerRobots, false},
      {"rail_signal_states", layout.mapViewRailSignalStates, false},
      {"recipe_icons", layout.mapViewRecipeIcons, false},
      {"pipelines", layout.mapViewPipelines, false},
   };
   createTable(L, 0, static_cast<int>(std::size(overlays)));
   for (const auto& overlay : overlays) {
      if (!on(overlay.item) || !(overlay.always || nonstandard)) continue;
      pushBoolean(L, true);
      setField(L, -2, overlay.name);
   }
   return 1;
}

// The mod's sound files, read once each from its audio folder; a file that would not read stays
// null so it is not tried again.
std::shared_ptr<const std::string> soundFile(const std::string& name) {
   static std::unordered_map<std::string, std::shared_ptr<const std::string>> files;
   if (auto it = files.find(name); it != files.end()) return it->second;
   std::shared_ptr<const std::string> bytes;
   if (auto read = modfiles::read("__FactorioAccess__/audio/" + name)) {
      log::info("Audio: read audio/{}, {} bytes", name, read->size());
      bytes = std::make_shared<const std::string>(std::move(*read));
   } else
      log::error("Audio: cannot read the mod's audio/{}", name);
   files.emplace(name, bytes);
   return bytes;
}

// Reads the commands scripts/launcher-audio.lua builds. A malformed one is logged and skipped,
// never raised: only this client runs it, and a Lua error here would desync it.
class AudioReader {
public:
   explicit AudioReader(lua_State* L) : L(L) {}

   // The command table at `index`, flattened into `out`; false when it is malformed.
   bool command(int index, std::vector<audio::Command>& out) {
      if (type(L, index) != kTable) return fail("a command is not a table");
      const std::string kind = string(index, "command");
      if (kind == "stop") {
         out.push_back(audio::Stop{string(index, "id")});
         return true;
      }
      if (kind == "compound") {
         Field commands(L, index, "commands");
         if (type(L, commands.index) != kTable) return fail("a compound has no commands");
         const int count = static_cast<int>(rawLen(L, commands.index));
         for (int i = 1; i <= count; ++i) {
            Field element(L, commands.index, i);
            if (!command(element.index, out)) return false;
         }
         return true;
      }
      if (kind != "patch") return fail("unknown command " + kind);
      audio::Patch patch;
      patch.id = string(index, "id");
      if (patch.id.empty()) return fail("a patch has no id");
      if (!source(index, patch.source)) return false;
      if (!parameter(index, "volume", patch.volume) || !parameter(index, "pan", patch.pan) ||
          !parameter(index, "playback_rate", patch.playbackRate))
         return false;
      patch.looping = boolean(index, "looping");
      patch.startTime = number(index, "start_time").value_or(0.0);
      {
         Field lpf(L, index, "lpf");
         if (type(L, lpf.index) == kTable && (!present(lpf.index, "enabled") || boolean(lpf.index, "enabled"))) {
            patch.lpfCutoff = number(lpf.index, "cutoff").value_or(1000.0);
            patch.filterGain = audio::Parameter{1.0};
            if (!parameter(index, "filter_gain", *patch.filterGain)) return false;
         }
      }
      out.push_back(std::move(patch));
      return true;
   }

private:
   // Pushes t[name] (or t[n]) for the life of the object.
   struct Field {
      lua_State* L;
      int index;
      Field(lua_State* L, int table, const char* name) : L(L) {
         getField(L, table, name);
         index = getTop(L);
      }
      Field(lua_State* L, int table, int n) : L(L) {
         rawGetI(L, table, n);
         index = getTop(L);
      }
      ~Field() { setTop(L, index - 1); }
   };

   bool fail(const std::string& why) {
      log::error("fa_native.audio: {}", why);
      return false;
   }

   bool present(int table, const char* name) {
      Field field(L, table, name);
      return type(L, field.index) != kNil;
   }
   std::string string(int table, const char* name) {
      Field field(L, table, name);
      return type(L, field.index) == kString ? toString(L, field.index) : std::string();
   }
   std::optional<double> number(int table, const char* name) {
      Field field(L, table, name);
      if (type(L, field.index) != kNumber) return std::nullopt;
      return toNumber(L, field.index);
   }
   bool boolean(int table, const char* name) {
      Field field(L, table, name);
      return type(L, field.index) == kBoolean && toBoolean(L, field.index);
   }

   // A number, or a list of {time, value, interpolation_from_prev = "linear" | "jump"}; absent
   // leaves the default.
   bool parameter(int table, const char* name, audio::Parameter& out) {
      Field field(L, table, name);
      const int kind = type(L, field.index);
      if (kind == kNil) return true;
      if (kind == kNumber) {
         out = audio::Parameter{toNumber(L, field.index)};
         return true;
      }
      if (kind != kTable) return fail(std::string(name) + " is neither a number nor points");
      out = {};
      const int count = static_cast<int>(rawLen(L, field.index));
      for (int i = 1; i <= count; ++i) {
         Field point(L, field.index, i);
         if (type(L, point.index) != kTable) return fail(std::string(name) + " has a point that is not a table");
         const auto time = number(point.index, "time");
         const auto value = number(point.index, "value");
         if (!time || !value) return fail(std::string(name) + " has a point without time or value");
         out.points.push_back({*time, *value, string(point.index, "interpolation_from_prev") == "jump"});
      }
      if (out.points.empty()) return fail(std::string(name) + " has no points");
      std::ranges::stable_sort(out.points, {}, &audio::Parameter::Point::time);
      return true;
   }

   bool source(int table, audio::Source& out) {
      Field field(L, table, "source");
      if (type(L, field.index) != kTable) return fail("a patch has no source");
      const std::string kind = string(field.index, "kind");
      if (kind == "encoded_bytes") {
         out.name = string(field.index, "name");
         if (out.name.empty()) return fail("a sound file has no name");
         out.bytes = soundFile(out.name);
         return out.bytes != nullptr;
      }
      if (kind != "waveform") return fail("unknown source " + kind);
      const std::string wave = string(field.index, "waveform");
      if (wave == "sine")
         out.wave = audio::Wave::Sine;
      else if (wave == "square")
         out.wave = audio::Wave::Square;
      else if (wave == "triangle")
         out.wave = audio::Wave::Triangle;
      else if (wave == "saw" || wave == "sawtooth")
         out.wave = audio::Wave::Saw;
      else
         return fail("unknown waveform " + wave);
      const auto frequency = number(field.index, "frequency");
      if (!frequency) return fail("a tone has no frequency");
      out.name = wave;
      out.frequency = *frequency;
      out.duration = number(field.index, "non_looping_duration");
      out.fadeOut = number(field.index, "fade_out");
      return true;
   }

   lua_State* L;
};

// fa_native.audio(pindex, command): plays, retunes or stops the mod's sounds for this client's
// player (see audio.h). It changes nothing in the game.
int playAudio(lua_State* L) {
   if (!world::mayBeLocalPlayer(static_cast<int>(checkInteger(L, 1)))) return 0;
   std::vector<audio::Command> commands;
   if (AudioReader(L).command(2, commands)) audio::submit(std::move(commands));
   return 0;
}

// The number in field `name` of the table at `table`, if it holds one.
std::optional<double> numberField(lua_State* L, int table, const char* name) {
   getField(L, table, name);
   std::optional<double> value;
   if (type(L, -1) == kNumber) value = toNumber(L, -1);
   setTop(L, -2);
   return value;
}

// fa_native.scanner_refresh(pindex, {surface, x, y, direction, extras, generation, automatic,
// keep_origin}): lists for this client's player what the scanner finds on that surface where the
// player's force charted it, nearest x, y first, and only in `direction` (an 8-way
// defines.direction) when given; extras is an array of {category, key, x, y} the mod lists itself
// (scanner::Extra), from its collection `generation`. With `automatic`, over the next ticks (see
// scanner::Refresh). The list is this client's only; nothing in the game depends on it.
int scannerRefresh(lua_State* L) {
   scanner::Refresh request;
   request.playerIndex = static_cast<int>(checkInteger(L, 1));
   if (type(L, 2) != kTable) return 0;
   auto surface = numberField(L, 2, "surface");
   auto x = numberField(L, 2, "x");
   auto y = numberField(L, 2, "y");
   if (!surface || !x || !y) return 0;
   request.surfaceIndex = static_cast<uint32_t>(*surface);
   request.x = *x;
   request.y = *y;
   if (auto direction = numberField(L, 2, "direction")) request.direction = static_cast<int>(*direction);
   request.generation = static_cast<uint32_t>(numberField(L, 2, "generation").value_or(0));
   getField(L, 2, "automatic");
   request.automatic = toBoolean(L, -1);
   getField(L, 2, "keep_origin");
   request.keepOrigin = toBoolean(L, -1);
   setTop(L, -3);
   getField(L, 2, "extras");
   if (type(L, -1) == kTable) {
      const int extras = getTop(L);
      const size_t count = rawLen(L, extras);
      for (size_t i = 1; i <= count; ++i) {
         rawGetI(L, extras, static_cast<int>(i));
         const int entry = getTop(L);
         scanner::Extra extra;
         getField(L, entry, "category");
         extra.category = toString(L, -1);
         getField(L, entry, "key");
         extra.key = toString(L, -1);
         setTop(L, entry);
         extra.x = numberField(L, entry, "x").value_or(0);
         extra.y = numberField(L, entry, "y").value_or(0);
         request.extras.push_back(std::move(extra));
         setTop(L, extras);
      }
   }
   setTop(L, -2);
   scanner::refresh(request);
   return 0;
}

// fa_native.scanner_tick(pindex, surface): each tick, carries this client's automatic scanner
// refresh along; true when the mod is to start one now with scanner_refresh. For this client only:
// the mod must change nothing in the game by it.
int scannerTick(lua_State* L) {
   pushBoolean(L, scanner::tick(static_cast<int>(checkInteger(L, 1)), static_cast<uint32_t>(checkInteger(L, 2))));
   return 1;
}

// fa_native.scanner_entry(pindex, x, y): what the scanner key whose event carried cursor_position
// x, y moved onto, as {category, edge, empty, index, count, kind, alert, targeted, extra, prototype,
// text, trees, width, height, x, y, origin_x, origin_y} (see scanner::Entry), or nil. For speech only:
// other clients have no scanner.
int scannerEntry(lua_State* L) {
   const auto entry = scanner::entryAt(static_cast<int>(checkInteger(L, 1)), checkNumber(L, 2), checkNumber(L, 3));
   if (!entry) return 0;
   createTable(L, 0, 18);
   pushString(L, entry->category);
   setField(L, -2, "category");
   pushBoolean(L, entry->edge);
   setField(L, -2, "edge");
   pushBoolean(L, entry->empty);
   setField(L, -2, "empty");
   if (!entry->empty) {
      pushNumber(L, entry->index);
      setField(L, -2, "index");
      pushNumber(L, entry->count);
      setField(L, -2, "count");
      pushString(L, entry->kind);
      setField(L, -2, "kind");
      pushString(L, entry->alert);
      setField(L, -2, "alert");
      pushBoolean(L, entry->targeted);
      setField(L, -2, "targeted");
      pushNumber(L, entry->extra);
      setField(L, -2, "extra");
      pushNumber(L, entry->generation);
      setField(L, -2, "generation");
      pushString(L, entry->prototype);
      setField(L, -2, "prototype");
      pushString(L, entry->text);
      setField(L, -2, "text");
      pushNumber(L, entry->trees);
      setField(L, -2, "trees");
      pushNumber(L, entry->width);
      setField(L, -2, "width");
      pushNumber(L, entry->height);
      setField(L, -2, "height");
      pushNumber(L, entry->x);
      setField(L, -2, "x");
      pushNumber(L, entry->y);
      setField(L, -2, "y");
      pushNumber(L, entry->originX);
      setField(L, -2, "origin_x");
      pushNumber(L, entry->originY);
      setField(L, -2, "origin_y");
   }
   return 1;
}

// fa_native.scanner_category(pindex): the category the last category key moved to, as
// {category, edge}, or nil. For speech only.
int scannerCategory(lua_State* L) {
   const auto category = scanner::category(static_cast<int>(checkInteger(L, 1)));
   if (!category) return 0;
   createTable(L, 0, 2);
   pushString(L, category->category);
   setField(L, -2, "category");
   pushBoolean(L, category->edge);
   setField(L, -2, "edge");
   return 1;
}

// fa_native.scanner_current_category(pindex): the category the scanner is in now, by its key
// ("pins"), or nil before the first scanner key. For sounds and speech only.
int scannerCurrentCategory(lua_State* L) {
   const auto category = scanner::currentCategory(static_cast<int>(checkInteger(L, 1)));
   if (!category) return 0;
   pushString(L, *category);
   return 1;
}

void pushRoutePoint(lua_State* L, const railplanner::RoutePoint& point) {
   pushNumber(L, static_cast<double>(point.x) / game::kMapPositionScale);
   setField(L, -2, "x");
   pushNumber(L, static_cast<double>(point.y) / game::kMapPositionScale);
   setField(L, -2, "y");
   pushInt(L, point.direction);
   setField(L, -2, "direction");
   pushBoolean(L, point.elevated);
   setField(L, -2, "elevated");
}

// fa_native.rail_route(pindex): the route the rail planner draws while this client plans, as
// {start = point, pieces = {{type = prototype type, x, y, direction, elevated}...}}, each point the
// end a rail leads on from, in tiles; nil when it does not plan. For speech only.
int railRoute(lua_State* L) {
   if (!world::mayBeLocalPlayer(static_cast<int>(checkInteger(L, 1)))) return 0;
   const auto route = railplanner::route();
   if (!route) return 0;
   createTable(L, 0, 2);
   createTable(L, 0, 4);
   pushRoutePoint(L, route->start);
   setField(L, -2, "start");
   createTable(L, static_cast<int>(route->pieces.size()), 0);
   for (size_t i = 0; i < route->pieces.size(); ++i) {
      createTable(L, 0, 5);
      pushString(L, route->pieces[i].type);
      setField(L, -2, "type");
      pushRoutePoint(L, route->pieces[i].end);
      rawSetI(L, -2, static_cast<int>(i + 1));
   }
   setField(L, -2, "pieces");
   return 1;
}

struct Function {
   const char* name;
   lua_CFunction function;
};

constexpr Function kFunctions[] = {
   {"cursor_mode", &cursorMode},
   {"cursor_jump", &cursorJump},
   {"view_entered", &viewEntered},
   {"view_hand", &viewHand},
   {"detach_build", &detachBuild},
   {"cursor_step", &cursorStep},
   {"cursor_read", &cursorRead},
   {"entity_read", &entityRead},
   {"cursor_position", &cursorPosition},
   {"speak", &speak},
   {"next_part", &nextPart},
   {"build_direction", &buildDirection},
   {"held_build", &heldBuild},
   {"drag_build", &dragBuild},
   {"walking_step", &walkingStep},
   {"open_selected_info", &openSelectedInfo},
   {"map_overlays", &mapOverlays},
   {"audio", &playAudio},
   {"scanner_refresh", &scannerRefresh},
   {"scanner_tick", &scannerTick},
   {"scanner_entry", &scannerEntry},
   {"scanner_category", &scannerCategory},
   {"scanner_current_category", &scannerCurrentCategory},
   {"rail_route", &railRoute},
};

using InitLuaState = void (*)(lua_State*);
InitLuaState g_original = nullptr;

// What Lua gets for each of kFunctions: the stub's function, which outlives this core.
lua_CFunction g_bound[std::size(kFunctions)] = {};

void detour(lua_State* L) {
   g_original(L);
   createTable(L, 0, static_cast<int>(std::size(kFunctions)));
   for (size_t i = 0; i < std::size(kFunctions); ++i) {
      if (!g_bound[i]) continue;
      pushCClosure(L, g_bound[i], 0);
      setField(L, -2, kFunctions[i].name);
   }
   setGlobal(L, "fa_native");
   log::info("fa_native added to a Lua state");
   // A game loading: the sounds of the one before have no script left to stop them.
   audio::submit({audio::StopAll{}});
}

} // namespace

void bind(stub::LuaFunction (*bindLua)(const char* name, stub::LuaFunction target)) {
   for (size_t i = 0; i < std::size(kFunctions); ++i) {
      // The stub's lua_State is this file's: both are the game's, declared by name only.
      auto target = reinterpret_cast<stub::LuaFunction>(kFunctions[i].function);
      g_bound[i] = reinterpret_cast<lua_CFunction>(bindLua(kFunctions[i].name, target));
      if (!g_bound[i]) log::error("fa_native.{} has no Lua slot in the stub", kFunctions[i].name);
   }
}

void* initLuaStateDetour() { return reinterpret_cast<void*>(&detour); }

void** initLuaStateOriginal() { return reinterpret_cast<void**>(&g_original); }

} // namespace fa::luabridge
