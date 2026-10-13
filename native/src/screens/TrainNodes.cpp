#include "TrainNodes.hpp"

#include <format>
#include <vector>

#include "AguiNodes.hpp"
#include "game.h"

namespace fa::screens
{

using agui::Widget;

void AddTrainCards(graph::GraphBuilder& builder, const std::string& prefix, const Widget* trainsWidget)
{
    std::vector<agui::TrainCard> cards = agui::trainCards(trainsWidget);
    bool any = false;
    for (std::size_t i = 0; i < cards.size(); ++i)
    {
        const agui::TrainCard& card = cards[i];
        if (!Shows(card.status))
            continue;
        any = true;
        std::string key = std::format("{}/{}", prefix, i);
        // One row key for every card, so Up and Down keep to the column.
        builder.StartRow(prefix);
        AddControl(builder, key + "/status", card.status);
        AddControl(builder, key + "/open", card.open);
        builder.EndRow();
    }
    if (any)
        return;
    const Widget* empty = agui::trainsWidgetEmpty(trainsWidget);
    AddSubtree(builder, prefix + "/empty", empty && Shows(empty) ? empty : trainsWidget);
}

void AddTrainTabs(graph::GraphBuilder& builder, const std::string& key, const Widget* tabs)
{
    builder.BeginStop(key);
    if (const Widget* header = FindDescendant(tabs, "agui::Table"))
        AddSubtree(builder, key + "/tabs", header);
    const Widget* page = agui::member(tabs, game::layout.tabbedPaneContent);
    if (const Widget* trains = FindDescendant(page, "TrainsWidget"))
        AddTrainCards(builder, key + "/trains", trains);
    else
        AddSubtree(builder, key + "/page", page);
}

} // namespace fa::screens
