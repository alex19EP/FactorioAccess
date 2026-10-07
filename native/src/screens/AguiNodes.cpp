#include "AguiNodes.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "GuiDump.hpp"
#include "log.h"
#include "speech.h"
#include "text.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Kind;
using agui::Widget;
using graph::AnnouncementKinds::Enabled;
using graph::AnnouncementKinds::Label;
using graph::AnnouncementKinds::Role;
using graph::AnnouncementKinds::Selected;
using graph::AnnouncementKinds::Value;

constexpr int kMaxDepth = 40;

graph::ControlType MakeType(std::string key, std::string_view role)
{
    graph::ControlType type;
    type.Key = std::move(key);
    type.Order = {Label, Value, Selected, Role, Enabled};
    type.Common = [role]()
    { return std::vector<graph::NodeAnnouncement>{{[role]() { return std::string(role); }, false, Role}}; };
    return type;
}

const graph::ControlType* TypeOf(Kind kind)
{
    static const std::unordered_map<Kind, graph::ControlType> types = {
        {Kind::Button, MakeType("button", vocab::kButton)},
        {Kind::CheckBox, MakeType("checkbox", vocab::kCheckBox)},
        {Kind::RadioButton, MakeType("radio", vocab::kRadioButton)},
        {Kind::DropDown, MakeType("dropdown", vocab::kDropDown)},
        {Kind::Slider, MakeType("slider", vocab::kSlider)},
        {Kind::Switch, MakeType("switch", vocab::kSwitch)},
        {Kind::TextBox, MakeType("edit", vocab::kEdit)},
        {Kind::Tab, MakeType("tab", vocab::kTab)},
        {Kind::ProgressBar, MakeType("progressbar", vocab::kProgressBar)},
    };
    auto it = types.find(kind);
    return it == types.end() ? nullptr : &it->second;
}

bool IsControl(Kind kind) { return TypeOf(kind) != nullptr; }

// Controls whose own text is their value rather than their name; a label before one names it.
bool ShowsValue(Kind kind)
{
    return kind == Kind::DropDown || kind == Kind::Slider || kind == Kind::Switch || kind == Kind::TextBox
        || kind == Kind::ProgressBar;
}

template <class Visit>
void ForEachChild(const Widget* widget, Visit&& visit)
{
    // Public children first: for a Frame they hold the header row, while the content is private.
    int index = 0;
    for (const Widget* child : agui::children(widget))
        visit(child, std::to_string(index++));
    index = 0;
    for (const Widget* child : agui::privateChildren(widget))
        visit(child, "p" + std::to_string(index++));
}

// The kind as read aloud: a read-only text box (a dialog's message) is text, like a label.
Kind KindOf(const Widget* widget)
{
    Kind kind = agui::kind(widget);
    return kind == Kind::TextBox && agui::readOnly(widget) ? Kind::Label : kind;
}

// What a text widget shows: a label's caption or a text box's content.
std::string_view TextOf(const Widget* widget)
{
    return agui::kind(widget) == Kind::TextBox ? agui::textBoxText(widget) : agui::text(widget);
}

std::string OwnTextAt(const Widget* widget, int depth)
{
    std::string spoken = text::speakable(agui::text(widget));
    if (!spoken.empty() || depth >= 4)
        return spoken;
    ForEachChild(widget,
        [&](const Widget* child, const std::string&)
        {
            if (spoken.empty() && Shows(child))
                spoken = OwnTextAt(child, depth + 1);
        });
    return spoken;
}

// A text's non-empty lines, each made speakable. Long texts (the changelog) read a line per
// arrow press rather than as one utterance minutes long.
std::vector<std::string> Lines(std::string_view raw)
{
    std::vector<std::string> lines;
    while (!raw.empty())
    {
        std::size_t end = raw.find('\n');
        std::string line = text::speakable(raw.substr(0, end));
        if (!line.empty())
            lines.push_back(std::move(line));
        if (end == std::string_view::npos)
            break;
        raw.remove_prefix(end + 1);
    }
    return lines;
}

// A text's content as one phrase, its lines joined. Everything shown is kept, bullets and colons
// included: a sighted player sees them.
std::string Phrase(const Widget* label)
{
    std::string phrase;
    for (const std::string& line : Lines(TextOf(label)))
    {
        if (!phrase.empty())
            phrase += ' ';
        phrase += line;
    }
    return phrase;
}

bool Contains(std::span<const Widget* const> widgets, const Widget* widget)
{
    return std::ranges::find(widgets, widget) != widgets.end();
}

// How many nodes a widget's subtree declares, counted up to `limit`, leaving out `skip`.
int Navigable(const Widget* widget, int limit, std::span<const Widget* const> skip = {}, int depth = 0)
{
    if (depth > kMaxDepth || !Shows(widget) || Contains(skip, widget))
        return 0;
    Kind kind = KindOf(widget);
    if (kind == Kind::Label)
        return Phrase(widget).empty() ? 0 : static_cast<int>(std::min<std::size_t>(Lines(TextOf(widget)).size(), limit));
    if (IsControl(kind))
        return 1;
    int count = 0;
    ForEachChild(widget,
        [&](const Widget* child, const std::string&)
        {
            if (count < limit)
                count += Navigable(child, limit - count, skip, depth + 1);
        });
    return count;
}

// At most one node: a single control or label, however deeply wrapped.
bool IsSimple(const Widget* widget, std::span<const Widget* const> skip = {})
{
    return Navigable(widget, 2, skip) <= 1;
}

// The one label or control a simple widget holds, or null when it holds nothing.
const Widget* LeafOf(const Widget* widget, std::span<const Widget* const> skip = {}, int depth = 0)
{
    if (depth > kMaxDepth || !Shows(widget) || Contains(skip, widget))
        return nullptr;
    Kind kind = KindOf(widget);
    if (kind == Kind::Label)
        return Phrase(widget).empty() ? nullptr : widget;
    if (IsControl(kind))
        return widget;
    const Widget* leaf = nullptr;
    ForEachChild(widget,
        [&](const Widget* child, const std::string&)
        {
            if (!leaf)
                leaf = LeafOf(child, skip, depth + 1);
        });
    return leaf;
}

std::string CheckWord(const Widget* widget)
{
    switch (agui::checkState(widget))
    {
    case agui::CheckState::Checked:
        return std::string(vocab::kChecked);
    case agui::CheckState::Intermediate:
        return std::string(vocab::kPartlyChecked);
    default:
        return std::string(vocab::kUnchecked);
    }
}

// The state a control reports in its Value part, or empty when the kind has none.
std::string ValueText(const Widget* widget, Kind kind)
{
    switch (kind)
    {
    case Kind::CheckBox:
        return CheckWord(widget);
    case Kind::Button:
        return agui::buttonToggled(widget) ? std::string(vocab::kPressed) : std::string();
    case Kind::DropDown:
        return text::speakable(agui::text(widget));
    case Kind::Slider:
    {
        // The game shows a slider's value, formatted ("100%"), as its tooltip.
        std::string shown = text::speakable(agui::toolTip(widget).title);
        return shown.empty() ? std::format("{:g}", agui::sliderValue(widget).value) : shown;
    }
    case Kind::Switch:
        switch (agui::switchState(widget))
        {
        case agui::SwitchState::Left:
            return std::string(vocab::kLeft);
        case agui::SwitchState::Right:
            return std::string(vocab::kRight);
        default:
            return {};
        }
    case Kind::TextBox:
    {
        std::string content = text::speakable(agui::textBoxText(widget));
        if (content.empty())
            content = vocab::kBlank;
        if (agui::readOnly(widget))
            content += ", " + std::string(vocab::kReadOnly);
        return content;
    }
    case Kind::ProgressBar:
        return std::format("{}%", std::lround(agui::progress(widget) * 100));
    default:
        return {};
    }
}

std::string SelectedText(const Widget* widget, Kind kind)
{
    bool selected = kind == Kind::RadioButton ? agui::checkState(widget) == agui::CheckState::Checked
        : kind == Kind::Tab                   ? agui::tabSelected(widget)
                                              : false;
    return selected ? std::string(vocab::kSelected) : std::string();
}

// The tooltip text, and its title too when that is not already the control's name.
std::string ToolTipText(const Widget* widget)
{
    // A slider's tooltip is its value, spoken already.
    if (agui::kind(widget) == Kind::Slider)
        return {};
    agui::ToolTip tip = agui::toolTip(widget);
    std::string title = text::speakable(tip.title);
    std::string body = text::speakable(tip.text);
    if (title == OwnText(widget))
        title.clear();
    if (!title.empty() && !body.empty())
        return title + ", " + body;
    return title.empty() ? body : title;
}

// The tooltips of the widgets that make up one node (a control and the label naming it), joined.
std::string ToolTipsOf(const std::vector<const Widget*>& widgets)
{
    std::string spoken;
    for (const Widget* widget : widgets)
    {
        std::string tip = ToolTipText(widget);
        if (tip.empty())
            continue;
        if (!spoken.empty())
            spoken += ", ";
        spoken += tip;
    }
    return spoken;
}

// Y reads the tooltips, when any of the widgets has one.
void SetTooltip(graph::NodeVtable& vtable, std::vector<const Widget*> widgets)
{
    if (ToolTipsOf(widgets).empty())
        return;
    vtable.OnTooltip = [widgets = std::move(widgets)]() { speech::say(ToolTipsOf(widgets), true); };
}

// A table's cells in order, blanks included: an EmptyWidget holds its place in the grid.
std::vector<const Widget*> TableCells(const Widget* table)
{
    std::vector<const Widget*> cells;
    for (const Widget* child : agui::children(table))
        if (agui::visible(child))
            cells.push_back(child);
    return cells;
}

// The names of the icons a cell shows (a table of planet icons): "Nauvis, Vulcanus".
std::string IconNames(const Widget* cell)
{
    std::string names;
    for (const Widget* icon : agui::children(cell))
    {
        if (!agui::visible(icon))
            continue;
        std::string name = text::speakable(agui::iconName(icon));
        if (name.empty())
            continue;
        if (!names.empty())
            names += ", ";
        names += name;
    }
    return names;
}

// A cell made only of icons.
bool IsIconCell(const Widget* cell)
{
    if (agui::kind(cell) != Kind::Table)
        return false;
    std::vector<const Widget*> icons = TableCells(cell);
    return !icons.empty()
        && std::ranges::all_of(icons, [](const Widget* icon) { return agui::derivesFrom(icon, "IconButton"); });
}

// A table whose first row is headers (labels and blanks) above rows holding controls: a grid of
// settings, each cell named by its column's header.
bool IsHeaderGrid(const std::vector<const Widget*>& cells, std::size_t columns)
{
    if (columns < 2 || cells.size() < 2 * columns)
        return false;
    for (std::size_t i = 0; i < columns; ++i)
    {
        Kind kind = KindOf(cells[i]);
        if (kind != Kind::Label && kind != Kind::Ignored)
            return false;
    }
    return std::any_of(cells.begin() + columns, cells.end(),
        [](const Widget* cell)
        {
            const Widget* leaf = IsSimple(cell) ? LeafOf(cell) : nullptr;
            return leaf && IsControl(KindOf(leaf));
        });
}

// A slider whose value a field beside it shows, and takes typed: the field's text is the value,
// Left/Right step the slider, Enter puts the game's focus in the field for typing.
graph::NodeVtable SliderWithField(const Widget* slider, const Widget* field, const Widget* label)
{
    graph::NodeVtable vtable = ControlNode(slider, label);
    auto shown = [field]() { return text::speakable(agui::textBoxText(field)); };
    for (graph::NodeAnnouncement& part : vtable.Announcements)
        if (part.Kind == Value)
            part.Text = shown;
    vtable.StateText = shown;
    vtable.OnActivate = [field]() { agui::focus(field); };
    return vtable;
}

// Declares a window part's nodes. Rows open only around items that exist, so a flow or table
// row that turns out to hold nothing never reaches the builder.
class Walker
{
public:
    Walker(graph::GraphBuilder& builder, std::string prefix, std::vector<const Widget*> skip, Attachments attachments)
        : _builder(builder)
        , _prefix(std::move(prefix))
        , _skip(std::move(skip))
        , _attachments(std::move(attachments))
    {
        for (const auto& [host, attachment] : _attachments)
        {
            for (const auto& [bar, name] : attachment.bars)
                _skip.push_back(bar);
            if (attachment.press)
                _skip.push_back(attachment.press);
        }
    }

    void Visit(const Widget* widget, const std::string& path, int depth)
    {
        if (depth > kMaxDepth || Skipped(widget) || !Shows(widget))
            return;
        Kind kind = KindOf(widget);
        switch (kind)
        {
        case Kind::Label:
        {
            std::vector<std::string> lines = Lines(TextOf(widget));
            if (lines.size() > 1)
            {
                for (std::size_t i = 0; i < lines.size(); ++i)
                    EmitLine(widget, path, i);
                return;
            }
            if (!Phrase(widget).empty())
                Add(Key(path), TextNode(widget, [widget]() { return Phrase(widget); }));
            return;
        }
        case Kind::Table:
        {
            std::vector<const Widget*> cells = TableCells(widget);
            std::size_t columns = std::max(agui::tableColumns(widget), 1u);
            if (IsHeaderGrid(cells, columns))
            {
                Grid(cells, columns, path);
                return;
            }
            // A table of whole panels is a layout grid, not rows of items: read it top-down.
            if (!std::ranges::all_of(cells, [this](const Widget* cell) { return IsSimple(cell, _skip); }))
            {
                for (std::size_t i = 0; i < cells.size(); ++i)
                    Visit(cells[i], path + "." + std::to_string(i), depth + 1);
                return;
            }
            for (std::size_t row = 0; row * columns < cells.size(); ++row)
            {
                std::size_t end = std::min(cells.size(), (row + 1) * columns);
                Row({cells.begin() + row * columns, cells.begin() + end}, path + "." + std::to_string(row),
                    Key(path));
            }
            return;
        }
        case Kind::HorizontalFlow:
        {
            // Only a strip of single items is a row; panels laid out side by side (a list beside
            // its details) stay vertical, each read top-down.
            std::vector<const Widget*> items;
            bool row = true;
            ForEachChild(widget,
                [&](const Widget* child, const std::string&)
                {
                    if (Skipped(child) || !Shows(child))
                        return;
                    row = row && IsSimple(child, _skip);
                    items.push_back(child);
                });
            if (row)
            {
                Row(items, path, "");
                return;
            }
            break;
        }
        default:
            break;
        }

        if (IsControl(kind))
        {
            Add(Key(path), ControlNode(widget));
            return;
        }

        // A container: a frame's title is the context its contents are announced in.
        const Widget* title = agui::frameTitle(widget);
        std::string titleText = title && Shows(title) ? Phrase(title) : std::string();
        if (!titleText.empty())
            _builder.PushContext(titleText);
        ForEachChild(widget,
            [&](const Widget* child, const std::string& segment)
            {
                if (child != title)
                    Visit(child, path.empty() ? segment : path + "." + segment, depth + 1);
            });
        if (!titleText.empty())
            _builder.PopContext();
    }

private:
    // One row of simple items.
    void Row(const std::vector<const Widget*>& items, const std::string& path, const std::string& rowKey)
    {
        std::vector<const Widget*> leaves;
        for (const Widget* item : items)
            if (const Widget* leaf = LeafOf(item, _skip))
                leaves.push_back(leaf);
        if (leaves.empty())
            return;

        auto isLabel = [](const Widget* leaf) { return KindOf(leaf) == Kind::Label; };
        std::string key = Key(path) + "#row";
        if (std::ranges::all_of(leaves, isLabel))
        {
            // "Map version:" "2.0.72": one line, as it reads on screen.
            graph::NodeVtable node = TextNode(leaves.front(),
                         [leaves]()
                         {
                             std::string line;
                             for (const Widget* leaf : leaves)
                             {
                                 std::string phrase = Phrase(leaf);
                                 if (phrase.empty())
                                     continue;
                                 if (!line.empty())
                                     line += ' ';
                                 line += phrase;
                             }
                             return line;
                         });
            SetTooltip(node, leaves);
            Add(key, std::move(node));
            return;
        }
        if (leaves.size() == 3 && isLabel(leaves[0]) && agui::kind(leaves[1]) == Kind::Slider
            && agui::kind(leaves[2]) == Kind::TextBox && !agui::readOnly(leaves[2]))
        {
            // A setting with a slider and a field showing its value: one control.
            Add(key, SliderWithField(leaves[1], leaves[2], leaves[0]));
            return;
        }
        if (leaves.size() == 2 && isLabel(leaves[0]))
        {
            // A label and the one control it describes: "Website, https://..., button".
            Add(key, ControlNode(leaves[1], leaves[0]));
            return;
        }
        if (leaves.size() == 1)
        {
            Add(key, ControlNode(leaves[0]));
            return;
        }

        _builder.StartRow(rowKey);
        for (std::size_t i = 0; i < leaves.size(); ++i)
        {
            const Widget* leaf = leaves[i];
            std::string cellKey = key + "." + std::to_string(i);
            if (isLabel(leaf))
            {
                // A label right before a value control is that control's name.
                if (i + 1 < leaves.size() && ShowsValue(agui::kind(leaves[i + 1])))
                    continue;
                Add(cellKey, TextNode(leaf, [leaf]() { return Phrase(leaf); }));
                continue;
            }
            if (i > 0 && isLabel(leaves[i - 1]) && ShowsValue(agui::kind(leaf)))
            {
                Add(cellKey, ControlNode(leaf, leaves[i - 1]));
                continue;
            }
            Add(cellKey, ControlNode(leaf));
        }
        _builder.EndRow();
    }

    // A header grid (IsHeaderGrid): a row per item, in a context naming it (with the icons it
    // shows, which tell apart rows of the same name), and a cell per setting named by its column
    // header. Rows share a key, so Up/Down keep the column. A slider among the cells adjusts only
    // after Enter, as Left/Right move between the cells.
    void Grid(const std::vector<const Widget*>& cells, std::size_t columns, const std::string& path)
    {
        for (std::size_t first = columns; first < cells.size(); first += columns)
        {
            std::size_t end = std::min(cells.size(), first + columns);
            std::string rowKey = Key(path) + "#" + std::to_string(first / columns);
            const Widget* nameCell = cells[first];
            std::string rowName = KindOf(nameCell) == Kind::Label ? Phrase(nameCell) : OwnText(nameCell);
            for (std::size_t c = first + 1; c < end; ++c)
                if (IsIconCell(cells[c]))
                    if (std::string icons = IconNames(cells[c]); !icons.empty())
                        rowName += ", " + icons;

            _builder.PushContext(rowName, "", /*positions*/ false);
            bool open = false;
            for (std::size_t c = first; c < end; ++c)
            {
                const Widget* cell = cells[c];
                const Widget* header = cells[c - first];
                std::optional<graph::NodeVtable> node;
                if (c == first)
                {
                    // The row's own name is its context; its checkbox only says its state. A row
                    // named by a plain label still gets this cell, so every row keeps its columns
                    // at the same positions for Up/Down.
                    if (KindOf(cell) == Kind::CheckBox)
                        node = ControlNode(cell, []() { return std::string(); });
                    else if (KindOf(cell) == Kind::Label)
                    {
                        node = TextNode(cell, [cell]() { return Phrase(cell); });
                        SetTooltip(*node, {cell});
                    }
                }
                else if (IsIconCell(cell))
                {
                    node = TextNode(cell,
                        [header, cell]()
                        {
                            std::string icons = IconNames(cell);
                            std::string name = Phrase(header);
                            return name.empty() || icons.empty() ? name + icons : name + ", " + icons;
                        });
                    SetTooltip(*node, {header});
                }
                else if (const Widget* leaf = IsSimple(cell) ? LeafOf(cell) : nullptr)
                {
                    if (IsControl(KindOf(leaf)))
                    {
                        node = ControlNode(leaf, header);
                        node->AdjustOnEnter = agui::kind(leaf) == Kind::Slider;
                    }
                    else
                    {
                        node = TextNode(leaf, [header, leaf]() { return Phrase(header) + ", " + Phrase(leaf); });
                        SetTooltip(*node, {header, leaf});
                    }
                }
                if (!node)
                    continue;
                if (!open)
                {
                    _builder.StartRow(Key(path));
                    open = true;
                }
                Add(rowKey + "." + std::to_string(c - first), std::move(*node));
            }
            if (open)
                _builder.EndRow();
            _builder.PopContext();
        }
    }

    // One line of a multi-line label, re-read live like any other label.
    void EmitLine(const Widget* label, const std::string& path, std::size_t index)
    {
        Add(Key(path) + "#" + std::to_string(index),
            TextNode(label,
                [label, index]()
                {
                    std::vector<std::string> lines = Lines(TextOf(label));
                    return index < lines.size() ? lines[index] : std::string();
                }));
    }

    std::string Key(const std::string& path) const { return _prefix + "/" + path; }

    bool Skipped(const Widget* widget) const { return Contains(_skip, widget); }

    void Add(const std::string& key, graph::NodeVtable vtable)
    {
        auto* widget = static_cast<const Widget*>(vtable.HostTag);
        if (auto it = _attachments.find(widget); it != _attachments.end())
            Attach(vtable, it->second);
        // Each node in its labelled container's context: the cells of one row can belong to
        // different ones (a furnace's input slot, then its output slot).
        std::string context = ContainerLabel(widget);
        if (!context.empty())
            _builder.PushContext(context);
        _builder.AddItem(graph::ControlId::Referenced(widget, key), std::move(vtable));
        if (!context.empty())
            _builder.PopContext();
    }

    // The label of the nearest labelled container around a widget, or empty.
    std::string ContainerLabel(const Widget* widget) const
    {
        for (const Widget* ancestor = widget ? agui::parent(widget) : nullptr; ancestor;
             ancestor = agui::parent(ancestor))
            if (auto it = _attachments.find(ancestor); it != _attachments.end() && !it->second.label.empty())
                return it->second.label;
        return {};
    }

    static void Attach(graph::NodeVtable& vtable, const Attachment& attachment)
    {
        if (!attachment.label.empty())
            vtable.Announcements.insert(vtable.Announcements.begin(),
                graph::NodeAnnouncement([label = attachment.label]() { return label; }, false, Label));

        // Right after the node's own value; a typed node sorts its parts by kind anyway.
        auto at = std::ranges::find_if(vtable.Announcements,
            [](const graph::NodeAnnouncement& part) { return part.Kind != Label && part.Kind != Value; });
        std::vector<graph::NodeAnnouncement> bars;
        for (const auto& [bar, name] : attachment.bars)
            // Read when asked for, not watched: a bar moves every tick.
            bars.emplace_back(
                [bar, name]()
                {
                    if (!agui::visible(bar))
                        return std::string();
                    std::string value = ValueText(bar, Kind::ProgressBar);
                    return name.empty() ? value : name + " " + value;
                },
                false, Value);
        vtable.Announcements.insert(at, bars.begin(), bars.end());

        if (const Widget* button = attachment.press)
            vtable.OnActivate = [button, own = std::move(vtable.OnActivate)]()
            {
                if (agui::parent(button) && agui::visible(button) && agui::enabled(button))
                    agui::press(button, agui::MouseButton::Left, false, false);
                else if (own)
                    own();
            };
    }

    graph::GraphBuilder& _builder;
    std::string _prefix;
    std::vector<const Widget*> _skip;
    Attachments _attachments;
};

} // namespace

std::string OwnText(const Widget* widget) { return OwnTextAt(widget, 0); }

std::string NameOf(const Widget* widget)
{
    // A dropdown's text is its value, which it says as such; only a tooltip can name it.
    std::string name = agui::kind(widget) == Kind::DropDown ? std::string() : OwnText(widget);
    // A slider's tooltip is its value, not its name.
    if (name.empty() && agui::kind(widget) != Kind::Slider)
        name = text::speakable(agui::toolTip(widget).title);
    // A title bar's X: no text, no tooltip.
    if (name.empty() && agui::derivesFrom(widget, "CloseButton"))
        name = vocab::kClose;
    return name;
}

bool Shows(const Widget* widget) { return agui::visible(widget) && agui::kind(widget) != Kind::Ignored; }

bool HasContent(const Widget* widget) { return Navigable(widget, 1) > 0; }

std::string SlotText(const Widget* slot)
{
    std::string_view name;
    std::string_view quality;
    double count = 0;
    if (agui::derivesFrom(slot, "InventoryGuiSlot"))
    {
        agui::SlotItem item = agui::slotItem(slot);
        name = item.name;
        quality = item.quality;
        count = item.count;
    }
    // An empty item slot can still name its filter, or the ingredient a machine expects in it.
    if (count == 0)
    {
        agui::SlotButton button = agui::slotButton(slot);
        name = button.name;
        quality = button.quality;
        count = button.count;
    }
    if (name.empty())
        return std::string(vocab::kEmpty);
    std::string spoken = quality.empty() ? std::string(name) : std::format("{} {}", quality, name);
    if (count == 0)
        return std::format("{}, {}", spoken, vocab::kEmpty);
    // Fluid amounts are fractional; the game rounds what it draws too.
    if (count >= 100 || count == std::floor(count))
        return std::format("{} {:.0f}", spoken, count);
    return std::format("{} {:.1f}", spoken, count);
}

void SpeakGameTooltip(const Widget* widget)
{
    bool created = false;
    const Widget* tooltip = agui::showTooltip(widget, created);
    std::string text;
    // The tooltip itself stays hidden until the Gui places it; its content shows already.
    if (tooltip)
        ForEachChild(tooltip, [&](const Widget* child, const std::string&) {
            for (const Widget* label : FindAll(child, "agui::Label"))
                if (std::string line = LabelText(label); !line.empty())
                    text += (text.empty() ? "" : "\n") + line;
        });
    if (text.empty())
        log::info("No tooltip text on {}: {}", agui::className(widget),
            tooltip ? DescribeTree(tooltip) : std::string("none created"));
    if (created)
        agui::removeTooltip(widget);
    speech::say(text.empty() ? std::string(vocab::kNoTooltip) : text, true);
}

namespace
{

// A slot is named by what it holds; a label beside it ("Fuel") is its name instead, the content
// its value. Clicked as the Gui clicks the button under the mouse, so taking, placing and
// splitting stacks and choosing recipes are vanilla.
graph::NodeVtable SlotNode(const Widget* slot, std::function<std::string()> name)
{
    graph::NodeVtable vtable;
    vtable.HostTag = slot;
    // A fluid's amount changes every tick; watching it would talk over everything.
    bool live = !agui::derivesFrom(slot, "FluidBoxGuiSlot");
    auto content = [slot]() { return SlotText(slot); };
    if (name)
    {
        vtable.Announcements.emplace_back(std::move(name), false, Label);
        vtable.Announcements.emplace_back(content, live, Value);
    }
    else
        vtable.Announcements.emplace_back(content, live, Label);
    vtable.Announcements.emplace_back(
        [slot]() { return agui::enabled(slot) ? std::string() : std::string(vocab::kDisabled); }, false, Enabled);
    vtable.OnActivate = [slot]() { agui::press(slot, agui::MouseButton::Left, false, false); };
    vtable.OnActivateShift = [slot]() { agui::press(slot, agui::MouseButton::Left, true, false); };
    vtable.OnActivateCtrl = [slot]() { agui::press(slot, agui::MouseButton::Left, false, true); };
    vtable.OnSecondary = [slot]() { agui::press(slot, agui::MouseButton::Right, false, false); };
    vtable.OnTertiary = [slot]() { agui::press(slot, agui::MouseButton::Middle, false, false); };
    vtable.OnTooltip = [slot]() { SpeakGameTooltip(slot); };
    // No StateText: a click reaches the game as an input action applied later, so the result is
    // spoken by the live watch once it shows.
    return vtable;
}

} // namespace

graph::NodeVtable ControlNode(const Widget* widget, std::function<std::string()> name)
{
    if (agui::isSlotButton(widget))
        return SlotNode(widget, std::move(name));
    Kind kind = agui::kind(widget);
    if (!name)
        name = [widget, kind]()
        {
            // A bar's caption is mostly its percentage, which is already its value.
            std::string own = NameOf(widget);
            return kind == Kind::ProgressBar && own == ValueText(widget, kind) ? std::string() : own;
        };

    graph::NodeVtable vtable;
    vtable.Type = TypeOf(kind);
    vtable.HostTag = widget;
    vtable.Announcements.emplace_back(std::move(name), false, Label);
    // A progress bar moves every tick; it is read when asked for, not watched.
    vtable.Announcements.emplace_back(
        [widget, kind]() { return ValueText(widget, kind); }, kind != Kind::ProgressBar, Value);
    vtable.Announcements.emplace_back([widget, kind]() { return SelectedText(widget, kind); }, true, Selected);
    vtable.Announcements.emplace_back(
        [widget]() { return agui::enabled(widget) ? std::string() : std::string(vocab::kDisabled); }, false, Enabled);

    SetTooltip(vtable, {widget});
    // A tooltip the game builds rather than stores as text (a recipe's) is read off the screen.
    if (!vtable.OnTooltip && kind == Kind::Button)
        vtable.OnTooltip = [widget]() { SpeakGameTooltip(widget); };

    auto stateText = [widget, kind]()
    {
        std::string state = ValueText(widget, kind);
        return state.empty() ? SelectedText(widget, kind) : state;
    };
    switch (kind)
    {
    case Kind::Button:
    case Kind::CheckBox:
    case Kind::RadioButton:
    case Kind::Tab:
    case Kind::DropDown:
        // Pressed as a mouse would, so everything behaves and sounds as in vanilla. A dropdown
        // opens its list, which DropDownScreen then reads.
        vtable.OnActivate = [widget]() { agui::press(widget, agui::MouseButton::Left, false, false); };
        vtable.OnActivateShift = [widget]() { agui::press(widget, agui::MouseButton::Left, true, false); };
        vtable.OnActivateCtrl = [widget]() { agui::press(widget, agui::MouseButton::Left, false, true); };
        vtable.OnSecondary = [widget]() { agui::press(widget, agui::MouseButton::Right, false, false); };
        // A dropdown's value comes from its list; its state settles once that closes.
        if (kind != Kind::DropDown)
            vtable.StateText = stateText;
        break;
    case Kind::Slider:
        // Steps its value on the arrow keys and tells the game through its own listeners.
        vtable.OnAdjust = [widget](int sign, bool)
        { agui::sendKey(widget, sign > 0 ? agui::Key::Right : agui::Key::Left, /*release*/ false); };
        vtable.StateText = [widget, kind]() { return ValueText(widget, kind); };
        break;
    default:
        break;
    }
    return vtable;
}

graph::NodeVtable ControlNode(const Widget* widget, const Widget* label)
{
    graph::NodeVtable vtable = ControlNode(widget, [label]() { return Phrase(label); });
    // A slot's own tooltip is the game's description of what it holds, kept over the label's.
    if (!agui::isSlotButton(widget))
        SetTooltip(vtable, {label, widget});
    return vtable;
}

graph::NodeVtable TextNode(const Widget* tag, std::function<std::string()> text)
{
    graph::NodeVtable vtable;
    vtable.HostTag = tag;
    vtable.Announcements.emplace_back(std::move(text), true, Label);
    SetTooltip(vtable, {tag});
    return vtable;
}

void AddSubtree(graph::GraphBuilder& builder, const std::string& prefix, const Widget* widget,
    std::vector<const Widget*> skip, Attachments attachments)
{
    Walker(builder, prefix, std::move(skip), std::move(attachments)).Visit(widget, "", 0);
}

bool AddControl(graph::GraphBuilder& builder, const std::string& key, const Widget* widget,
    std::function<std::string()> name)
{
    if (!widget || !Shows(widget) || !IsControl(agui::kind(widget)))
        return false;
    builder.AddItem(graph::ControlId::Referenced(widget, key), ControlNode(widget, std::move(name)));
    return true;
}

std::vector<const Widget*> VisibleChildren(const Widget* widget)
{
    std::vector<const Widget*> visible;
    ForEachChild(widget,
        [&](const Widget* child, const std::string&)
        {
            if (Shows(child))
                visible.push_back(child);
        });
    return visible;
}

const Widget* FindDescendant(const Widget* widget, std::string_view className)
{
    if (!Shows(widget))
        return nullptr;
    if (agui::derivesFrom(widget, className))
        return widget;
    const Widget* found = nullptr;
    ForEachChild(widget,
        [&](const Widget* child, const std::string&)
        {
            if (!found)
                found = FindDescendant(child, className);
        });
    return found;
}

std::vector<const Widget*> FindAll(const Widget* widget, std::string_view className)
{
    std::vector<const Widget*> found;
    auto visit = [&](auto& self, const Widget* at, int depth) -> void
    {
        if (depth > kMaxDepth || !Shows(at))
            return;
        if (agui::derivesFrom(at, className))
        {
            found.push_back(at);
            return;
        }
        ForEachChild(at, [&](const Widget* child, const std::string&) { self(self, child, depth + 1); });
    };
    visit(visit, widget, 0);
    return found;
}

std::string LabelText(const Widget* label) { return Phrase(label); }

bool Contains(const Widget* ancestor, const Widget* widget)
{
    for (; widget; widget = agui::parent(widget))
        if (widget == ancestor)
            return true;
    return false;
}

} // namespace fa::screens
