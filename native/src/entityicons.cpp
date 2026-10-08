#include "entityicons.h"

#include "game.h"
#include "log.h"
#include "prototypes.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

namespace fa::entityicons {

namespace {

using game::layout;

template <class T>
T at(const void* object, uint32_t offset) {
   T value;
   std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
   return value;
}

template <class Function>
Function virtualAt(const void* object, uint32_t slot) {
   auto vtable = *static_cast<void* const* const*>(object);
   return reinterpret_cast<Function>(vtable[slot]);
}

struct MsvcString {
   union {
      char buffer[16];
      const char* pointer;
   };
   size_t size;
   size_t capacity;
};

std::string_view view(const MsvcString& string) {
   return {string.capacity > 15 ? string.pointer : string.buffer, string.size};
}

template <class T>
struct MsvcVector {
   T* first;
   T* last;
   T* end;
};

// An MSVC std::map node holding a std::pair<const std::string, Sprite*>.
struct SpriteNode {
   const SpriteNode* left;
   const SpriteNode* parent;
   const SpriteNode* right;
   char color;
   char isNil;
   MsvcString key;
   const void* sprite;
};

// What this thread records while it draws an entity to read it.
thread_local Drawn* t_drawn = nullptr;

const std::byte* utilitySprites() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   return at<const std::byte*>(context, layout.globalUtilitySprites);
}

// The utility sprite name of `sprite` ("electricity_icon"), or empty when it is not one.
std::string utilityName(const void* sprite) {
   const std::byte* sprites = utilitySprites();
   auto* address = static_cast<const std::byte*>(sprite);
   if (!sprites || address < sprites || address >= sprites + layout.utilitySpritesSize) return {};
   const SpriteNode* head = at<const SpriteNode*>(sprites, layout.utilitySpritesMapping);
   std::vector<const SpriteNode*> pending{head->parent};
   while (!pending.empty()) {
      const SpriteNode* node = pending.back();
      pending.pop_back();
      if (node->isNil) continue;
      if (node->sprite == sprite) return std::string(view(node->key));
      pending.push_back(node->left);
      pending.push_back(node->right);
   }
   return {};
}

const std::byte* qualityPrototype(uint8_t index) {
   const auto& list = *reinterpret_cast<const MsvcVector<const std::byte* const>*>(layout.qualityPrototypes);
   return index < static_cast<size_t>(list.last - list.first) ? list.first[index] : nullptr;
}

// The quality badge drawQualityPartOfInfoIcon draws beside an icon.
void readQuality(Icon& icon, uint16_t condition, uint32_t flags) {
   const auto index = static_cast<uint8_t>(condition & 0xff);
   auto comparison = static_cast<uint8_t>(condition >> 8);
   if (index == 0) {
      icon.anyQuality = (flags & (game::kDrawingFlagQualityFilter | game::kDrawingFlagAnyQuality)) != 0;
      return;
   }
   const std::byte* quality = qualityPrototype(index);
   if (!quality) return;
   if (!at<bool>(quality, layout.qualityDrawByDefault) && comparison == game::kComparisonEquals) return;
   if (auto named = prototypes::identify(quality)) icon.quality = std::move(named->name);
   if (comparison != game::kComparisonEquals)
      icon.comparison = reinterpret_cast<const char* (*)(const uint8_t*)>(layout.comparisonStr)(&comparison);
}

bool showsStatusIcons(const void* drawQueue) {
   const void* parameters = at<const void*>(drawQueue, layout.drawQueueRenderParameters);
   return (at<uint32_t>(parameters, layout.renderParametersFlags) & game::kRenderShowStatusIcons) != 0;
}

// ---- detours ----

using DrawAlertFunction = void (*)(const void* entity, void* drawQueue, const void* sprite, const void* position,
                                   bool blinking);
DrawAlertFunction g_drawAlertOriginal = nullptr;

void detourDrawAlert(const void* entity, void* drawQueue, const void* sprite, const void* position, bool blinking) {
   // Recorded whatever the blinking shows this frame: the icon is there, on and off.
   if (Drawn* drawn = t_drawn; drawn && showsStatusIcons(drawQueue)) {
      if (std::string name = utilityName(sprite); !name.empty())
         drawn->status.push_back(std::move(name));
      else
         log::info("Status icon {} is no utility sprite", sprite);
   }
   g_drawAlertOriginal(entity, drawQueue, sprite, position, blinking);
}

// QualityCondition and DrawingFlags go by value as integers; Color by pointer to a copy.
using DrawInfoIconFunction = void (*)(void* drawQueue, const void* sprite, uint16_t quality, const void* position,
                                      double scale, uint32_t flags, uint32_t layer, const void* shift, int8_t order,
                                      const void* color);
DrawInfoIconFunction g_drawInfoIconOriginal = nullptr;

void detourDrawInfoIcon(void* drawQueue, const void* sprite, uint16_t quality, const void* position, double scale,
                        uint32_t flags, uint32_t layer, const void* shift, int8_t order, const void* color) {
   if (Drawn* drawn = t_drawn; drawn && sprite && !(flags & game::kDrawingFlagIconBackground)) {
      Icon icon;
      const void* owner = at<const void*>(sprite, layout.spriteOwner);
      if (std::string name = utilityName(sprite); !name.empty()) {
         // Drawn over the filter icon before it: that filter denies.
         if (name == "filter_blacklist" && !drawn->icons.empty()) {
            drawn->icons.back().denied = true;
            name.clear();
         }
         icon.name = std::move(name);
      } else if (auto named = owner ? prototypes::identify(owner) : std::nullopt) {
         icon.kind = named->kind;
         icon.name = std::move(named->name);
      } else {
         log::info("Info icon {} has no prototype", sprite);
      }
      if (!icon.name.empty()) {
         readQuality(icon, quality, flags);
         drawn->icons.push_back(std::move(icon));
      }
   }
   g_drawInfoIconOriginal(drawQueue, sprite, quality, position, scale, flags, layer, shift, order, color);
}

// The queue entities are drawn into to read them. Nothing renders it; it is cleared after each read.
std::byte* g_queue = nullptr;

const std::byte* mainRenderParameters() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   auto* game = context ? at<const std::byte*>(context, layout.globalGame) : nullptr;
   auto* view = game ? at<const std::byte*>(game, layout.gameView) : nullptr;
   auto* renderer = view ? at<const std::byte*>(view, layout.gameViewRenderer) : nullptr;
   return renderer ? renderer + layout.gameRendererParameters : nullptr;
}

} // namespace

std::optional<Drawn> read(const void* entity) {
   const std::byte* parameters = mainRenderParameters();
   if (!parameters) return std::nullopt;
   if (!g_queue) {
      g_queue = static_cast<std::byte*>(::operator new(layout.drawQueueSize, std::align_val_t{16}));
      reinterpret_cast<void* (*)(void*, const void*)>(layout.drawQueueConstruct)(g_queue, parameters);
   }
   std::memcpy(g_queue + layout.drawQueueRenderParameters, &parameters, sizeof(parameters));

   Drawn drawn;
   t_drawn = &drawn;
   virtualAt<void (*)(const void*, void*)>(entity, layout.entityDraw)(entity, g_queue);
   t_drawn = nullptr;
   reinterpret_cast<void (*)(void*)>(layout.drawQueueClear)(g_queue);
   return drawn;
}

void* drawAlertDetour() { return reinterpret_cast<void*>(&detourDrawAlert); }
void** drawAlertOriginal() { return reinterpret_cast<void**>(&g_drawAlertOriginal); }
void* drawInfoIconDetour() { return reinterpret_cast<void*>(&detourDrawInfoIcon); }
void** drawInfoIconOriginal() { return reinterpret_cast<void**>(&g_drawInfoIconOriginal); }

} // namespace fa::entityicons
