#include "InterruptScreen.hpp"

#include "ScheduleNodes.hpp"

namespace fa::screens
{

using agui::Widget;

bool InterruptScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "EditInterruptGui"); }

void InterruptScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    agui::InterruptEditorParts parts = agui::interruptEditorParts(window);
    AddTitledWindow(builder, "interrupt", window,
        {parts.conditionsScroll, parts.targetsScroll, parts.addCondition, parts.addTarget});
    builder.BeginStop("conditions");
    AddConditions(builder, "conditions", parts.conditions, parts.addCondition);
    builder.BeginStop("targets");
    AddStations(builder, "targets", parts.targets, parts.addTarget);
}

} // namespace fa::screens
