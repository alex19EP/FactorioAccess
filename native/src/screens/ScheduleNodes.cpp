#include "ScheduleNodes.hpp"

#include <format>
#include <utility>
#include <vector>

#include "AguiNodes.hpp"
#include "vocab.h"

namespace fa::screens
{

using agui::Widget;

namespace
{

void AddText(graph::GraphBuilder& builder, const std::string& key, const Widget* label,
    std::function<std::string()> text = {}, std::function<bool(int by)> reorder = {})
{
    if (!text)
        text = [label]() { return LabelText(label); };
    graph::NodeVtable node = TextNode(label, std::move(text));
    node.OnReorder = std::move(reorder);
    builder.AddItem(graph::ControlId::Referenced(label, key), std::move(node));
}

// `word` names an icon button the game gives no tooltip.
void AddButton(graph::GraphBuilder& builder, const std::string& key, const Widget* widget,
    std::function<bool(int by)> reorder, const vocab::Word* word = nullptr)
{
    if (!widget || !Shows(widget))
        return;
    graph::NodeVtable node = word ? ControlNode(widget, [word]() { return std::string(*word); }) : ControlNode(widget);
    node.OnReorder = std::move(reorder);
    builder.AddItem(graph::ControlId::Referenced(widget, key), std::move(node));
}

// A list's item moved by one place as dragging it does, then the cursor following it to the key
// `keyAt` gives its new place once `isAt` finds it there.
std::function<bool(int by)> Reorder(const Widget* pane, const Widget* item, int index,
    std::function<std::string(int)> keyAt, std::function<bool(int)> isAt, Lander land)
{
    return [=](int by)
    {
        if (!agui::dragChild(pane, item, by))
            return false;
        int to = index + by;
        land(keyAt(to), [isAt, to]() { return isAt(to); });
        return true;
    };
}

std::string StationKey(const std::string& list, int index) { return std::format("{}/station/{}", list, index); }

void AddStation(graph::GraphBuilder& builder, const std::string& list, int index, const agui::ScheduleStation& station,
    const StationsNow& now, const Widget* pane, const Lander& land)
{
    std::string key = StationKey(list, index);
    const Widget* label = station.label;
    const Widget* goTo = station.goTo;
    std::string name = LabelText(label);
    auto reorder = Reorder(
        pane, station.widget, index, [list](int to) { return StationKey(list, to) + "/name"; },
        [now, name](int to)
        {
            std::vector<agui::ScheduleStation> stations = now();
            return to < static_cast<int>(stations.size()) && LabelText(stations[to].label) == name;
        },
        land);

    builder.SetRegion(key);
    builder.StartRow("station");
    AddText(
        builder, key + "/name", label,
        [label, goTo]()
        {
            std::string text = LabelText(label);
            if (agui::buttonToggled(goTo))
                text += ", " + std::string(vocab::kCurrentStop);
            return text;
        },
        reorder);
    AddButton(builder, key + "/goto", goTo, reorder, &vocab::kGoNow);
    AddButton(builder, key + "/change", station.change, reorder, &vocab::kChangeStation);
    AddButton(builder, key + "/remove", station.remove, reorder, &vocab::kRemove);
    AddButton(builder, key + "/unload", station.unload, reorder);
    builder.EndRow();

    // In the station's context, said as the cursor comes in from another station. Stations of one
    // name are never entered one from the other: the next station's own row lies between them.
    if (!name.empty())
        builder.PushContext(name, "", false);
    // The station's widgets are rebuilt with the list, so its conditions are found through it.
    ConditionsNow conditions = [now, index]()
    {
        std::vector<agui::ScheduleStation> stations = now();
        return index < static_cast<int>(stations.size()) ? stations[index].conditions : std::vector<const Widget*>();
    };
    AddConditions(builder, key, conditions, station.conditionsPane, station.addCondition, land);
    if (!name.empty())
        builder.PopContext();
}

} // namespace

void AddConditions(graph::GraphBuilder& builder, const std::string& key, const ConditionsNow& now, const Widget* pane,
    const Widget* add, const Lander& land)
{
    std::vector<const Widget*> conditions = now();
    auto keyAt = [key](int index) { return std::format("{}/condition/{}", key, index); };
    for (std::size_t i = 0; i < conditions.size(); ++i)
    {
        const Widget* condition = conditions[i];
        if (!Shows(condition))
            continue;
        const Widget* remove = agui::scheduleConditionRemove(condition);
        std::vector<const Widget*> skip{agui::scheduleConditionDrag(condition), remove};
        std::string text = RowText(condition, skip);
        // A condition's first item: and or, or the condition's type in the first row.
        auto reorder = Reorder(
            pane, condition, static_cast<int>(i), [keyAt](int to) { return keyAt(to) + ".0"; },
            [now, text](int to)
            {
                std::vector<const Widget*> moved = now();
                return to < static_cast<int>(moved.size())
                    && RowText(moved[to], {agui::scheduleConditionDrag(moved[to])}) == text;
            },
            land);
        builder.StartRow("condition");
        AddRowItems(builder, keyAt(static_cast<int>(i)), {condition}, std::move(skip), reorder);
        AddButton(builder, keyAt(static_cast<int>(i)) + "/remove", remove, std::move(reorder), &vocab::kRemove);
        builder.EndRow();
    }
    AddControl(builder, key + "/add", add);
}

void AddStations(graph::GraphBuilder& builder, const std::string& key, const StationsNow& now, const Widget* pane,
    const Widget* add, const Lander& land)
{
    std::vector<agui::ScheduleStation> stations = now();
    for (std::size_t i = 0; i < stations.size(); ++i)
        if (Shows(stations[i].label))
            AddStation(builder, key, static_cast<int>(i), stations[i], now, pane, land);
    builder.SetRegion(key + "/end");
    AddControl(builder, key + "/add", add);
}

void AddSchedule(graph::GraphBuilder& builder, const std::string& key, const Widget* schedule, const Lander& land)
{
    agui::ScheduleParts parts = agui::scheduleParts(schedule);

    builder.BeginStop(key);
    if (parts.groupNote && Shows(parts.groupNote))
        AddText(builder, key + "/note", parts.groupNote);
    AddStations(
        builder, key, [schedule]() { return agui::scheduleParts(schedule).stations; }, parts.stationsPane,
        parts.addStation, land);

    std::string interrupts = key + "/interrupts";
    auto rowKey = [interrupts](int index) { return std::format("{}/{}", interrupts, index); };
    builder.BeginStop(interrupts);
    for (std::size_t i = 0; i < parts.interrupts.size(); ++i)
    {
        const agui::ScheduleInterrupt& interrupt = parts.interrupts[i];
        if (!Shows(interrupt.name))
            continue;
        std::string row = rowKey(static_cast<int>(i));
        std::string name = LabelText(interrupt.name);
        auto reorder = Reorder(
            parts.interruptsPane, interrupt.widget, static_cast<int>(i),
            [rowKey](int to) { return rowKey(to) + "/name"; },
            [schedule, name](int to)
            {
                std::vector<agui::ScheduleInterrupt> moved = agui::scheduleParts(schedule).interrupts;
                return to < static_cast<int>(moved.size()) && LabelText(moved[to].name) == name;
            },
            land);
        builder.StartRow("interrupt");
        AddText(builder, row + "/name", interrupt.name, {}, reorder);
        AddButton(builder, row + "/goto", interrupt.goTo, reorder, &vocab::kGoNow);
        AddButton(builder, row + "/edit", interrupt.edit, reorder);
        AddButton(builder, row + "/remove", interrupt.remove, reorder, &vocab::kRemove);
        builder.EndRow();
    }
    AddControl(builder, interrupts + "/add", parts.addInterrupt);
}

} // namespace fa::screens
