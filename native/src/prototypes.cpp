#include "prototypes.h"

#include "game.h"
#include "stdabi.h"

#include <cstddef>
#include <cstdint>

namespace fa::prototypes {

namespace {

using game::layout;

// The value of a std::map<std::string, T*>.
struct NamedPrototype {
   stdabi::String key;
   const std::byte* prototype;
};

const std::byte* find(uintptr_t map, std::string_view name) {
   const auto* found = reinterpret_cast<const stdabi::Tree<NamedPrototype>*>(map)->search(
      [&](const auto* node) { return node->value.key.view().compare(name); });
   return found ? found->value.prototype : nullptr;
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
      using Str = const stdabi::String* (*)(const void* localisedString, const void* localeProvider);
      return std::string(
         reinterpret_cast<Str>(layout.localisedStringStr)(prototype + layout.prototypeLocalisedName, nullptr)->view());
   }
   return std::nullopt;
}

std::optional<Named> identify(const void* prototype) {
   auto* base = static_cast<const std::byte*>(prototype);
   std::string_view name = reinterpret_cast<const stdabi::String*>(base + layout.prototypeName)->view();
   for (size_t i = 0; i < game::kNamedPrototypeCount; ++i)
      if (find(layout.prototypeNames[i], name) == base) return Named{game::kNamedPrototypes[i].tag, std::string(name)};
   return std::nullopt;
}

} // namespace fa::prototypes
