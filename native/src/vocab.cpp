#include "vocab.h"

#include "game.h"

#include <cstddef>
#include <cstring>
#include <iterator>

namespace fa::vocab {

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

// Room for one LocalisedString; game::layout says how much one takes.
constexpr size_t kLocalisedCapacity = 256;
// LocalisedString::Mode::Literal: a text the game says as it is.
constexpr uint8_t kLiteral = 2;

using FromKeyFunction = void* (*)(void* out, const char* key);
using LiteralFunction = void* (*)(void* out, uint8_t mode, const char* text);
using WithOne = void* (*)(void* out, const MsvcString* key, const void*);
using WithTwo = void* (*)(void* out, const MsvcString* key, const void*, const void*);
using WithThree = void* (*)(void* out, const MsvcString* key, const void*, const void*, const void*);
using StrFunction = const MsvcString* (*)(const void* localisedString, const void* localeProvider);

// The key as a std::string the game reads: the constructors taking one copy it, so it borrows the
// key's own characters when they do not fit the small buffer.
MsvcString keyString(const char* key) {
   MsvcString out{};
   out.size = std::strlen(key);
   if (out.size < sizeof(out.buffer)) {
      std::memcpy(out.buffer, key, out.size + 1);
      out.capacity = sizeof(out.buffer) - 1;
   } else {
      out.pointer = key;
      out.capacity = out.size;
   }
   return out;
}

} // namespace

std::string translate(const char* key, std::initializer_list<std::string> parameters) {
   if (layout.localisedStringSize > kLocalisedCapacity || parameters.size() > kMaxParameters) return {};
   // The parameters, then the string that copies them.
   struct Made {
      alignas(8) std::byte strings[kMaxParameters + 1][kLocalisedCapacity];
      size_t count = 0;
      ~Made() {
         for (size_t i = 0; i < count; ++i)
            reinterpret_cast<void (*)(void*)>(layout.localisedStringDestroy)(strings[i]);
      }
   } made;
   for (const std::string& parameter : parameters) {
      reinterpret_cast<LiteralFunction>(layout.localisedStringLiteral)(made.strings[made.count], kLiteral,
                                                                       parameter.c_str());
      ++made.count;
   }
   void* localised = made.strings[made.count];
   const MsvcString name = keyString(key);
   const auto* with = layout.localisedStringWithParameters;
   switch (parameters.size()) {
   case 0: reinterpret_cast<FromKeyFunction>(layout.localisedStringFromKey)(localised, key); break;
   case 1: reinterpret_cast<WithOne>(with[0])(localised, &name, made.strings[0]); break;
   case 2: reinterpret_cast<WithTwo>(with[1])(localised, &name, made.strings[0], made.strings[1]); break;
   case 3:
      reinterpret_cast<WithThree>(with[2])(localised, &name, made.strings[0], made.strings[1], made.strings[2]);
      break;
   }
   ++made.count;
   const MsvcString* text = reinterpret_cast<StrFunction>(layout.localisedStringStr)(localised, nullptr);
   return {text->capacity >= sizeof(text->buffer) ? text->pointer : text->buffer, text->size};
}

std::string position(int index, int count) { return kPosition(index, count); }

std::string expandedState(bool expanded) { return expanded ? kExpanded : kCollapsed; }

std::string flyoutHint(int count) { return kSubmenu(count); }

std::string direction(int eighth) { return kDirection(eighth * 2); }

std::string alertCategory(uint8_t category) {
   return category < std::size(kAlertCategories) ? kAlertCategories[category] : std::string();
}

} // namespace fa::vocab
