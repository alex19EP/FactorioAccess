#pragma once

// The pieces every agui window screen is made of: nodes for single controls and texts, and a
// walker that declares a whole subtree generically, for the parts of a window no recipe needs to
// know about.
//
// The walker's reading of a subtree:
//   - controls map to roles through their agui base class (button, checkbox, dropdown, ...)
//   - slot buttons (items, fluids, recipes, filters) say what they hold; progress bars their percent
//   - a frame's title is the context its contents are announced in
//   - a horizontal flow or table row of single items is a row (Left/Right); a table's rows keep
//     the column on Up/Down
//   - a row of texts only is one line ("Map version: 2.0.72"), and a label leading a row with a
//     single control names that control
//   - a multi-line label reads a line per node
//   - a switch with a label at each side (whitelist, blacklist) is one switch reading its side
//
// Activation never touches the mouse: buttons, toggles, tabs and dropdowns are pressed the way the
// Gui presses the widget under a real click (agui::press), sliders step on the arrow keys through
// their own keyDown.

#include <functional>
#include <string>
#include <unordered_map>
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

/// The node for a LabeledSwitch: its switch, its value the label of the side it is on
/// ("Whitelist, switch"). Enter flips it as a click does.
graph::NodeVtable LabeledSwitchNode(const agui::Widget* labeledSwitch);
/// A read-only line of text, re-read live. `tag` is the widget scrolled into view when focused,
/// and its tooltip is what the tooltip key reads.
graph::NodeVtable TextNode(const agui::Widget* tag, std::function<std::string()> text);

/// What another widget lends a node, so that one stop reads what belongs together on screen:
/// progress bars spoken after the node's value, each under an optional name ("productivity"),
/// while they show; and a button its activation presses instead, while that button is in the
/// window and enabled. A `label` names what the game shows unnamed: read before a node's own
/// text, or for a container the context of everything in it ("fuel", as focus enters its slots).
struct Attachment
{
    std::vector<std::pair<const agui::Widget*, std::string>> bars;
    const agui::Widget* press = nullptr;
    std::string label;
};
using Attachments = std::unordered_map<const agui::Widget*, Attachment>;

/// Declares `widget`'s whole subtree into the builder's current stop. Keys start with `prefix`.
/// Widgets in `skip` are left out wherever they appear: a title already spoken as the context, a
/// part declared as a stop of its own. A widget in `attachments` gets those parts, and the widgets
/// lent are left out too; a labelled container's nodes are declared in its context.
void AddSubtree(graph::GraphBuilder& builder, const std::string& prefix, const agui::Widget* widget,
    std::vector<const agui::Widget*> skip = {}, Attachments attachments = {});

/// A table of buttons as the game lays it out: a row of the grid per table row, keeping the column
/// on Up and Down. Cells for which `accept` is false (the fillers that end a subgroup's line) are
/// skipped, and a row of fillers alone is left out. Keys start with `prefix`.
void AddGrid(graph::GraphBuilder& builder, const std::string& prefix, const agui::Widget* table,
    const std::function<bool(const agui::Widget*)>& accept,
    const std::function<graph::NodeVtable(const agui::Widget*)>& node);

/// A line of a description (a LabelWithHoverableRichText) with the icons the game makes clickable in
/// it as links beside it: Right reaches them, Enter clicks one as the mouse would, the tooltip key
/// reads its tooltip. Returns false, declaring nothing, for a label without such icons.
bool AddLinkLine(graph::GraphBuilder& builder, const std::string& key, const agui::Widget* label);

/// Screen::TypingIn for a game window: whether `node` is an editable text field that has the game's
/// keyboard focus.
bool TypingInField(const graph::GraphNode& node);

/// Screen::OnCursorMoved for a game window: scrolls the node's widget into view, and gives a text
/// field the game's focus again so that typing goes to it.
void FollowCursor(const graph::GraphNode& node);

/// What a slot button shows: "iron plate 50", "rare iron plate 50", "water 1200", an empty slot's
/// filter or expected ingredient as "iron plate, empty", or "empty".
std::string SlotText(const agui::Widget* slot);

/// Speaks the game's own tooltip for the widget, as hovering shows it: its texts a line each.
void SpeakGameTooltip(const agui::Widget* widget);

/// The texts of a tooltip widget the game made, a line each.
std::string TooltipText(const agui::Widget* tooltip);

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
