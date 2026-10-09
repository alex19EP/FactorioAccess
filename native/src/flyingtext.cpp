#include "flyingtext.h"

#include "game.h"
#include "speech.h"

#include <cstddef>
#include <string_view>

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

// void Map::addLocalFlyingText(LocalMapFlyingText&&). It moves the text out of its argument, so
// the text is read first.
using AddLocalFlyingText = void (*)(void* map, void* flyingText);
AddLocalFlyingText g_mapOriginal = nullptr;

void mapFlyingText(void* map, void* flyingText) {
   speech::sayShown(view(
      *reinterpret_cast<const MsvcString*>(static_cast<const std::byte*>(flyingText) + layout.localMapFlyingTextText)));
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
   speech::sayShown(view(*text));
   g_guiOriginal(allocator, where, position, text, color, font, timeToLive, screenWidth);
}

} // namespace

void* mapDetour() { return reinterpret_cast<void*>(&mapFlyingText); }
void** mapOriginal() { return reinterpret_cast<void**>(&g_mapOriginal); }
void* guiDetour() { return reinterpret_cast<void*>(&guiFlyingText); }
void** guiOriginal() { return reinterpret_cast<void**>(&g_guiOriginal); }

} // namespace fa::flyingtext
