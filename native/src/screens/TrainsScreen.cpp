#include "TrainsScreen.hpp"

#include <format>

#include "TrainNodes.hpp"

namespace fa::screens
{

using agui::Widget;

bool TrainsScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "TrainsGui"); }

void TrainsScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    agui::TrainsOverviewParts parts = agui::trainsOverviewParts(window);
    AddTitledWindow(builder, "overview", window, {parts.tabs});
    if (const Widget* header = FindDescendant(parts.tabs, "agui::Table"))
        AddSubtree(builder, "overview/tabs", header);

    builder.BeginStop("list");
    AddSubtree(builder, "list", parts.list);

    builder.BeginStop("cards");
    if (parts.trains)
    {
        AddTrainCards(builder, "cards", parts.trains);
        return;
    }
    bool any = false;
    for (std::size_t i = 0; i < parts.stops.size(); ++i)
    {
        if (!Shows(parts.stops[i]))
            continue;
        any = true;
        builder.StartRow("stop");
        AddRowItems(builder, std::format("cards/{}", i), {parts.stops[i]});
        builder.EndRow();
    }
    if (!any && parts.noStops)
        AddSubtree(builder, "cards/none", parts.noStops);
}

} // namespace fa::screens
