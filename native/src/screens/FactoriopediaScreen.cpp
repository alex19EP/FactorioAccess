#include "FactoriopediaScreen.hpp"

#include <cmath>
#include <format>
#include <functional>
#include <string>
#include <vector>

#include "AguiNodes.hpp"
#include "speech.h"
#include "text.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Kind;
using agui::Widget;
namespace Kinds = graph::AnnouncementKinds;

constexpr const char* kSearchKey = "search/field";
constexpr const char* kTitleKey = "page/title";

const Widget* FindWindow()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return nullptr;
    return agui::factoriopedia().window;
}

const Widget* SearchField(const agui::Factoriopedia& pedia)
{
    if (const Widget* popup = FindDescendant(pedia.window, "SearchPopup"))
        return FindDescendant(popup, "agui::TextField");
    return FindDescendant(pedia.header, "agui::TextField");
}

// An entry by its name, "iron plate" or "rare iron plate", and the amount it shows, if any.
std::string EntryText(const Widget* slot)
{
    if (!agui::isSlotButton(slot))
        return NameOf(slot);
    agui::SlotButton button = agui::slotButton(slot);
    std::string text = button.quality.empty() ? std::string(button.name)
                                              : std::format("{} {}", button.quality, button.name);
    if (button.count <= 0)
        return text;
    if (button.count >= 100 || button.count == std::floor(button.count))
        return std::format("{} {:.0f}", text, button.count);
    return std::format("{} {:.1f}", text, button.count);
}

// An entry's button, in the list or on a page: named by what it shows, clicked as the mouse would,
// which shows that entry's page. The list's current entry shows pressed.
graph::NodeVtable EntryNode(const Widget* slot, std::function<std::string()> text = {})
{
    graph::NodeVtable vtable = ControlNode(slot);
    if (!text)
        text = [slot]() { return EntryText(slot); };
    vtable.Announcements.clear();
    vtable.Announcements.emplace_back(std::move(text), false, Kinds::Label);
    vtable.Announcements.emplace_back(
        [slot]() { return agui::buttonToggled(slot) ? std::string(vocab::kSelected) : std::string(); }, true,
        Kinds::Selected);
    return vtable;
}

// The page's title, "Wooden chest (Recipe/Item/Entity)": the entry's icon before its name reads once.
std::string TitleText(const Widget* title)
{
    return text::speakable(agui::text(title));
}

bool HasSlot(const Widget* table)
{
    for (const Widget* cell : agui::children(table))
        if (agui::isSlotButton(cell))
            return true;
    return false;
}

// The page's content, the game's own layout read in order. Anything without entries or links in it
// (the description's plain lines, a section's heading) is read by the generic walker; entries, the
// rows that lead with one (an ingredient: its button, then "2 × Iron plate") and the lines with links
// are declared here.
class PageWalker
{
public:
    explicit PageWalker(graph::GraphBuilder& builder) : _builder(builder) {}

    void Visit(const Widget* widget, const std::string& prefix)
    {
        std::size_t index = 0;
        for (const Widget* child : VisibleChildren(widget))
        {
            std::string key = prefix + "/" + std::to_string(index++);
            if (!NeedsOwnWalk(child))
                AddSubtree(_builder, key, child);
            else if (agui::isSlotButton(child))
                _builder.AddItem(graph::ControlId::Referenced(child, key), EntryNode(child));
            else if (AddLinkLine(_builder, key, child))
                continue;
            else if (agui::derivesFrom(child, "agui::Table") && HasSlot(child))
                AddGrid(
                    _builder, key, child, [](const Widget* cell) { return agui::isSlotButton(cell); },
                    [](const Widget* cell) { return EntryNode(cell); });
            else if (const Widget* slot = LeadingSlot(child))
                _builder.AddItem(graph::ControlId::Referenced(slot, key),
                    EntryNode(slot, [child, slot]() { return RowText(child, slot); }));
            else
                Visit(child, key);
        }
    }

private:
    static bool NeedsOwnWalk(const Widget* widget)
    {
        if (agui::isSlotButton(widget) || !agui::richTextLinks(widget).empty())
            return true;
        for (const Widget* child : VisibleChildren(widget))
            if (NeedsOwnWalk(child))
                return true;
        return false;
    }

    // The entry button a horizontal row starts with, when the rest of the row only describes it.
    static const Widget* LeadingSlot(const Widget* row)
    {
        if (!agui::derivesFrom(row, "agui::HorizontalFlow"))
            return nullptr;
        std::vector<const Widget*> children = VisibleChildren(row);
        if (children.empty() || !agui::isSlotButton(children.front()))
            return nullptr;
        for (std::size_t i = 1; i < children.size(); ++i)
            if (NeedsOwnWalk(children[i]))
                return nullptr;
        return children.front();
    }

    // The row's texts as one line, as they stand beside the button; the entry's own name without.
    static std::string RowText(const Widget* row, const Widget* slot)
    {
        std::string line;
        for (const Widget* label : FindAll(row, "agui::Label"))
            if (std::string text = LabelText(label); !text.empty())
                line += (line.empty() ? "" : " ") + text;
        return line.empty() ? EntryText(slot) : line;
    }

    graph::GraphBuilder& _builder;
};

// The title bar's buttons as one row. The two toggles say whether they are on as checkboxes do.
void AddHeader(graph::GraphBuilder& builder, const Widget* header)
{
    builder.BeginStop("header");
    builder.StartRow("header");
    int index = 0;
    std::function<void(const Widget*)> visit = [&](const Widget* at)
    {
        for (const Widget* child : VisibleChildren(at))
        {
            std::string key = "header/" + std::to_string(index++);
            // The search button is a toggle too, but it opens the field rather than switching
            // anything on.
            if (agui::kind(child) == Kind::Button && agui::buttonIsToggle(child) && !agui::derivesFrom(child, "SearchBar"))
            {
                auto checked = [child]()
                { return std::string(agui::buttonToggled(child) ? vocab::kChecked : vocab::kUnchecked); };
                graph::NodeVtable vtable = ControlNode(child);
                for (graph::NodeAnnouncement& part : vtable.Announcements)
                    if (part.Kind == Kinds::Value)
                        part.Text = checked;
                vtable.StateText = checked;
                builder.AddItem(graph::ControlId::Referenced(child, key), std::move(vtable));
            }
            else if (!AddControl(builder, key, child))
                visit(child);
        }
    };
    visit(header);
    builder.EndRow();
}

void AddList(graph::GraphBuilder& builder, const Widget* list)
{
    builder.BeginStop("list");
    std::vector<const Widget*> groups = FindAll(list, "ItemGroupTab");
    if (!groups.empty())
    {
        builder.StartRow("groups");
        for (std::size_t i = 0; i < groups.size(); ++i)
            builder.AddItem(graph::ControlId::Referenced(groups[i], "groups/" + std::to_string(i)), ControlNode(groups[i]));
        builder.EndRow();
    }
    // The chosen group's entries, a table with fillers ending each subgroup's line.
    std::size_t index = 0;
    for (const Widget* table : FindAll(list, "agui::Table"))
        if (HasSlot(table))
            AddGrid(
                builder, "entries" + std::to_string(index++), table,
                [](const Widget* cell) { return agui::isSlotButton(cell); },
                [](const Widget* cell) { return EntryNode(cell); });
}

} // namespace

bool FactoriopediaScreen::IsActive()
{
    const Widget* window = FindWindow();
    if (!window || (_window && window != _window))
    {
        // Gone, or opened anew: one inactive frame pops this screen.
        _window = nullptr;
        return false;
    }
    _window = window;
    return true;
}

void FactoriopediaScreen::Build(graph::GraphBuilder& builder)
{
    agui::Factoriopedia pedia = agui::factoriopedia();
    if (!_window || pedia.window != _window)
        return;

    const Widget* title = FindDescendant(pedia.subheader, "agui::Label");
    std::string titleText = title ? TitleText(title) : std::string();
    const Widget* search = SearchField(pedia);
    // Opening the search lands on its field; another entry's page showing, on its title.
    if (search && !_searching)
        _landing = kSearchKey;
    else if (!titleText.empty() && titleText != _title)
        _landing = kTitleKey;
    _searching = search != nullptr;
    _title = titleText;

    std::string caption;
    if (const Widget* label = agui::frameTitle(pedia.window))
        caption = text::speakable(agui::text(label));
    if (!caption.empty())
        builder.PushContext(caption);

    if (search)
    {
        builder.BeginStop("search");
        builder.AddItem(graph::ControlId::Referenced(search, kSearchKey), ControlNode(search));
    }
    AddHeader(builder, pedia.header);
    AddList(builder, pedia.list);

    builder.BeginStop("page");
    if (title)
        builder.AddItem(graph::ControlId::Referenced(title, kTitleKey), TextNode(title, [title]() { return TitleText(title); }));
    PageWalker(builder).Visit(pedia.page, "page");

    if (!caption.empty())
        builder.PopContext();
}

const char* FactoriopediaScreen::TakeSuggestedLanding()
{
    const char* landing = _landing;
    _landing = nullptr;
    return landing;
}

bool FactoriopediaScreen::TypingIn(const graph::GraphNode& node) { return TypingInField(node); }

void FactoriopediaScreen::OnCursorMoved(const graph::GraphNode& node) { FollowCursor(node); }

void FactoriopediaScreen::OnPop()
{
    _window = nullptr;
    _title.clear();
    _searching = false;
    _landing = nullptr;
}

} // namespace fa::screens
