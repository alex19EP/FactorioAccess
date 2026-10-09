#include "ConsoleScreen.hpp"

#include <format>
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

using agui::Widget;
using graph::AnnouncementKinds::Label;
using graph::AnnouncementKinds::Role;

constexpr const char* kFieldKey = "field";

// Only over a loaded game, under any menu opened over it.
const Widget* OpenField()
{
    if (agui::menuStateWindow())
        return nullptr;
    return agui::consoleInput();
}

graph::NodeVtable LineNode(std::string line)
{
    graph::NodeVtable vtable;
    vtable.Announcements.emplace_back([line = std::move(line)]() { return text::speakable(line); }, false, Label);
    return vtable;
}

} // namespace

std::string ConsoleScreen::Name() const { return vocab::kConsole; }

bool ConsoleScreen::IsActive()
{
    if (_clickItem)
    {
        const void* item = _clickItem;
        _clickItem = nullptr;
        agui::clickConsoleLink(item, _clickSection);
    }
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

void ConsoleScreen::Build(graph::GraphBuilder& builder)
{
    const Widget* field = OpenField();
    if (!field || field != _field)
        return;

    builder.BeginStop("field");
    builder.AddItem(graph::ControlId::Referenced(field, kFieldKey), ControlNode(field));

    const Widget* button = FindDescendant(field, "ChooseChatIconButton");
    if (button && Shows(button))
    {
        builder.BeginStop("icon");
        builder.AddItem(graph::ControlId::Referenced(button, "icon"),
            ControlNode(button, []() { return std::string(vocab::kSelectIcon); }));
    }

    // Keyed by the message, so the cursor keeps its line as new ones come in under it.
    builder.BeginStop("log");
    builder.LandOnLast();
    builder.PushContext(vocab::kConsoleLog, "", /*positions*/ false);
    std::vector<agui::ConsoleLine> lines = agui::consoleLines();
    if (lines.empty())
        builder.AddLabel(graph::ControlId::Structural("log/none"), []() { return std::string(vocab::kEmpty); });
    for (agui::ConsoleLine& line : lines)
    {
        std::string key = std::format("log/{}", line.item);
        if (line.links.empty())
        {
            builder.AddItem(graph::ControlId::Referenced(line.item, key), LineNode(std::move(line.text)));
            continue;
        }
        builder.StartLine(key);
        builder.AddItem(graph::ControlId::Referenced(line.item, key), LineNode(std::move(line.text)));
        for (std::size_t i = 0; i < line.links.size(); ++i)
        {
            const void* item = line.item;
            std::size_t section = line.links[i].section;
            std::string tag(line.links[i].tag);

            graph::NodeVtable vtable;
            vtable.Announcements.emplace_back(
                [tag]() { return text::speakable(std::format("[{}]", tag)); }, false, Label);
            vtable.Announcements.emplace_back([]() { return std::string(vocab::kLink); }, false, Role);
            vtable.OnActivate = [this, item, section]()
            {
                _clickItem = item;
                _clickSection = section;
            };
            vtable.OnTooltip = [item, section]()
            {
                const Widget* tooltip = agui::hoverConsoleLink(item, section);
                std::string text = tooltip ? TooltipText(tooltip) : std::string();
                agui::clearConsoleHover();
                speech::say(text.empty() ? std::string(vocab::kNoTooltip) : text, true);
            };
            builder.AddItem(graph::ControlId::Referenced(item, std::format("{}/link{}", key, i)), std::move(vtable));
        }
        builder.EndRow();
    }
    builder.PopContext();
}

const char* ConsoleScreen::TakeSuggestedLanding()
{
    bool land = _landOnField;
    _landOnField = false;
    return land ? kFieldKey : nullptr;
}

bool ConsoleScreen::TypingIn(const graph::GraphNode& node) { return TypingInField(node); }

void ConsoleScreen::OnCursorMoved(const graph::GraphNode& node) { FollowCursor(node); }

void ConsoleScreen::OnPush() { _landOnField = true; }

void ConsoleScreen::OnPop()
{
    _field = nullptr;
    _landOnField = false;
    _clickItem = nullptr;
}

// Closed over the map, it is back to the map; a window it was opened over says itself.
std::string ConsoleScreen::LeaveLine() const { return vocab::kMap; }

} // namespace fa::screens
