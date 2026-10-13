#include "InterruptScreen.hpp"

#include "ScheduleNodes.hpp"

namespace fa::screens
{

using agui::Widget;

namespace
{

// The label heading a scroll pane ("Conditions", "Targets"): the widget just before it.
const Widget* HeadingOf(const Widget* pane)
{
    const Widget* up = pane ? agui::parent(pane) : nullptr;
    if (!up)
        return nullptr;
    for (auto list : {agui::children(up), agui::privateChildren(up)})
    {
        const Widget* previous = nullptr;
        for (const Widget* child : list)
        {
            if (child == pane)
                return previous && agui::kind(previous) == agui::Kind::Label ? previous : nullptr;
            previous = child;
        }
    }
    return nullptr;
}

} // namespace

bool InterruptScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "EditInterruptGui"); }

void InterruptScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    agui::InterruptEditorParts parts = agui::interruptEditorParts(window);
    Lander land = [this](std::string key, std::function<bool()> ready) { LandWhen(std::move(key), std::move(ready)); };
    // Each heading names its own stop, said as the cursor comes into it.
    const Widget* conditionsHeading = HeadingOf(parts.conditionsScroll);
    const Widget* targetsHeading = HeadingOf(parts.targetsScroll);
    AddTitledWindow(builder, "interrupt", window,
        {parts.conditionsScroll, parts.targetsScroll, parts.addCondition, parts.addTarget, conditionsHeading,
            targetsHeading});

    auto stop = [&](const char* key, const Widget* heading, auto&& add)
    {
        builder.BeginStop(key);
        const bool named = heading && Shows(heading);
        if (named)
            builder.PushContext(LabelText(heading), "", false);
        add();
        if (named)
            builder.PopContext();
    };
    stop("conditions", conditionsHeading,
        [&]()
        {
            AddConditions(
                builder, "conditions", [window]() { return agui::interruptEditorParts(window).conditions; },
                parts.conditionsPane, parts.addCondition, land);
        });
    stop("targets", targetsHeading,
        [&]()
        {
            AddStations(
                builder, "targets", [window]() { return agui::interruptEditorParts(window).targets; },
                parts.targetsPane, parts.addTarget, land);
        });
}

} // namespace fa::screens
