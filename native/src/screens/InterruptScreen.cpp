#include "InterruptScreen.hpp"

#include "ScheduleNodes.hpp"

namespace fa::screens
{

using agui::Widget;

bool InterruptScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "EditInterruptGui"); }

void InterruptScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    agui::InterruptEditorParts parts = agui::interruptEditorParts(window);
    Lander land = [this](std::string key, std::function<bool()> ready) { LandWhen(std::move(key), std::move(ready)); };
    AddTitledWindow(builder, "interrupt", window,
        {parts.conditionsScroll, parts.targetsScroll, parts.addCondition, parts.addTarget});
    builder.BeginStop("conditions");
    AddConditions(
        builder, "conditions", [window]() { return agui::interruptEditorParts(window).conditions; },
        parts.conditionsPane, parts.addCondition, land);
    builder.BeginStop("targets");
    AddStations(
        builder, "targets", [window]() { return agui::interruptEditorParts(window).targets; }, parts.targetsPane,
        parts.addTarget, land);
}

} // namespace fa::screens
