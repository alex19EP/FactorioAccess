#pragma once

#include <span>
#include <string>
#include <string_view>

// Read-only views of Factorio's agui widgets. Only call these on the game's main thread while the
// widget tree is not being changed, i.e. from inside a hooked agui::Gui method.
namespace fa::agui {

struct Gui;    // agui::Gui
struct Widget; // agui::Widget or any subclass

const Gui* instance(); // agui::Gui::instance
const Widget* baseWidget(const Gui* gui);
const Widget* focusedWidget(const Gui* gui);
const Widget* widgetUnderMouse(const Gui* gui);

const Widget* parent(const Widget* widget);
std::span<const Widget* const> children(const Widget* widget);
// Children a widget manages itself rather than through add(), e.g. a Frame's content layout.
std::span<const Widget* const> privateChildren(const Widget* widget);
// Widget::text, or for anything derived from agui::Label its caption.
std::string_view text(const Widget* widget);
bool visible(const Widget* widget);
bool enabled(const Widget* widget);

// Fully qualified C++ class from RTTI, e.g. "agui::TextButton" or "MainMenuGui".
const std::string& className(const Widget* widget);

} // namespace fa::agui
