#include "ScheduleNodes.hpp"

#include <format>
#include <vector>

#include "AguiNodes.hpp"
#include "vocab.h"

namespace fa::screens
{

using agui::Widget;

namespace
{

void AddText(
    graph::GraphBuilder& builder, const std::string& key, const Widget* label, std::function<std::string()> text = {})
{
    if (!text)
        text = [label]() { return LabelText(label); };
    builder.AddItem(graph::ControlId::Referenced(label, key), TextNode(label, std::move(text)));
}

void AddStation(graph::GraphBuilder& builder, const std::string& key, const agui::ScheduleStation& station)
{
    builder.SetRegion(key);
    builder.StartRow("station");
    const Widget* label = station.label;
    const Widget* goTo = station.goTo;
    AddText(builder, key + "/name", label,
        [label, goTo]()
        {
            std::string text = LabelText(label);
            if (agui::buttonToggled(goTo))
                text += ", " + std::string(vocab::kCurrentStop);
            return text;
        });
    AddControl(builder, key + "/goto", goTo);
    AddControl(builder, key + "/change", station.change);
    AddControl(builder, key + "/remove", station.remove);
    AddControl(builder, key + "/unload", station.unload);
    builder.EndRow();

    // In the station's context, said as the cursor comes in from another station. Stations of one
    // name are never entered one from the other: the next station's own row lies between them.
    std::string name = LabelText(label);
    if (!name.empty())
        builder.PushContext(name, "", false);
    for (std::size_t i = 0; i < station.conditions.size(); ++i)
    {
        const Widget* condition = station.conditions[i];
        if (!Shows(condition))
            continue;
        builder.StartRow("condition");
        AddRowItems(
            builder, std::format("{}/condition/{}", key, i), {condition}, {agui::scheduleConditionDrag(condition)});
        builder.EndRow();
    }
    AddControl(builder, key + "/add", station.addCondition);
    if (!name.empty())
        builder.PopContext();
}

} // namespace

void AddSchedule(graph::GraphBuilder& builder, const std::string& key, const Widget* schedule)
{
    agui::ScheduleParts parts = agui::scheduleParts(schedule);

    builder.BeginStop(key);
    if (parts.groupNote && Shows(parts.groupNote))
        AddText(builder, key + "/note", parts.groupNote);
    for (std::size_t i = 0; i < parts.stations.size(); ++i)
        if (Shows(parts.stations[i].label))
            AddStation(builder, std::format("{}/station/{}", key, i), parts.stations[i]);
    builder.SetRegion(key + "/end");
    AddControl(builder, key + "/add", parts.addStation);

    std::string interrupts = key + "/interrupts";
    builder.BeginStop(interrupts);
    for (std::size_t i = 0; i < parts.interrupts.size(); ++i)
    {
        const agui::ScheduleInterrupt& interrupt = parts.interrupts[i];
        if (!Shows(interrupt.name))
            continue;
        std::string row = std::format("{}/{}", interrupts, i);
        builder.StartRow("interrupt");
        AddText(builder, row + "/name", interrupt.name);
        AddControl(builder, row + "/goto", interrupt.goTo);
        AddControl(builder, row + "/edit", interrupt.edit);
        AddControl(builder, row + "/remove", interrupt.remove);
        builder.EndRow();
    }
    AddControl(builder, interrupts + "/add", parts.addInterrupt);
}

} // namespace fa::screens
