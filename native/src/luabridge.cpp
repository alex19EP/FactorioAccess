#include "luabridge.h"

#include "audio.h"
#include "entityicons.h"
#include "game.h"
#include "log.h"
#include "modfiles.h"
#include "movement.h"
#include "parts.h"
#include "scanner.h"
#include "selectedinfo.h"
#include "speech.h"
#include "world.h"

#include <algorithm>
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

using ParamEntity = const void* (*)(lua_State*, int index, const char* name, const void* fallback);

// What an entity shows on the map, as the game draws it now (see entityicons.h): a table {status =
// {utility sprite names}, icons = {{kind, name, quality, comparison, any_quality, denied}}}, kind
// absent for a utility sprite; nil for another client's player. Only this client draws, so the mod
// may speak it but must not change the game by it.
int entityIcons(lua_State* L) {
   if (!world::mayBeLocalPlayer(static_cast<int>(checkInteger(L, 1)))) return 0;
   const void* entity = reinterpret_cast<ParamEntity>(layout.luaParamEntity)(L, 2, "entity", nullptr);
   if (!entity) return 0;
   const auto drawn = entityicons::read(entity);
   if (!drawn) return 0;
   createTable(L, 0, 2);
   createTable(L, static_cast<int>(drawn->status.size()), 0);
   for (size_t i = 0; i < drawn->status.size(); ++i) {
      pushString(L, drawn->status[i]);
      rawSetI(L, -2, static_cast<int>(i + 1));
   }
   setField(L, -2, "status");
   createTable(L, static_cast<int>(drawn->icons.size()), 0);
   for (size_t i = 0; i < drawn->icons.size(); ++i) {
      const entityicons::Icon& icon = drawn->icons[i];
      createTable(L, 0, 6);
      const auto text = [&](const char* field, std::string_view value) {
         if (value.empty()) return;
         pushString(L, value);
         setField(L, -2, field);
      };
      const auto flag = [&](const char* field, bool value) {
         if (!value) return;
         pushBoolean(L, true);
         setField(L, -2, field);
      };
      text("kind", icon.kind);
      text("name", icon.name);
      text("quality", icon.quality);
      text("comparison", icon.comparison);
      flag("any_quality", icon.anyQuality);
      flag("denied", icon.denied);
      rawSetI(L, -2, static_cast<int>(i + 1));
   }
   setField(L, -2, "icons");
   return 1;
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

void pushNumber(lua_State* L, double value) {
   reinterpret_cast<void (*)(lua_State*, lua_Number)>(layout.luaPushNumber)(L, value);
}

// The number in field `name` of the table at `table`, if it holds one.
std::optional<double> numberField(lua_State* L, int table, const char* name) {
   getField(L, table, name);
   std::optional<double> value;
   if (type(L, -1) == kNumber) value = toNumber(L, -1);
   setTop(L, -2);
   return value;
}

// The strings of the array in field `name` of the table at `table`.
std::vector<std::string> stringsField(lua_State* L, int table, const char* name) {
   std::vector<std::string> strings;
   getField(L, table, name);
   if (type(L, -1) == kTable) {
      const size_t count = rawLen(L, -1);
      for (size_t i = 1; i <= count; ++i) {
         rawGetI(L, -1, static_cast<int>(i));
         if (type(L, -1) == kString) strings.push_back(toString(L, -1));
         setTop(L, -2);
      }
   }
   setTop(L, -2);
   return strings;
}

// fa_native.scanner_refresh(pindex, {surface, x, y, radius, direction, water, ice, extras}): lists
// for this client's player what the scanner finds on that surface within `radius` of x, y, where
// the player's force charted it, and only in `direction` (an 8-way defines.direction) when given;
// water and ice are arrays of the tile names that make bodies of water and of ice; extras is an
// array of {category, key, x, y} the mod lists itself (scanner::Extra). Returns the entities
// whose subcategories the mod is to give, as an array of {name, x, y}, or nil when it listed
// nothing. The list is this client's only; nothing in the game depends on it.
int scannerRefresh(lua_State* L) {
   scanner::Refresh request;
   request.playerIndex = static_cast<int>(checkInteger(L, 1));
   if (type(L, 2) != kTable) return 0;
   auto surface = numberField(L, 2, "surface");
   auto x = numberField(L, 2, "x");
   auto y = numberField(L, 2, "y");
   auto radius = numberField(L, 2, "radius");
   if (!surface || !x || !y || !radius) return 0;
   request.surfaceIndex = static_cast<uint32_t>(*surface);
   request.x = *x;
   request.y = *y;
   request.radius = *radius;
   if (auto direction = numberField(L, 2, "direction")) request.direction = static_cast<int>(*direction);
   request.water = stringsField(L, 2, "water");
   request.ice = stringsField(L, 2, "ice");
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
   const auto details = scanner::refresh(request);
   if (!details) return 0;
   createTable(L, static_cast<int>(details->size()), 0);
   for (size_t i = 0; i < details->size(); ++i) {
      const scanner::Detail& detail = (*details)[i];
      createTable(L, 0, 3);
      pushString(L, detail.prototype);
      setField(L, -2, "name");
      pushNumber(L, detail.x);
      setField(L, -2, "x");
      pushNumber(L, detail.y);
      setField(L, -2, "y");
      rawSetI(L, -2, static_cast<int>(i + 1));
   }
   return 1;
}

// fa_native.scanner_subcategories(pindex, keys): the subcategories of the entities the last refresh
// returned, in its order: a string each, or false to keep the entity under its prototype.
int scannerSubcategories(lua_State* L) {
   const int playerIndex = static_cast<int>(checkInteger(L, 1));
   if (type(L, 2) != kTable) return 0;
   std::vector<std::optional<std::string>> keys(rawLen(L, 2));
   for (size_t i = 0; i < keys.size(); ++i) {
      rawGetI(L, 2, static_cast<int>(i + 1));
      if (type(L, -1) == kString) keys[i] = toString(L, -1);
      setTop(L, -2);
   }
   scanner::setSubcategories(playerIndex, keys);
   return 0;
}

// fa_native.scanner_mod_ui(pindex, open): whether one of the mod's own UIs is open, and so has the
// scanner keys.
int scannerModUi(lua_State* L) {
   scanner::setModUiOpen(static_cast<int>(checkInteger(L, 1)), toBoolean(L, 2));
   return 0;
}

// fa_native.scanner_entry(pindex, x, y): what the scanner key whose event carried cursor_position
// x, y moved onto, as {category, edge, empty, index, count, kind, extra, prototype, text, trees,
// width, height, x, y, origin_x, origin_y} (see scanner::Entry), or nil. For speech only: other clients
// have no scanner.
int scannerEntry(lua_State* L) {
   const auto entry = scanner::entryAt(static_cast<int>(checkInteger(L, 1)), checkNumber(L, 2), checkNumber(L, 3));
   if (!entry) return 0;
   createTable(L, 0, 15);
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
      pushNumber(L, entry->extra);
      setField(L, -2, "extra");
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
   {"entity_icons", &entityIcons},
   {"map_overlays", &mapOverlays},
   {"audio", &playAudio},
   {"scanner_refresh", &scannerRefresh},
   {"scanner_subcategories", &scannerSubcategories},
   {"scanner_mod_ui", &scannerModUi},
   {"scanner_entry", &scannerEntry},
   {"scanner_category", &scannerCategory},
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
   // A game loading: the sounds of the one before have no script left to stop them.
   audio::submit({audio::StopAll{}});
}

} // namespace

void* initLuaStateDetour() { return reinterpret_cast<void*>(&detour); }

void** initLuaStateOriginal() { return reinterpret_cast<void**>(&g_original); }

} // namespace fa::luabridge
