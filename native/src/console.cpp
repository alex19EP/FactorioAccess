#include "console.h"

#include "game.h"
#include "speech.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace fa::console {

namespace {

using game::layout;

template <class T>
T at(const void* object, uint32_t offset) {
   return *reinterpret_cast<const T*>(static_cast<const std::byte*>(object) + offset);
}

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

// Every player has a console, and every client keeps them all; only this client's player's is
// on its screen.
bool isLocalConsole(const void* console) {
   const auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   const void* game = context ? at<const void*>(context, layout.globalGame) : nullptr;
   const void* player = game ? at<const void*>(game, layout.gameLocalPlayer) : nullptr;
   return player && at<const void*>(console, layout.outputConsoleOwner) == player;
}

// The newest line of one of the console's two std::lists: the head node's next. A line that was
// added takes that place, even when the oldest line drops off past the cap.
const void* newest(const void* console, uint32_t list) { return at<const void*>(at<const void*>(console, list), 0); }

// void OutputConsole::add(std::string const& playerName, Color, LocalisedString const& body,
// Player const* speaker, PrintSettings const&, std::vector<SavedSpecialItemReference>&&). Color
// is 16 bytes, so x64 passes it by address.
using Add = void (*)(void* console, const MsvcString* playerName, const void* color, const void* body,
                     const void* speaker, const void* settings, void* specialItems);
Add g_original = nullptr;

void add(void* console, const MsvcString* playerName, const void* color, const void* body, const void* speaker,
         const void* settings, void* specialItems) {
   if (!isLocalConsole(console)) {
      g_original(console, playerName, color, body, speaker, settings, specialItems);
      return;
   }
   // Shown as the speaker's name, if any, then the body ("Alex" ": hello"). Translated before the
   // call, while the caller still owns the body; once added, the renderer reads the line too.
   using Str = const MsvcString* (*)(const void* localisedString, const void* localeProvider);
   std::string line(view(*playerName));
   line += view(*reinterpret_cast<Str>(layout.localisedStringStr)(body, nullptr));
   const void* before = newest(console, layout.outputConsoleItems);
   const void* beforeNotSaved = newest(console, layout.outputConsoleItemsNotSaved);
   g_original(console, playerName, color, body, speaker, settings, specialItems);
   // Skipped as a repeat of a line still on screen: nothing new to hear.
   if (newest(console, layout.outputConsoleItems) != before ||
       newest(console, layout.outputConsoleItemsNotSaved) != beforeNotSaved)
      speech::sayShown(line);
}

} // namespace

void* addDetour() { return reinterpret_cast<void*>(&add); }
void** addOriginal() { return reinterpret_cast<void**>(&g_original); }

} // namespace fa::console
