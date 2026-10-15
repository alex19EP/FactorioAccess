#include "flyingtext.h"

#include "game.h"
#include "speech.h"

#include <cstddef>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

template <class T>
T at(const void* object, uint32_t offset) {
   T value;
   std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
   return value;
}

// A map text waiting for the main thread, which knows what the view shows.
struct MapText {
   std::string text;
   uint32_t surface;
   cursormoves::Position position;
};

// Map texts come from the update thread (mining, the input actions) and the main thread; frame()
// takes them on the main thread.
std::mutex g_mutex;
std::vector<MapText> g_mapTexts;

// void Map::addLocalFlyingText(LocalMapFlyingText&&). It moves the text out of its argument, so
// the text is read first.
using AddLocalFlyingText = void (*)(void* map, void* flyingText);
AddLocalFlyingText g_mapOriginal = nullptr;

void mapFlyingText(void* map, void* flyingText) {
   MapText copy{std::string(view(*reinterpret_cast<const MsvcString*>(static_cast<const std::byte*>(flyingText) +
                                                                      layout.localMapFlyingTextText))),
                at<uint32_t>(flyingText, layout.localMapFlyingTextSurface),
                at<cursormoves::Position>(flyingText, layout.localMapFlyingTextPosition)};
   {
      std::scoped_lock lock(g_mutex);
      g_mapTexts.push_back(std::move(copy));
   }
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

void frame(const std::optional<Shown>& shown) {
   std::vector<MapText> texts;
   {
      std::scoped_lock lock(g_mutex);
      texts.swap(g_mapTexts);
   }
   if (!shown) return;
   for (const MapText& text : texts)
      if (text.surface == shown->surface && shown->screen.holds(text.position)) speech::sayShown(text.text);
}

void* mapDetour() { return reinterpret_cast<void*>(&mapFlyingText); }
void** mapOriginal() { return reinterpret_cast<void**>(&g_mapOriginal); }
void* guiDetour() { return reinterpret_cast<void*>(&guiFlyingText); }
void** guiOriginal() { return reinterpret_cast<void**>(&g_guiOriginal); }

} // namespace fa::flyingtext
