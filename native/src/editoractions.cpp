#include "editoractions.h"

#include "cursor.h"
#include "game.h"
#include "reader.h"
#include "speech.h"
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

// MSVC std::string: a 16-byte small buffer or a heap pointer, then size and capacity.
struct MsvcString {
   union {
      char buffer[16];
      char* pointer;
   };
   size_t size;
   size_t capacity;
};

std::string_view view(const MsvcString& string) {
   return {string.capacity >= sizeof(string.buffer) ? string.pointer : string.buffer, string.size};
}

// Frees what a std::string the game returned holds, as its own destructor does: through the game's
// allocator, which keeps the true start of a large block just before it.
void destroy(MsvcString& string) {
   if (string.capacity < sizeof(string.buffer)) return;
   void* block = string.pointer;
   if (string.capacity + 1 >= 0x1000) block = reinterpret_cast<void**>(block)[-1];
   reinterpret_cast<void (*)(void*)>(layout.gameOperatorDelete)(block);
}

// An MSVC std::map node: the links, then the stored pair at an offset the PDB gives.
struct MapNode {
   const MapNode* left;
   const MapNode* parent;
   const MapNode* right;
   char color;
   char isNil;
};

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
   MsvcString shown{};
   reinterpret_cast<MsvcString* (*)(const void*, MsvcString*)>(layout.gameSpeedStr)(speed, &shown);
   std::string line(view(shown));
   destroy(shown);
   if (at<bool>(paused, layout.pausedSetting)) line += " " + std::string(vocab::kEditorPaused);
   return text::speakable(line);
}

// The name of the scripting area or position `id` on `surface`, from one of ExtraScriptData's maps
// by surface; nothing when it has none.
std::optional<std::string> scriptName(const std::byte* byShape, uint32_t keyOffset, uint32_t vectorOffset,
                                      uint32_t surface, uint32_t id) {
   const auto* head = at<const MapNode*>(byShape, 0);
   std::vector<const MapNode*> pending{head->parent};
   while (!pending.empty()) {
      const MapNode* node = pending.back();
      pending.pop_back();
      if (node->isNil) continue;
      pending.push_back(node->left);
      pending.push_back(node->right);
      if (at<uint32_t>(node, keyOffset) != surface) continue;
      const auto* first = at<const std::byte* const*>(node, vectorOffset);
      const auto* last = at<const std::byte* const*>(node, vectorOffset + sizeof(void*));
      for (const std::byte* const* object = first; object != last; ++object)
         if (at<uint32_t>(*object, layout.scriptDataId) == id)
            return std::string(view(at<MsvcString>(*object, layout.scriptDataName)));
   }
   return std::nullopt;
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
   const bool area = type == layout.editorRemoveScriptRectangle;
   if ((area || type == layout.editorRemoveScriptPosition) && map) {
      const auto* data = at<const std::byte*>(map, layout.mapExtraScriptData);
      const uint32_t surface = at<uint32_t>(action, layout.editorActionSurface);
      const uint32_t id = at<uint32_t>(action, layout.editorActionSurface + layout.surfaceAndDataData);
      if (data)
         removed = area ? scriptName(data + layout.scriptRectangles, layout.scriptRectangleNodeValue,
                                     layout.scriptRectangleNodeVector, surface, id)
                        : scriptName(data + layout.scriptPositions, layout.scriptPositionNodeValue,
                                     layout.scriptPositionNodeVector, surface, id);
   }

   g_actionPerformedOriginal(handler, action, controller);
   if (!map) return;

   const int playerIndex = at<uint16_t>(player, layout.playerIndex) + 1;
   if (type == layout.editorTogglePause || type == layout.editorIncreaseSpeed || type == layout.editorDecreaseSpeed ||
       type == layout.editorResetSpeed) {
      speech::say(speedLine(map), true);
   } else if (type == layout.editorTickOnce) {
      std::scoped_lock lock(g_mutex);
      g_tickFrom = at<uint64_t>(map, layout.mapEntityTick);
   } else if (type == layout.editorEntityVariation || type == layout.editorCliffVariation ||
              type == layout.editorResourceVariation || type == layout.editorTileVariation) {
      if (const std::optional<cursor::Tile> tile = cursor::position(playerIndex)) reader::read(tile->x, tile->y);
   } else if (type == layout.editorCloneSource) {
      speech::say(vocab::kCloneSourceSet, true);
   } else if (type == layout.editorCloneDestination) {
      speech::say(vocab::kCloneDestinationSet, true);
   } else if (removed) {
      const std::string name = text::speakable(*removed);
      speech::say(area ? vocab::kScriptAreaRemoved(name) : vocab::kScriptPositionRemoved(name), true);
   } else if (type == layout.editorSwitchToSurface) {
      speech::say(text::speakable(view(at<MsvcString>(action, layout.editorActionString))), true);
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
   speech::say(vocab::kEditorTick(tick), true);
}

void* actionPerformedDetour() { return reinterpret_cast<void*>(&detourActionPerformed); }
void** actionPerformedOriginal() { return reinterpret_cast<void**>(&g_actionPerformedOriginal); }

} // namespace fa::editoractions
