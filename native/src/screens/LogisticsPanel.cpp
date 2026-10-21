#include "LogisticsPanel.hpp"

#include <format>
#include <utility>
#include <vector>

#include "AguiNodes.hpp"
#include "game.h"

namespace fa::screens
{

namespace
{

using agui::Widget;

std::vector<const Widget*> Sections(const Widget* pane)
{
    std::vector<const Widget*> sections;
    for (const Widget* child : VisibleChildren(pane))
        if (agui::derivesFrom(child, "LogisticGuiSection"))
            sections.push_back(child);
    return sections;
}

// The group a section takes its filters from, as its header shows it: the caption of its group
// checkbox ("[No group assigned]", "Mall"), or the label a section the player cannot edit shows.
std::string SectionName(const Widget* section)
{
    const Widget* subheader = agui::member(section, game::layout.logisticSectionSubheader);
    if (const Widget* checkbox = FindDescendant(subheader, "agui::CheckBox"))
        return OwnText(checkbox);
    if (const Widget* label = FindDescendant(subheader, "agui::Label"))
        return LabelText(label);
    return {};
}

// Whether `widget` shows inside `panel`: a storage chest's panel hides the part holding the sections
// and the add button, which stay visible themselves.
bool ShownIn(const Widget* panel, const Widget* widget)
{
    for (; widget && widget != panel; widget = agui::parent(widget))
        if (!agui::visible(widget))
            return false;
    return widget == panel;
}

std::string SectionKey(const std::string& key, std::size_t index) { return std::format("{}/section/{}", key, index); }

void AddSection(graph::GraphBuilder& builder, const std::string& key, const Widget* pane, const Widget* section,
    std::size_t index)
{
    std::string sectionKey = SectionKey(key, index);
    std::string name = SectionName(section);
    builder.SetRegion(sectionKey);

    // Moved as dragging its handle moves it. The game shows it there at once, and rebuilds the
    // sections once it has applied the move; the cursor goes with it either way.
    auto reorder = [pane, section, index, key, name, land = BuildLander()](int by)
    {
        if (!agui::dragChild(pane, section, by))
            return false;
        auto to = static_cast<std::size_t>(static_cast<int>(index) + by);
        if (land)
            land(SectionKey(key, to) + "/header.0",
                [pane, section, to, name]()
                {
                    std::vector<const Widget*> now = Sections(pane);
                    return to < now.size() && (now[to] == section || SectionName(now[to]) == name);
                });
        return true;
    };
    builder.StartRow(sectionKey + "/header");
    AddRowItems(builder, sectionKey + "/header", {agui::member(section, game::layout.logisticSectionSubheader)}, {},
        std::move(reorder));
    builder.EndRow();

    const Widget* filters = agui::member(section, game::layout.logisticSectionFilters);
    if (!name.empty())
        builder.PushContext(name, "", /*positions*/ false);
    AddGrid(
        builder, sectionKey + "/slots", filters, [](const Widget* cell) { return agui::isSlotButton(cell); },
        [](const Widget* cell) { return ControlNode(cell); });
    if (!name.empty())
        builder.PopContext();
}

} // namespace

void AddLogistics(graph::GraphBuilder& builder, const std::string& key, const Widget* panel)
{
    const Widget* pane = agui::member(panel, game::layout.logisticGuiSections);
    const Widget* add = agui::member(panel, game::layout.logisticGuiAddSection);

    // The panel's own parts, which the walker would hand back here.
    builder.SetRegion(key + "/options");
    std::vector<const Widget*> parts = VisibleChildren(panel);
    for (std::size_t i = 0; i < parts.size(); ++i)
        AddSubtree(builder, std::format("{}/options/{}", key, i), parts[i], {pane, add});

    if (ShownIn(panel, pane))
    {
        std::vector<const Widget*> sections = Sections(pane);
        for (std::size_t i = 0; i < sections.size(); ++i)
            AddSection(builder, key, pane, sections[i], i);
    }
    if (ShownIn(panel, add))
    {
        builder.SetRegion(key + "/add");
        AddControl(builder, key + "/add", add);
    }
    builder.SetRegion("");
}

} // namespace fa::screens
