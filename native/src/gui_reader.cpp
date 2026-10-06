#include "gui_reader.h"

#include "agui.h"
#include "log.h"
#include "speech.h"
#include "text.h"

#include <windows.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fa::gui_reader {

namespace {

using agui::Widget;

constexpr int kMaxDepth = 32;

struct WindowKey {
   const Widget* widget;
   std::string className;

   bool operator==(const WindowKey&) const = default;
};

// What was last spoken for one Gui. Gui::logic runs for more than one Gui per frame, so each
// keeps its own state; a shared one would make every Gui re-announce what another just said.
struct GuiState {
   std::vector<WindowKey> windows;
   const Widget* focus = nullptr;
   const Widget* hover = nullptr;
};

bool g_disabled = false;
std::unordered_map<const agui::Gui*, GuiState> g_states;

// Public children first: for a Frame they hold the title row, while the content layout is private.
template <class Visit>
void forEachChild(const Widget* widget, Visit&& visit) {
   for (const Widget* child : agui::children(widget)) visit(child);
   for (const Widget* child : agui::privateChildren(widget)) visit(child);
}

std::string_view role(const std::string& className) {
   static const std::pair<std::string_view, std::string_view> roles[] = {
      {"agui::Button", "button"},          {"agui::TextButton", "button"},
      {"agui::ToggleButton", "toggle"},    {"IconButton", "button"},
      {"agui::CheckBox", "checkbox"},      {"agui::RadioButton", "radio button"},
      {"agui::TextBox", "edit"},           {"agui::TextField", "edit"},
      {"agui::DropDown", "dropdown"},      {"agui::ListBox", "list"},
      {"agui::Slider", "slider"},          {"agui::Tab", "tab"},
      {"agui::Switch", "switch"},
   };
   for (const auto& [name, spoken] : roles)
      if (className == name) return spoken;
   return {};
}

// The widget's own text, or else the first text among its visible descendants: a button often
// carries its caption in a child label.
std::string ownText(const Widget* widget, int depth = 0) {
   std::string spoken = text::speakable(agui::text(widget));
   if (!spoken.empty() || depth >= 4) return spoken;
   forEachChild(widget, [&](const Widget* child) {
      if (spoken.empty() && agui::visible(child)) spoken = ownText(child, depth + 1);
   });
   return spoken;
}

std::string describe(const Widget* widget) {
   std::string spoken = ownText(widget);
   std::string_view widgetRole = role(agui::className(widget));
   if (!widgetRole.empty()) {
      if (!spoken.empty()) spoken += ' ';
      spoken += widgetRole;
   }
   if (!spoken.empty() && !agui::enabled(widget)) spoken += " disabled";
   return spoken;
}

void collectText(const Widget* widget, std::vector<std::string>& out, int depth) {
   if (!agui::visible(widget) || depth > kMaxDepth) return;
   std::string spoken = text::speakable(agui::text(widget));
   if (!spoken.empty() && (out.empty() || out.back() != spoken)) out.push_back(std::move(spoken));
   forEachChild(widget, [&](const Widget* child) { collectText(child, out, depth + 1); });
}

void dumpTree(const Widget* widget, int depth) {
   log::info("{:{}}{} {}{} \"{}\"", "", depth * 2, agui::className(widget), static_cast<const void*>(widget),
             agui::visible(widget) ? "" : " hidden", agui::text(widget));
   if (depth >= kMaxDepth) return;
   forEachChild(widget, [&](const Widget* child) { dumpTree(child, depth + 1); });
}

void announceWindow(const Widget* window) {
   log::info("Window shown: {}", agui::className(window));
   dumpTree(window, 1);
   std::vector<std::string> texts;
   collectText(window, texts, 0);
   std::string spoken;
   for (const auto& part : texts) {
      if (!spoken.empty()) spoken += ", ";
      spoken += part;
   }
   speech::say(std::move(spoken), false);
}

void tick(const agui::Gui* gui) {
   auto [it, firstSeen] = g_states.try_emplace(gui);
   GuiState& state = it->second;
   if (firstSeen) log::info("Gui {} (Gui::instance is {})", static_cast<const void*>(gui), static_cast<const void*>(agui::instance()));

   const Widget* root = agui::baseWidget(gui);
   if (!root) return;

   std::vector<WindowKey> windows;
   for (const Widget* child : agui::children(root))
      if (agui::visible(child)) windows.push_back({child, agui::className(child)});
   for (const auto& window : windows)
      if (std::ranges::find(state.windows, window) == state.windows.end()) announceWindow(window.widget);
   state.windows = std::move(windows);

   if (const Widget* focus = agui::focusedWidget(gui); focus != state.focus) {
      state.focus = focus;
      if (focus) speech::say(describe(focus), true);
   }
   if (const Widget* hover = agui::widgetUnderMouse(gui); hover != state.hover) {
      state.hover = hover;
      if (hover) speech::say(describe(hover), true);
   }
}

// A wrong offset or a widget freed under us shows up as an access violation. Stop reading rather
// than take the game down; nothing here may unwind C++ objects, so the work lives in tick().
bool guardedTick(const agui::Gui* gui) {
   __try {
      tick(gui);
      return true;
   } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER
                                                                 : EXCEPTION_CONTINUE_SEARCH) {
      return false;
   }
}

} // namespace

void afterGuiLogic(const agui::Gui* gui) {
   if (g_disabled) return;
   if (!guardedTick(gui)) {
      g_disabled = true;
      log::error("Access violation while reading the widget tree; the GUI reader is off until restart");
      speech::say("FactorioAccess native GUI reader failed and is now off", false);
   }
}

} // namespace fa::gui_reader
