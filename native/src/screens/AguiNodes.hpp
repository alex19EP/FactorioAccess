#pragma once

// The pieces every agui window screen is made of: nodes for single controls and texts, and a
// walker that declares a whole subtree generically, for the parts of a window no recipe needs to
// know about.
//
// The walker's reading of a subtree:
//   - controls map to roles through their agui base class (button, checkbox, dropdown, ...)
//   - a frame's title is the context its contents are announced in
//   - a horizontal flow or table row of single items is a row (Left/Right); a table's rows keep
//     the column on Up/Down
//   - a row of texts only is one line ("Map version: 2.0.72"), and a label leading a row with a
//     single control names that control
//   - a multi-line label reads a line per node
//
// Activation never touches the mouse: buttons, toggles, tabs and dropdowns are pressed the way the
// Gui presses the widget under a real click (agui::press), sliders step on the arrow keys through
// their own keyDown.

#include <functional>
#include <string>
#include <vector>

#include "agui.h"
#include "graph/GraphBuilder.hpp"

namespace fa::screens
{

/// The widget's own text, or else the first text among its visible descendants: a button often
/// carries its caption in a child label.
std::string OwnText(const agui::Widget* widget);

/// What names a control: its own text, or else its tooltip title (icon buttons have no text).
std::string NameOf(const agui::Widget* widget);

/// Whether the walker would declare anything for the widget: visible and not chrome.
bool Shows(const agui::Widget* widget);

/// Whether the widget would declare anything at all: a shown text or control somewhere in it.
bool HasContent(const agui::Widget* widget);

/// The node for one control. `name` reads the control's label; empty uses NameOf.
graph::NodeVtable ControlNode(const agui::Widget* widget, std::function<std::string()> name = {});

/// The node for a control named by the label beside it. The tooltip key reads both their tooltips:
/// the label's usually explains the setting.
graph::NodeVtable ControlNode(const agui::Widget* widget, const agui::Widget* label);

/// A read-only line of text, re-read live. `tag` is the widget scrolled into view when focused,
/// and its tooltip is what the tooltip key reads.
graph::NodeVtable TextNode(const agui::Widget* tag, std::function<std::string()> text);

/// Declares `widget`'s whole subtree into the builder's current stop. Keys start with `prefix`.
/// Widgets in `skip` are left out wherever they appear: a title already spoken as the context, a
/// part declared as a stop of its own.
void AddSubtree(graph::GraphBuilder& builder, const std::string& prefix, const agui::Widget* widget,
    std::vector<const agui::Widget*> skip = {});

/// Declares a single control, if it is visible. Returns whether it did.
bool AddControl(graph::GraphBuilder& builder, const std::string& key, const agui::Widget* widget,
    std::function<std::string()> name = {});

/// The visible children of a widget, public ones first.
std::vector<const agui::Widget*> VisibleChildren(const agui::Widget* widget);

/// The first visible descendant (or the widget itself) that derives from the named class.
const agui::Widget* FindDescendant(const agui::Widget* widget, std::string_view className);

/// Every visible descendant (or the widget itself) that derives from the named class, in order;
/// the search does not descend into a match.
std::vector<const agui::Widget*> FindAll(const agui::Widget* widget, std::string_view className);

/// A label's (or read-only text box's) text as one phrase, exactly as shown, its lines joined.
std::string LabelText(const agui::Widget* label);

/// Whether `widget` is `ancestor` or lies inside it.
bool Contains(const agui::Widget* ancestor, const agui::Widget* widget);

} // namespace fa::screens
