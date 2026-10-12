#include "prototypes.h"

#include "game.h"

#include <cstddef>
#include <cstdint>

namespace fa::prototypes {

namespace {

using game::layout;

// MSVC std::string: a 16-byte small buffer or a heap pointer, then size and capacity.
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

// An MSVC std::map node holding a std::pair<const std::string, T*>. The map itself starts with
// its head node, whose parent is the root.
struct NameNode {
   const NameNode* left;
   const NameNode* parent;
   const NameNode* right;
   char color;
   char isNil;
   MsvcString key;
   const std::byte* prototype;
};

const std::byte* find(uintptr_t map, std::string_view name) {
   const NameNode* head = *reinterpret_cast<const NameNode* const*>(map);
   const NameNode* node = head->parent;
   while (!node->isNil) {
      int order = view(node->key).compare(name);
      if (order == 0) return node->prototype;
      node = order < 0 ? node->right : node->left;
   }
   return nullptr;
}

// Whether `layers` (CollisionLayers) holds the layer named `name`; false when no such layer exists.
bool hasLayer(const std::byte* layers, std::string_view name) {
   const std::byte* layer = find(layout.collisionLayerNames, name);
   if (!layer) return false;
   const uint16_t row = *reinterpret_cast<const uint16_t*>(layer + layout.collisionLayerRow);
   const uint64_t mask = *reinterpret_cast<const uint64_t*>(layer + layout.collisionLayerMask);
   return (reinterpret_cast<const uint64_t*>(layers)[row] & mask) != 0;
}

} // namespace

TileKind tileKind(const void* tilePrototype) {
   const std::byte* layers = static_cast<const std::byte*>(tilePrototype) + layout.tileCollisionLayers;
   if (hasLayer(layers, "water_tile") && !hasLayer(layers, "ground_tile"))
      return hasLayer(layers, "player") ? TileKind::Liquid : TileKind::Shallow;
   if (hasLayer(layers, "meltable")) return TileKind::Ice;
   return TileKind::Land;
}

uint16_t tileFluid(const void* tilePrototype) {
   return *reinterpret_cast<const uint16_t*>(static_cast<const std::byte*>(tilePrototype) + layout.tileFluid);
}

std::optional<std::string> localisedName(std::string_view kind, std::string_view name) {
   for (size_t i = 0; i < game::kNamedPrototypeCount; ++i) {
      if (kind != game::kNamedPrototypes[i].tag) continue;
      const std::byte* prototype = find(layout.prototypeNames[i], name);
      if (!prototype) return std::nullopt;
      using Str = const MsvcString* (*)(const void* localisedString, const void* localeProvider);
      return std::string(
         view(*reinterpret_cast<Str>(layout.localisedStringStr)(prototype + layout.prototypeLocalisedName, nullptr)));
   }
   return std::nullopt;
}

std::optional<Named> identify(const void* prototype) {
   auto* base = static_cast<const std::byte*>(prototype);
   std::string_view name = view(*reinterpret_cast<const MsvcString*>(base + layout.prototypeName));
   for (size_t i = 0; i < game::kNamedPrototypeCount; ++i)
      if (find(layout.prototypeNames[i], name) == base) return Named{game::kNamedPrototypes[i].tag, std::string(name)};
   return std::nullopt;
}

} // namespace fa::prototypes
