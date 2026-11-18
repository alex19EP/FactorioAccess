#include "vocab.h"

#include "game.h"
#include "stdabi.h"

#include <cstddef>
#include <cstring>
#include <iterator>

namespace fa::vocab {

namespace {

using game::layout;

// Room for one LocalisedString; game::layout says how much one takes.
constexpr size_t kLocalisedCapacity = 256;
// LocalisedString::Mode::Literal: a text the game says as it is.
constexpr uint8_t kLiteral = 2;

using FromKeyFunction = void* (*)(void* out, const char* key);
using LiteralFunction = void* (*)(void* out, uint8_t mode, const char* text);
using WithOne = void* (*)(void* out, const stdabi::String* key, const void*);
using WithTwo = void* (*)(void* out, const stdabi::String* key, const void*, const void*);
using WithThree = void* (*)(void* out, const stdabi::String* key, const void*, const void*, const void*);
using StrFunction = const stdabi::String* (*)(const void* localisedString, const void* localeProvider);

// The key as a std::string the game reads: the constructors taking one copy it, so it borrows the
// key's own characters when they do not fit the small buffer.
stdabi::String keyString(const char* key) { return stdabi::String::borrow(key, std::strlen(key)); }

// The most parameters the game's constructors that copy them take.
constexpr size_t kCopiedParameters = 3;

const stdabi::String& textOf(const void* localised) {
   return *reinterpret_cast<StrFunction>(layout.localisedStringStr)(localised, nullptr);
}

void destroy(void* localised) { reinterpret_cast<void (*)(void*)>(layout.localisedStringDestroy)(localised); }

// More parameters than the copying constructors take: the constructor that takes over a vector of
// them, both the vector and the key made by the game, so that it frees them as its own.
std::string translateMany(const char* key, std::initializer_list<std::string> parameters) {
   const size_t size = layout.localisedStringSize;
   const size_t bytes = size * parameters.size();
   auto* first = static_cast<std::byte*>(reinterpret_cast<void* (*)(size_t)>(layout.gameOperatorNew)(bytes));
   std::byte* made = first;
   for (const std::string& parameter : parameters) {
      reinterpret_cast<LiteralFunction>(layout.localisedStringLiteral)(made, kLiteral, parameter.c_str());
      made += size;
   }
   stdabi::Vector<std::byte> vector{first, first + bytes, first + bytes};
   alignas(8) std::byte holder[kLocalisedCapacity];
   reinterpret_cast<FromKeyFunction>(layout.localisedStringFromKey)(holder, key);
   alignas(8) std::byte localised[kLocalisedCapacity];
   using WithVector = void* (*)(void* out, int32_t mode, void* key, void* parameters);
   reinterpret_cast<WithVector>(layout.localisedStringWithVector)(
      localised, static_cast<int32_t>(layout.localisedTranslation), holder + layout.localisedStringKey, &vector);
   destroy(holder);
   const stdabi::String& text = textOf(localised);
   std::string out(text.view());
   destroy(localised);
   return out;
}

} // namespace

std::string translate(const char* key, std::initializer_list<std::string> parameters) {
   if (layout.localisedStringSize > kLocalisedCapacity || parameters.size() > kMaxParameters) return {};
   if (parameters.size() > kCopiedParameters) return translateMany(key, parameters);
   // The parameters, then the string that copies them.
   struct Made {
      alignas(8) std::byte strings[kCopiedParameters + 1][kLocalisedCapacity];
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
   const stdabi::String name = keyString(key);
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
   const stdabi::String* text = reinterpret_cast<StrFunction>(layout.localisedStringStr)(localised, nullptr);
   return std::string(text->view());
}

std::string position(int index, int count) { return kPosition(index, count); }

std::string expandedState(bool expanded) { return expanded ? kExpanded : kCollapsed; }

std::string flyoutHint(int count) { return kSubmenu(count); }

std::string direction(int eighth) { return kDirection(eighth * 2); }

std::string alertCategory(uint8_t category) {
   return category < std::size(kAlertCategories) ? kAlertCategories[category] : std::string();
}

} // namespace fa::vocab
