#include "MapSearchScreen.hpp"

#include <format>
#include <string>

#include "AguiNodes.hpp"
#include "text.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;

constexpr const char* kFieldKey = "search/field";

// The frame the game draws around remote view, holding its title bar; null outside remote view.
const Widget* FindFrame()
{
    const agui::Gui* gui = agui::applicationGui();
    const Widget* root = gui ? agui::baseWidget(gui) : nullptr;
    if (!root)
        return nullptr;
    for (const Widget* child : agui::children(root))
        if (agui::visible(child) && agui::derivesFrom(child, "RemoteControllerView::FrameAround"))
            return child;
    return nullptr;
}

// The search field, while the title bar's search button shows it.
const Widget* SearchField(const Widget* frame)
{
    const Widget* popup = frame ? FindDescendant(frame, "SearchPopup") : nullptr;
    return popup ? FindDescendant(popup, "agui::TextField") : nullptr;
}

// Only over the map: a window opened in its place (the inventory, an entity's) takes over.
const Widget* OpenField()
{
    if (!agui::inGame() || agui::menuStateWindow() || agui::gameWindowOpen())
        return nullptr;
    return SearchField(FindFrame());
}

} // namespace

bool MapSearchScreen::IsActive()
{
    const Widget* field = OpenField();
    if (!field || (_field && field != _field))
    {
        // Closed, or opened anew: one inactive frame pops this screen.
        _field = nullptr;
        return false;
    }
    _field = field;
    return true;
}

void MapSearchScreen::Build(graph::GraphBuilder& builder)
{
    const Widget* frame = FindFrame();
    const Widget* field = SearchField(frame);
    if (!field || field != _field)
        return;

    // The field is named by the title bar's search button: "Search (Ctrl + F)". The results follow it
    // in the same stop, so Down goes from the field to the first.
    const Widget* button = FindDescendant(frame, "SearchBar");
    builder.BeginStop("search");
    builder.AddItem(graph::ControlId::Referenced(field, kFieldKey),
        ControlNode(field, [button]() { return button ? NameOf(button) : std::string(); }));

    bool any = false;
    // Keyed by place: the game refills the list as the search changes, and the cursor keeps its row.
    agui::ChartSearchResults results = agui::chartSearchResults();
    for (std::size_t i = 0; i < results.rows.size(); ++i)
    {
        const agui::ChartSearchRow& row = results.rows[i];
        const Widget* item = row.item;
        if (!Shows(item))
            continue;
        any = true;
        std::string key = std::format("results/{}", i);
        auto line = [item]() { return text::speakable(agui::text(item)); };
        if (!row.pin || !Shows(row.pin))
        {
            builder.AddItem(graph::ControlId::Referenced(item, key), ControlNode(item, line));
            continue;
        }
        builder.StartLine(key);
        builder.AddItem(graph::ControlId::Referenced(item, key), ControlNode(item, line));
        builder.AddItem(graph::ControlId::Referenced(row.pin, key + "/pin"),
            ControlNode(row.pin, []() { return std::string(vocab::kPin); }));
        builder.EndRow();
    }
    if (!any)
        builder.AddLabel(graph::ControlId::Structural("results/none"), []() { return std::string(vocab::kEmpty); });
}

const char* MapSearchScreen::TakeSuggestedLanding()
{
    bool land = _landOnField;
    _landOnField = false;
    return land ? kFieldKey : nullptr;
}

bool MapSearchScreen::TypingIn(const graph::GraphNode& node) { return TypingInField(node); }

void MapSearchScreen::OnCursorMoved(const graph::GraphNode& node) { FollowCursor(node); }

void MapSearchScreen::OnEscape()
{
    // Closed as the mouse closes it, by the title bar's search button.
    if (const Widget* button = FindDescendant(FindFrame(), "SearchBar"))
        agui::press(button, agui::MouseButton::Left, false, false);
}

void MapSearchScreen::OnPush() { _landOnField = true; }

void MapSearchScreen::OnPop()
{
    _field = nullptr;
    _landOnField = false;
}

std::string MapSearchScreen::LeaveLine() const
{
    // Closed in remote view, by Escape in the field or on a result, it is back to the map; leaving
    // remote view says the view itself.
    return FindFrame() ? std::string(vocab::kMap) : std::string();
}

} // namespace fa::screens
