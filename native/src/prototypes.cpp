#include "prototypes.h"

#include "game.h"

#include <cstddef>

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

} // namespace

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
