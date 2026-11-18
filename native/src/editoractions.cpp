#include "editoractions.h"

#include "cursor.h"
#include "game.h"
#include "reader.h"
#include "speech.h"
#include "stdabi.h"
#include "text.h"
#include "vocab.h"

#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fa::editoractions {

namespace {

using game::layout;

template <class T>
T at(const void* object, uint32_t offset) {
   T value;
   std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
   return value;
}

// Frees what a std::string the game returned holds, as its own destructor does: through the game's
// allocator.
void destroy(stdabi::String& string) {
   if (void* block = string.allocation()) reinterpret_cast<void (*)(void*)>(layout.gameOperatorDelete)(block);
}

const std::byte* localPlayer() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   const auto* game = context ? at<const std::byte*>(context, layout.globalGame) : nullptr;
   return game ? at<const std::byte*>(game, layout.gameLocalPlayer) : nullptr;
}

// The speed as the Time editor's label shows it: GameSpeed::str(), then " (paused)" while the map
// is paused.
std::string speedLine(const std::byte* map) {
   const auto* speed = at<const std::byte*>(map, layout.mapGameSpeed);
   const auto* paused = at<const std::byte*>(map, layout.mapPausedState);
   if (!speed || !paused) return {};
   stdabi::String shown{};
   reinterpret_cast<stdabi::String* (*)(const void*, stdabi::String*)>(layout.gameSpeedStr)(speed, &shown);
   std::string line(shown.view());
   destroy(shown);
   if (at<bool>(paused, layout.pausedSetting)) line += " " + std::string(vocab::kEditorPaused);
   return text::speakable(line);
}

// The surface named `name` as the Surfaces editor lists it: "nauvis (Nauvis)", with the planet's
// icon, or the name alone when the display name is the same.
std::string surfaceLine(const std::byte* map, std::string_view name) {
   const auto surfaces = at<stdabi::Vector<const std::byte* const>>(map, layout.mapSurfaces);
   for (const std::byte* const* surface = surfaces.begin(); surface != surfaces.end(); ++surface) {
      if (!*surface || at<stdabi::String>(*surface, layout.surfaceName).view() != name) continue;
      stdabi::String shown{};
      reinterpret_cast<stdabi::String* (*)(const void*, stdabi::String*)>(layout.surfaceDisplayName)(*surface, &shown);
      std::string line(name);
      if (shown.view() != name) line += " (" + std::string(shown.view()) + ")";
      destroy(shown);
      return text::speakable(line);
   }
   return text::speakable(name);
}

// The scripting areas or positions on `surface`, from one of ExtraScriptData's maps by surface.
std::vector<const std::byte*> scriptObjects(const std::byte* map, bool areas, uint32_t surface) {
   const auto* data = at<const std::byte*>(map, layout.mapExtraScriptData);
   if (!data) return {};
   const uint32_t keyOffset = areas ? layout.scriptRectangleNodeValue : layout.scriptPositionNodeValue;
   const uint32_t vectorOffset = areas ? layout.scriptRectangleNodeVector : layout.scriptPositionNodeVector;
   const auto& bySurface =
      *reinterpret_cast<const stdabi::Tree<void>*>(data + (areas ? layout.scriptRectangles : layout.scriptPositions));
   std::vector<const std::byte*> found;
   for (const std::byte* node : bySurface) {
      if (at<uint32_t>(node, keyOffset) != surface) continue;
      const auto objects = at<stdabi::Vector<const std::byte* const>>(node, vectorOffset);
      found.insert(found.end(), objects.begin(), objects.end());
   }
   return found;
}

std::string scriptName(const std::byte* object) {
   return text::speakable(at<stdabi::String>(object, layout.scriptDataName).view());
}

// The name of the scripting area or position `id` on `surface`; nothing when it has none.
std::optional<std::string> scriptNameById(const std::byte* map, bool areas, uint32_t surface, uint32_t id) {
   for (const std::byte* object : scriptObjects(map, areas, surface))
      if (at<uint32_t>(object, layout.scriptDataId) == id) return scriptName(object);
   return std::nullopt;
}

// The name of the newest scripting area or position on `surface` (the highest id): the one just
// created. Nothing when it has none.
std::optional<std::string> newestScriptName(const std::byte* map, bool areas, uint32_t surface) {
   const std::byte* newest = nullptr;
   for (const std::byte* object : scriptObjects(map, areas, surface))
      if (!newest || at<uint32_t>(object, layout.scriptDataId) > at<uint32_t>(newest, layout.scriptDataId))
         newest = object;
   if (!newest) return std::nullopt;
   return scriptName(newest);
}

// The tick a tick once was asked at, while the map has not left it.
std::mutex g_mutex;
std::optional<uint64_t> g_tickFrom;

using ActionPerformed = void (*)(void* handler, const std::byte* action, std::byte* controller);
ActionPerformed g_actionPerformedOriginal = nullptr;

void detourActionPerformed(void* handler, const std::byte* action, std::byte* controller) {
   const std::byte* player = at<const std::byte*>(controller, layout.controllerPlayer);
   if (!player || player != localPlayer()) {
      g_actionPerformedOriginal(handler, action, controller);
      return;
   }
   const auto* map = at<const std::byte*>(player, layout.playerMap);
   const uint32_t type = at<uint8_t>(action, layout.editorActionType);

   // A removed scripting object is named before it is gone.
   std::optional<std::string> removed;
   const uint32_t surface = at<uint32_t>(action, layout.editorActionSurface);
   const bool area = type == layout.editorRemoveScriptRectangle;
   if ((area || type == layout.editorRemoveScriptPosition) && map)
      removed = scriptNameById(map, area, surface,
                               at<uint32_t>(action, layout.editorActionSurface + layout.surfaceAndDataData));

   g_actionPerformedOriginal(handler, action, controller);
   if (!map) return;

   const int playerIndex = at<uint16_t>(player, layout.playerIndex) + 1;
   if (type == layout.editorTogglePause || type == layout.editorIncreaseSpeed || type == layout.editorDecreaseSpeed ||
       type == layout.editorResetSpeed) {
      speech::say(speedLine(map));
   } else if (type == layout.editorTickOnce) {
      std::scoped_lock lock(g_mutex);
      g_tickFrom = at<uint64_t>(map, layout.mapEntityTick);
   } else if (type == layout.editorEntityVariation || type == layout.editorCliffVariation ||
              type == layout.editorResourceVariation || type == layout.editorTileVariation) {
      if (const std::optional<cursor::Tile> tile = cursor::position(playerIndex)) reader::read(tile->x, tile->y);
   } else if (type == layout.editorCloneSource) {
      speech::say(vocab::kCloneSourceSet);
   } else if (type == layout.editorCloneDestination) {
      speech::say(vocab::kCloneDestinationSet);
   } else if (removed) {
      speech::say(area ? vocab::kScriptAreaRemoved(*removed) : vocab::kScriptPositionRemoved(*removed));
   } else if (type == layout.editorCreateScriptRectangle || type == layout.editorCreateScriptPosition) {
      const bool rectangle = type == layout.editorCreateScriptRectangle;
      if (const std::optional<std::string> created = newestScriptName(map, rectangle, surface))
         speech::say(rectangle ? vocab::kScriptAreaCreated(*created) : vocab::kScriptPositionCreated(*created));
   } else if (type == layout.editorSelectScriptRectangle || type == layout.editorSelectScriptPosition) {
      const bool rectangle = type == layout.editorSelectScriptRectangle;
      const uint32_t id = at<uint32_t>(action, layout.editorActionSurface + layout.surfaceAndDataData);
      if (const std::optional<std::string> selected = scriptNameById(map, rectangle, surface, id))
         speech::say(rectangle ? vocab::kScriptAreaSelected(*selected) : vocab::kScriptPositionSelected(*selected));
   } else if (type == layout.editorSwitchToSurface) {
      speech::say(surfaceLine(map, at<stdabi::String>(action, layout.editorActionString).view()));
   }
}

} // namespace

void frame(const std::byte* player) {
   const auto* map = player ? at<const std::byte*>(player, layout.playerMap) : nullptr;
   if (!map) return;
   const uint64_t tick = at<uint64_t>(map, layout.mapEntityTick);
   {
      std::scoped_lock lock(g_mutex);
      if (!g_tickFrom || *g_tickFrom == tick) return;
      g_tickFrom.reset();
   }
   speech::say(vocab::kEditorTick(tick));
}

void* actionPerformedDetour() { return reinterpret_cast<void*>(&detourActionPerformed); }
void** actionPerformedOriginal() { return reinterpret_cast<void**>(&g_actionPerformedOriginal); }

} // namespace fa::editoractions
