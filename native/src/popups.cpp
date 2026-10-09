#include "popups.h"

#include "agui.h"
#include "game.h"
#include "speech.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace fa::popups {

namespace {

using agui::Kind;
using agui::Widget;
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

// The button reads "New tip" and the tip's name; it is made only on this client, for its player.
using TipButton = void* (*)(void* button, const void* item, void* context);
TipButton g_tipOriginal = nullptr;

void* tipButton(void* button, const void* item, void* context) {
   g_tipOriginal(button, item, context);
   speech::sayShown(agui::text(static_cast<const Widget*>(button)));
   return button;
}

// Made by SpeechBubble::updateGui for this client's player, when the bubble is on the surface
// they look at; made again if they leave that surface and come back.
using SpeechBubbleGui = void* (*)(void* gui, void* bubble, const MsvcString* text, void* gameView,
                                  const void* wrapperStyle, void* bubbleStyle);
SpeechBubbleGui g_speechBubbleOriginal = nullptr;

void* speechBubbleGui(void* gui, void* bubble, const MsvcString* text, void* gameView, const void* wrapperStyle,
                      void* bubbleStyle) {
   speech::sayShown(view(*text));
   return g_speechBubbleOriginal(gui, bubble, text, gameView, wrapperStyle, bubbleStyle);
}

// The texts a manager's boxes show: window titles, labels and read-only text boxes, in order.
void collect(const Widget* widget, std::vector<const Widget*>& titles, std::vector<std::string>& texts, int depth) {
   if (depth > 40 || !agui::visible(widget)) return;
   Kind kind = agui::kind(widget);
   if (kind == Kind::Ignored) return;
   if (const Widget* title = agui::frameTitle(widget)) {
      titles.push_back(title);
      if (agui::visible(title) && !agui::text(title).empty()) texts.emplace_back(agui::text(title));
   }
   if (kind == Kind::Label && std::find(titles.begin(), titles.end(), widget) == titles.end()) {
      if (!agui::text(widget).empty()) texts.emplace_back(agui::text(widget));
   } else if (kind == Kind::TextBox && agui::readOnly(widget)) {
      if (!agui::textBoxText(widget).empty()) texts.emplace_back(agui::textBoxText(widget));
   }
   for (const Widget* child : agui::children(widget)) collect(child, titles, texts, depth + 1);
   for (const Widget* child : agui::privateChildren(widget)) collect(child, titles, texts, depth + 1);
}

// What each manager showed when its boxes last changed. MainLoop::prepare updates the managers
// on the main thread.
struct Shown {
   const void* manager;
   ptrdiff_t connectors;
   std::vector<std::string> texts;
};
std::vector<Shown> g_shown;

ptrdiff_t connectorBytes(const void* manager) {
   const auto* vector = static_cast<const std::byte*>(manager) + layout.infoBoxManagerConnectors;
   return at<const std::byte*>(vector, sizeof(void*)) - at<const std::byte*>(vector, 0);
}

using InfoBoxesUpdate = void (*)(void* manager);
InfoBoxesUpdate g_infoBoxesOriginal = nullptr;

// A box's text is set when it is made, before the manager lays it out. Only the texts that are
// new since the boxes last changed are spoken: a countdown that ticks while nothing else changes
// is not read again each second, and a box shown again later (the next autosave) is.
void infoBoxesUpdate(void* manager) {
   bool added = at<bool>(manager, layout.infoBoxManagerRebuild);
   g_infoBoxesOriginal(manager);
   auto shown =
      std::find_if(g_shown.begin(), g_shown.end(), [&](const Shown& entry) { return entry.manager == manager; });
   if (shown == g_shown.end()) shown = g_shown.insert(g_shown.end(), {manager, 0, {}});
   ptrdiff_t connectors = connectorBytes(manager);
   if (!added && connectors == shown->connectors) return;
   shown->connectors = connectors;
   std::vector<const Widget*> titles;
   std::vector<std::string> texts;
   collect(reinterpret_cast<const Widget*>(static_cast<const std::byte*>(manager) + layout.infoBoxManagerFrame), titles,
           texts, 0);
   for (const std::string& text : texts)
      if (std::find(shown->texts.begin(), shown->texts.end(), text) == shown->texts.end()) speech::sayShown(text);
   shown->texts = std::move(texts);
}

} // namespace

void* tipDetour() { return reinterpret_cast<void*>(&tipButton); }
void** tipOriginal() { return reinterpret_cast<void**>(&g_tipOriginal); }
void* speechBubbleDetour() { return reinterpret_cast<void*>(&speechBubbleGui); }
void** speechBubbleOriginal() { return reinterpret_cast<void**>(&g_speechBubbleOriginal); }
void* infoBoxesDetour() { return reinterpret_cast<void*>(&infoBoxesUpdate); }
void** infoBoxesOriginal() { return reinterpret_cast<void**>(&g_infoBoxesOriginal); }

} // namespace fa::popups
