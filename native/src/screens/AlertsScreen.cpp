#include "AlertsScreen.hpp"

#include <format>
#include <string>

#include "AguiNodes.hpp"
#include "GuiDump.hpp"
#include "game.h"
#include "text.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;

const Widget* FindWindow()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return nullptr;
    return agui::alertsWindow().window;
}

// "Alerts, attack": the window's title, then the category, which the title leaves out.
std::string Context(const agui::AlertsWindow& alerts)
{
    const Widget* title = agui::frameTitle(alerts.window);
    std::string context = title ? text::speakable(agui::text(title)) : std::string(vocab::kAlerts);
    std::string category = vocab::alertCategory(static_cast<uint8_t>(alerts.category));
    return category.empty() ? context : std::format("{}, {}", context, category);
}

} // namespace

bool AlertsScreen::IsActive()
{
    const Widget* window = FindWindow();
    if (!window || (_window && window != _window))
    {
        // Gone, or opened anew for another category: one inactive frame pops this screen.
        _window = nullptr;
        return false;
    }
    _window = window;
    return true;
}

void AlertsScreen::Build(graph::GraphBuilder& builder)
{
    agui::AlertsWindow alerts = agui::alertsWindow();
    if (!_window || alerts.window != _window)
        return;
    DumpWindow(alerts.window, "AlertsOverview");

    builder.BeginStop("alerts");
    builder.PushContext(Context(alerts));
    bool any = false;
    // Keyed by place: the game rebuilds the list as alerts come and go, and the cursor keeps its row.
    for (std::size_t i = 0; i < alerts.rows.size(); ++i)
    {
        const agui::AlertsRow& row = alerts.rows[i];
        const Widget* item = row.item;
        if (!Shows(item))
            continue;
        std::string key = std::format("rows/{}", i);
        auto text = [item]() { return text::speakable(agui::text(item)); };
        if (!row.pin)
        {
            builder.AddItem(graph::ControlId::Referenced(item, key), TextNode(item, text));
            continue;
        }
        any = true;
        builder.StartLine(key);
        builder.AddItem(graph::ControlId::Referenced(item, key), ControlNode(item, text));
        builder.AddItem(graph::ControlId::Referenced(row.pin, key + "/pin"),
            ControlNode(row.pin, []() { return std::string(vocab::kPin); }));
        builder.EndRow();
    }
    if (!any)
        builder.AddLabel(graph::ControlId::Structural("empty"), []() { return std::string(vocab::kEmpty); });
    builder.PopContext();

    builder.BeginStop("close");
    AddControl(builder, "close", FindDescendant(agui::member(alerts.window, game::layout.frameHeader), "CloseButton"));
}

void AlertsScreen::OnCursorMoved(const graph::GraphNode& node) { FollowCursor(node); }

void AlertsScreen::OnPop() { _window = nullptr; }

} // namespace fa::screens
