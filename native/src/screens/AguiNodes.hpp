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
//   - vertical flows of texts side by side (FA's lane views) are columns: Up/Down walk a column,
//     Left/Right go to the same line of the next, and a column's first text heads it
//   - a switch with a label at each side (whitelist, blacklist) is one switch reading its side;
//     Enter steps it as a controller does, through the middle when it has one
//
// Activation never touches the mouse: buttons, toggles, tabs and dropdowns are pressed the way the
// Gui presses the widget under a real click (agui::press), sliders step on the arrow keys through
// their own keyDown.

#include <functional>
#include <optional>
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

/// Declares every label and control `widgets` show, however deeply wrapped, as items of the
/// builder's current row, in the order shown: labels side by side as one line, a label before a
/// value control naming it. For a row the game builds of panels (a schedule's wait condition), which
/// the walker would read top-down. Keys start with `prefix`; widgets in `skip` are left out. Every
/// item gets `reorder`, if given (NodeVtable::OnReorder): the row moves in its list.
void AddRowItems(graph::GraphBuilder& builder, const std::string& prefix,
    const std::vector<const agui::Widget*>& widgets, std::vector<const agui::Widget*> skip = {},
    std::function<bool(int by)> reorder = {});

/// What a row the game builds of panels shows, its labels and controls' names in order as one text:
/// to know the row again after the game rebuilt it elsewhere. Widgets in `skip` are left out.
std::string RowText(const agui::Widget* widget, std::vector<const agui::Widget*> skip = {});

/// A table of buttons as the game lays it out: a row of the grid per table row, keeping the column
/// on Up and Down. Cells for which `accept` is false (the fillers that end a subgroup's line) are
/// skipped, and a row of fillers alone is left out. Keys start with `prefix`.
void AddGrid(graph::GraphBuilder& builder, const std::string& prefix, const agui::Widget* table,
    const std::function<bool(const agui::Widget*)>& accept,
    const std::function<graph::NodeVtable(const agui::Widget*)>& node);

/// The choices of a SelectListGui<T> (a filter chooser, remote view's ghost cursor selection): a
/// row of its item group tabs, ended by its search button, then a grid of the selected group's
/// choices, one row per subgroup line as the game lays them out. A choice reads its name only.
/// Keys start with `prefix`.
void AddChoices(graph::GraphBuilder& builder, const std::string& prefix, const agui::Widget* list);

/// A line of a description (a LabelWithHoverableRichText) with the icons the game makes clickable in
/// it as links beside it: Right reaches them, Enter clicks one as the mouse would, the tooltip key
/// reads its tooltip. Returns false, declaring nothing, for a label without such icons.
bool AddLinkLine(graph::GraphBuilder& builder, const std::string& key, const agui::Widget* label);

/// A multi-line description (a LabelWithHoverableRichText), a node per line, each line with the
/// icons the game makes clickable in it as links beside it, as AddLinkLine has them. Keys start
/// with `prefix`.
void AddLinkLines(graph::GraphBuilder& builder, const std::string& prefix, const agui::Widget* label);

/// Screen::TypingIn for a game window: whether `node` is an editable text field that has the game's
/// keyboard focus.
bool TypingInField(const graph::GraphNode& node);

/// Screen::OnCursorMoved for a game window: scrolls the node's widget into view, and gives a text
/// field the game's focus again so that typing goes to it.
void FollowCursor(const graph::GraphNode& node);

/// What a slot button shows: "iron plate 50", "rare iron plate 50", "water 1200", an empty slot's
/// filter or expected ingredient as "iron plate, empty", or "empty". Nothing for a stack taken in
/// hand: the slot's node says "in hand" apart, so the live watch leaves it to the hand's own line.
std::string SlotText(const agui::Widget* slot);

/// Reads the game's own tooltip for the widget, as hovering shows it (ReadTooltip).
void SpeakGameTooltip(const agui::Widget* widget);

/// A line of a tooltip, and whether the game draws a line above it, starting a section.
struct TooltipLine
{
    std::string text;
    bool section = false;
};

/// The lines of a tooltip widget the game made, in the order shown: a text's lines, a row of texts
/// (an ingredient's count and name) as one, and the items of a table of icons (a recipe's total raw)
/// or a slot with a number drawn on it, a line each.
std::vector<TooltipLine> TooltipLines(const agui::Widget* tooltip);

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

/// The footer button a dialog window's button row is entered on, or null: the one E presses, as
/// a sighted player's eye goes to the green one; for a dialog E does not confirm (loading over
/// changed mods), the last of the footer's right-hand group, where the game puts its confirm
/// buttons, the main one last.
const agui::Widget* DialogConfirm(const agui::Widget* window);

/// Whether `widget` is `ancestor` or lies inside it.
bool Contains(const agui::Widget* ancestor, const agui::Widget* widget);

/// Where the cursor goes once the game shows a change: the node keyed `key`, at the first build
/// where `ready` holds (a section dragged to its new place, once the game has rebuilt the list).
using Lander = std::function<void(std::string key, std::function<bool()> ready)>;

/// A screen's landing for a change the game shows a frame or more after the key that asked for
/// it, given up after about two seconds.
class PendingLanding
{
public:
    void Set(std::string key, std::function<bool()> ready);
    void Reset() { _landing.reset(); }
    /// For Screen::TakeSuggestedLanding, after a build that found the screen's window still open.
    const char* Take();

private:
    struct Landing
    {
        std::string key;
        std::function<bool()> ready;
        int builds = 0; // left before it is given up
    };
    std::optional<Landing> _landing;
    std::string _landed; // what the last Take returned
};

/// The screen's lander while it builds, for the parts the walker hands off (a logistics panel);
/// empty outside a build or for a screen that does not land.
const Lander& BuildLander();

/// Sets BuildLander for the build it spans.
class ScopedBuildLander
{
public:
    explicit ScopedBuildLander(Lander land);
    ~ScopedBuildLander();
    ScopedBuildLander(const ScopedBuildLander&) = delete;
    ScopedBuildLander& operator=(const ScopedBuildLander&) = delete;

private:
    Lander _previous;
};

} // namespace fa::screens
