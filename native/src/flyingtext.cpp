#include "flyingtext.h"

#include "game.h"
#include "speech.h"
#include "text.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace fa::flyingtext {

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

// Each line on its own: the item count text puts one item per line. Queued, so a burst of texts
// (mining several things at once) is read through rather than cut short.
void say(std::string_view text) {
   while (!text.empty()) {
      size_t end = text.find('\n');
      std::string line = text::speakable(text.substr(0, end));
      if (!line.empty()) speech::say(std::move(line), false);
      if (end == std::string_view::npos) break;
      text.remove_prefix(end + 1);
   }
}

// void Map::addLocalFlyingText(LocalMapFlyingText&&). It moves the text out of its argument, so
// the text is read first.
using AddLocalFlyingText = void (*)(void* map, void* flyingText);
AddLocalFlyingText g_mapOriginal = nullptr;

void mapFlyingText(void* map, void* flyingText) {
   say(view(*reinterpret_cast<const MsvcString*>(static_cast<const std::byte*>(flyingText) +
                                                  layout.localMapFlyingTextText)));
   g_mapOriginal(map, flyingText);
}

// std::_Default_allocator_traits<std::allocator<agui::GuiFlyingText>>::construct<agui::GuiFlyingText,
// Point&, std::string const&, Color&, Font const*&, int&, int&>(allocator, where, position, text,
// color, font, timeToLive, screenWidth): static, so a plain function of eight arguments.
using ConstructGuiFlyingText = void (*)(void* allocator, void* where, void* position, const MsvcString* text,
                                        void* color, void* font, int* timeToLive, int* screenWidth);
ConstructGuiFlyingText g_guiOriginal = nullptr;

void guiFlyingText(void* allocator, void* where, void* position, const MsvcString* text, void* color, void* font,
                   int* timeToLive, int* screenWidth) {
   say(view(*text));
   g_guiOriginal(allocator, where, position, text, color, font, timeToLive, screenWidth);
}

} // namespace

void* mapDetour() { return reinterpret_cast<void*>(&mapFlyingText); }
void** mapOriginal() { return reinterpret_cast<void**>(&g_mapOriginal); }
void* guiDetour() { return reinterpret_cast<void*>(&guiFlyingText); }
void** guiOriginal() { return reinterpret_cast<void**>(&g_guiOriginal); }

} // namespace fa::flyingtext
