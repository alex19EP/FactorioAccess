#include "TechnologyScreen.hpp"

#include <algorithm>
#include <cstdlib>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "AguiNodes.hpp"
#include "GuiDump.hpp"
#include "text.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;
namespace Kinds = graph::AnnouncementKinds;

constexpr const char* kSearchKey = "search/field";

const Widget* FindWindow()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return nullptr;
    return agui::technologyWindow().window;
}

// "Queued 3", "Researching": the status, with the place in the queue past the research going on.
std::string StatusText(const agui::TechnologyInfo& info)
{
    if (info.queuePosition > 1)
        return std::format("{} {}", info.status, info.queuePosition);
    return info.status;
}

std::string ProgressText(const agui::TechnologyInfo& info)
{
    return info.progress > 0 ? std::format("{:.0f}%", info.progress * 100) : std::string();
}

// A technology's button: its name with its level, then its status, live so that starting or
// queueing it is heard; the progress is read when reached, as it moves all the time. Clicked as the
// mouse would.
graph::NodeVtable TechnologyNode(const Widget* slot)
{
    graph::NodeVtable vtable = ControlNode(slot);
    vtable.Type = nullptr;
    vtable.Announcements.clear();
    vtable.Announcements.emplace_back([slot]() { return agui::technologyInfo(slot).name; }, false, Kinds::Label);
    vtable.Announcements.emplace_back(
        [slot]() { return StatusText(agui::technologyInfo(slot)); }, true, Kinds::Value);
    vtable.Announcements.emplace_back([slot]() { return ProgressText(agui::technologyInfo(slot)); });
    return vtable;
}

std::string Caption(const Widget* frame)
{
    const Widget* title = frame ? agui::frameTitle(frame) : nullptr;
    return title ? text::speakable(agui::text(title)) : std::string();
}

// The heading the game draws over a part of the window: the label just before it ("Research
// queue"), or the first label of the row just before it ("Technology list", beside the search).
std::string Heading(const Widget* part)
{
    const Widget* before = nullptr;
    for (const Widget* sibling : VisibleChildren(agui::parent(part)))
    {
        if (sibling == part)
            break;
        before = sibling;
    }
    if (!before)
        return {};
    const Widget* label = agui::kind(before) == agui::Kind::Label ? before : FindDescendant(before, "agui::Label");
    return label ? LabelText(label) : std::string();
}

// A Tab stop named as the player hears it on entering: everything declared until the matching
// EndZone is said in the zone's name.
void BeginZone(graph::GraphBuilder& builder, std::string key, const std::string& name)
{
    builder.BeginStop(std::move(key));
    builder.PushContext(name);
}

void EndZone(graph::GraphBuilder& builder) { builder.PopContext(); }

void AddQueue(graph::GraphBuilder& builder, const Widget* queue)
{
    if (!queue || !agui::visible(queue))
        return;
    BeginZone(builder, "queue", Heading(queue));
    std::vector<agui::QueueEntry> entries = agui::researchQueueEntries(queue);
    // A technology with several levels can be queued more than once.
    std::unordered_map<uint16_t, int> seen;
    for (const agui::QueueEntry& entry : entries)
    {
        uint16_t id = agui::technologyInfo(entry.slot).id;
        std::string key = std::format("queue/{}/{}", id, seen[id]++);
        builder.StartLine(key);
        builder.AddItem(graph::ControlId::Structural(key), TechnologyNode(entry.slot));
        builder.AddItem(graph::ControlId::Structural(key + "/cancel"), ControlNode(entry.cancel,
            [cancel = entry.cancel]()
            {
                std::string name = NameOf(cancel);
                return name.empty() ? std::string(vocab::kCancel) : name;
            }));
        builder.EndRow();
    }
    if (entries.empty())
        builder.AddLabel(graph::ControlId::Structural("queue/empty"), []() { return std::string(vocab::kEmpty); });
    EndZone(builder);
}

// Every technology, a row of the grid per row of the game's table, keyed by technology so that the
// cursor stays on one as the search filters the rest away.
void AddList(graph::GraphBuilder& builder, const agui::TechnologyWindow& window, const Widget* search)
{
    BeginZone(builder, "list", Heading(window.list));
    // The search button over the grid, which opens the field.
    AddControl(builder, "list/search", FindDescendant(window.window, "SearchBar"));
    if (search)
        builder.AddItem(graph::ControlId::Referenced(search, kSearchKey), ControlNode(search));
    unsigned columns = agui::tableColumns(window.listTable);
    auto cells = agui::children(window.listTable);
    for (std::size_t start = 0; columns > 0 && start < cells.size(); start += columns)
    {
        bool open = false;
        for (std::size_t i = start; i < cells.size() && i < start + columns; ++i)
        {
            const Widget* slot = cells[i];
            if (!agui::visible(slot) || !agui::isTechnologySlot(slot))
                continue;
            if (!open)
                builder.StartRow("list");
            open = true;
            builder.AddItem(graph::ControlId::Structural(std::format("list/{}", agui::technologyInfo(slot).id)),
                TechnologyNode(slot));
        }
        if (open)
            builder.EndRow();
    }
    EndZone(builder);
}

bool HasLinks(const Widget* widget)
{
    if (!agui::richTextLinks(widget).empty())
        return true;
    for (const Widget* child : VisibleChildren(widget))
        if (HasLinks(child))
            return true;
    return false;
}

// The selected technology's details in the game's order: cost, effects, description, Start
// research. A line with icons the game makes clickable (the cost's "Craft 50 iron plate") carries
// them as links.
void AddDetails(graph::GraphBuilder& builder, const std::string& prefix, const Widget* widget, const Widget* skip)
{
    std::size_t index = 0;
    for (const Widget* child : VisibleChildren(widget))
    {
        std::string key = prefix + "/" + std::to_string(index++);
        if (child == skip)
            continue;
        if (!HasLinks(child) && !(skip && Contains(child, skip)))
            AddSubtree(builder, key, child);
        else if (!AddLinkLine(builder, key, child))
            AddDetails(builder, key, child, skip);
    }
}

void AddSelected(graph::GraphBuilder& builder, const agui::TechnologyWindow& window)
{
    BeginZone(builder, "selected", std::string(vocab::kSelectedTechnology));
    const Widget* title = window.title;
    const Widget* status = window.status;
    builder.AddItem(graph::ControlId::Structural("selected/title"), TextNode(title,
        [title, status]()
        {
            std::string line = LabelText(title);
            std::string state = agui::visible(status) ? LabelText(status) : std::string();
            return state.empty() ? line : line + " " + state;
        }));
    // The details start with the technology's own button, which the title already reads.
    AddDetails(builder, "selected", window.featured, FindDescendant(window.featured, "TechnologySlot"));
    EndZone(builder);
}

// The graph's title bar (Back, Forward, close) and "Show only essential technologies" under it.
void AddControls(graph::GraphBuilder& builder, const agui::TechnologyWindow& window)
{
    BeginZone(builder, "controls", std::string(vocab::kTreeControls));
    builder.StartRow("controls");
    int index = 0;
    for (const Widget* button : FindAll(window.graphTitle, "agui::Button"))
        AddControl(builder, std::format("controls/{}", index++), button);
    builder.EndRow();
    if (const Widget* essential = FindDescendant(window.graphHolder, "agui::CheckBox"))
        AddControl(builder, "controls/essential", essential);
    EndZone(builder);
}

std::string VertexKey(const agui::TechnologyVertex& vertex)
{
    return vertex.omitted ? std::format("graph/omitted/{}", vertex.technology)
                          : std::format("graph/{}", vertex.technology);
}

graph::NodeVtable VertexNode(const agui::TechnologyVertex& vertex, bool central, int index, int count)
{
    graph::NodeVtable vtable;
    if (vertex.omitted)
    {
        vtable = ControlNode(vertex.button);
        vtable.Type = nullptr;
        vtable.Announcements.clear();
        vtable.Announcements.emplace_back(
            [button = vertex.button, omitted = vertex.omitted]()
            {
                std::string name = NameOf(button);
                return name.empty() ? vocab::kOmitted(omitted) : name;
            },
            false, Kinds::Label);
    }
    else
    {
        vtable = TechnologyNode(vertex.button);
    }
    // The selected technology is where the stop is entered.
    if (central)
        vtable.Announcements.emplace_back([]() { return std::string(vocab::kSelected); }, false, Kinds::Selected);
    vtable.Announcements.emplace_back([index, count]() { return vocab::position(index, count); }, false,
        Kinds::Position);
    vtable.SpeaksOwnPosition = true;
    return vtable;
}

// Of the technologies an edge leads to, the one in the nearest layer, and of those the nearest
// across.
std::optional<std::size_t> Nearest(const agui::TechnologyGraph& graph, std::size_t from, const std::vector<std::size_t>& to)
{
    const agui::TechnologyVertex& origin = graph.vertices[from];
    std::optional<std::size_t> best;
    auto distance = [&](std::size_t i)
    {
        const agui::TechnologyVertex& v = graph.vertices[i];
        unsigned layers = v.layer > origin.layer ? v.layer - origin.layer : origin.layer - v.layer;
        return std::pair(layers, std::abs(v.x - origin.x));
    };
    for (std::size_t i : to)
        if (!best || distance(i) < distance(*best))
            best = i;
    return best;
}

void AddGraph(graph::GraphBuilder& builder, const agui::TechnologyWindow& window)
{
    agui::TechnologyGraph graph = agui::technologyGraph(window.graph);
    if (graph.vertices.empty())
        return;
    // Keyed by the selected technology: a newly selected one is a stop not yet visited, entered on
    // that technology rather than where the cursor last was.
    uint16_t central = graph.central < graph.vertices.size() ? graph.vertices[graph.central].technology : 0;
    BeginZone(builder, std::format("graph/{}", central), Caption(window.graphTitle));

    std::map<unsigned, std::vector<std::size_t>> layers;
    for (std::size_t i = 0; i < graph.vertices.size(); ++i)
        layers[graph.vertices[i].layer].push_back(i);
    for (auto& [layer, members] : layers)
        std::ranges::sort(members, {}, [&](std::size_t i) { return graph.vertices[i].x; });

    std::vector<graph::ControlId> ids;
    for (const agui::TechnologyVertex& vertex : graph.vertices)
        ids.push_back(graph::ControlId::Structural(VertexKey(vertex)));
    for (const auto& [layer, members] : layers)
        for (std::size_t at = 0; at < members.size(); ++at)
        {
            std::size_t i = members[at];
            graph::NodeVtable vtable =
                VertexNode(graph.vertices[i], i == graph.central, static_cast<int>(at + 1), static_cast<int>(members.size()));
            builder.AddNode(ids[i], std::move(vtable));
        }
    for (const auto& [layer, members] : layers)
        for (std::size_t at = 0; at < members.size(); ++at)
        {
            std::size_t i = members[at];
            if (at > 0)
                builder.Connect(ids[i], graph::GraphDir::Left, ids[members[at - 1]]);
            if (at + 1 < members.size())
                builder.Connect(ids[i], graph::GraphDir::Right, ids[members[at + 1]]);
            if (auto up = Nearest(graph, i, graph.vertices[i].prerequisites))
                builder.Connect(ids[i], graph::GraphDir::Up, ids[*up]);
            if (auto down = Nearest(graph, i, graph.vertices[i].unlocks))
                builder.Connect(ids[i], graph::GraphDir::Down, ids[*down]);
        }
    if (graph.central < graph.vertices.size())
        builder.SetStart(ids[graph.central]);
    EndZone(builder);
}

} // namespace

bool TechnologyScreen::IsActive()
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

void TechnologyScreen::Build(graph::GraphBuilder& builder)
{
    agui::TechnologyWindow window = agui::technologyWindow();
    if (!_window || window.window != _window)
        return;
    DumpWindow(window.window, "TechnologyGui");

    const Widget* search = FindDescendant(window.window, "agui::TextField");
    // Opening the search lands on its field.
    if (search && !_searching)
        _landing = kSearchKey;
    _searching = search != nullptr;

    AddQueue(builder, window.queue);
    AddList(builder, window, search);
    AddSelected(builder, window);
    AddControls(builder, window);
    AddGraph(builder, window);
}

const char* TechnologyScreen::TakeSuggestedLanding()
{
    const char* landing = _landing;
    _landing = nullptr;
    return landing;
}

bool TechnologyScreen::TypingIn(const graph::GraphNode& node) { return TypingInField(node); }

void TechnologyScreen::OnCursorMoved(const graph::GraphNode& node) { FollowCursor(node); }

void TechnologyScreen::OnPop()
{
    _window = nullptr;
    _searching = false;
    _landing = nullptr;
}

} // namespace fa::screens
