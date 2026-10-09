#include "TipsAndTricksScreen.hpp"

#include <functional>
#include <string>
#include <vector>

#include "game.h"
#include "text.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;
using game::layout;
namespace Kinds = graph::AnnouncementKinds;

constexpr const char* kSearchKey = "search/field";
constexpr const char* kTitleKey = "page/title";

// A tip's button in the list: its name as shown (the indent is spacing), then whether the game
// marks it new or shows it. Enter clicks it as the mouse would, and `chosen` runs after the click.
graph::NodeVtable TipNode(const Widget* button, bool suggested, std::function<void()> chosen)
{
    graph::NodeVtable vtable = ControlNode(button);
    vtable.Announcements.clear();
    vtable.Announcements.emplace_back([button]() { return text::speakable(agui::text(button)); }, false, Kinds::Label);
    vtable.Announcements.emplace_back(
        [suggested]() { return suggested ? std::string(vocab::kNewTip) : std::string(); }, false, Kinds::Value);
    auto selected = [button]() { return agui::buttonToggled(button) ? std::string(vocab::kSelected) : std::string(); };
    vtable.Announcements.emplace_back(selected, true, Kinds::Selected);
    vtable.StateText = selected;
    vtable.OnActivate = [button, chosen = std::move(chosen)]()
    {
        agui::press(button, agui::MouseButton::Left, false, false);
        chosen();
    };
    return vtable;
}

// The tips the game shows and the search leaves, as one list. The game hides locked tips. Each title
// tip and the indented tips after it are a region, as are the tips before the first title and any
// unindented run after a title's tips, so Ctrl+Up and Ctrl+Down move from heading to heading.
void AddList(graph::GraphBuilder& builder, const Widget* window, std::function<void()> chosen)
{
    builder.BeginStop("list");
    std::vector<const Widget*> buttons = agui::listBoxItems(agui::member(window, layout.tipsList));
    std::vector<agui::Tip> tips = agui::tips();
    std::string region = "region/start";
    std::string declared;
    bool underTitle = false;
    for (std::size_t i = 0; i < buttons.size(); ++i)
    {
        agui::Tip tip = i < tips.size() ? tips[i] : agui::Tip{};
        // A title the search hides still ends the region before it.
        if (tip.title || (tip.indent == 0 && underTitle))
            region = "region/" + std::to_string(i);
        underTitle = tip.title || (tip.indent > 0 && underTitle);
        const Widget* button = buttons[i];
        if (!Shows(button))
            continue;
        if (region != declared)
        {
            builder.SetRegion(region);
            declared = region;
        }
        // Keyed by the tip, not the button: reading a tip makes the game build the list anew.
        builder.AddItem(graph::ControlId::Referenced(button, "tips/" + std::to_string(i)),
            TipNode(button, tip.suggested, chosen));
    }
    builder.SetRegion("");
}

void AddPage(graph::GraphBuilder& builder, const Widget* window)
{
    builder.BeginStop("page");
    if (!Shows(agui::member(window, layout.tipsContent)))
    {
        AddSubtree(builder, "page/nothing", agui::member(window, layout.tipsNothingFound));
        return;
    }
    const Widget* title = agui::member(window, layout.tipsTitle);
    builder.AddItem(graph::ControlId::Referenced(title, kTitleKey), TextNode(title, [title]() { return LabelText(title); }));
    AddLinkLines(builder, "page/text", agui::member(window, layout.tipsText));
    AddControl(builder, "page/tutorial", agui::member(window, layout.tipsPlayTutorial));
    AddControl(builder, "page/unread", agui::member(window, layout.tipsUnread));
}

// The title bar's buttons as one row: search, Back, Forward and close.
void AddControls(graph::GraphBuilder& builder, const Widget* window)
{
    builder.BeginStop("controls");
    builder.StartRow("controls");
    int index = 0;
    std::function<void(const Widget*)> visit = [&](const Widget* at)
    {
        for (const Widget* child : VisibleChildren(at))
            if (!AddControl(builder, "controls/" + std::to_string(index++), child))
                visit(child);
    };
    visit(agui::member(window, layout.frameHeader));
    builder.EndRow();
}

} // namespace

bool TipsAndTricksScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "TipsAndTricksGui"); }

void TipsAndTricksScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* caption = agui::frameTitle(window);
    std::string context = caption ? text::speakable(agui::text(caption)) : std::string();
    if (!context.empty())
        builder.PushContext(context);

    // The search field drops down from the title bar in a popup of the window's own.
    const Widget* popup = FindDescendant(window, "SearchPopup");
    const Widget* field = popup ? FindDescendant(popup, "agui::TextField") : nullptr;
    const Widget* title = agui::member(window, layout.tipsTitle);
    std::string titleText = Shows(agui::member(window, layout.tipsContent)) ? LabelText(title) : std::string();
    // Opening the search lands on its field; another tip showing, on its title. The window opens
    // on the list's selected tip.
    if (field && !_searching)
        _landing = kSearchKey;
    else if (!_title.empty() && !titleText.empty() && titleText != _title)
        _landing = kTitleKey;
    _searching = field != nullptr;
    _title = titleText;

    if (field)
    {
        builder.BeginStop("search");
        builder.AddItem(graph::ControlId::Referenced(field, kSearchKey), ControlNode(field));
    }
    AddList(builder, window, [this]() { _landing = kTitleKey; });
    AddPage(builder, window);
    AddControls(builder, window);

    if (!context.empty())
        builder.PopContext();
}

const char* TipsAndTricksScreen::TakeSuggestedLanding()
{
    const char* landing = _landing;
    _landing = nullptr;
    return landing;
}

void TipsAndTricksScreen::OnPop()
{
    EntityWindowScreen::OnPop();
    _title.clear();
    _searching = false;
    _landing = nullptr;
}

} // namespace fa::screens
