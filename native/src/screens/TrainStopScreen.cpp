#include "TrainStopScreen.hpp"

#include <vector>

#include "TrainNodes.hpp"

namespace fa::screens
{

using agui::Widget;

bool TrainStopScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "TrainStopGui"); }

void TrainStopScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* tabs = agui::trainStopTabs(window);
    agui::EntityPanelParts panel = agui::entityPanelParts(window);
    std::vector<const Widget*> skip{tabs};
    if (panel.sidePanel)
        skip.push_back(panel.sidePanel);
    AddTitledWindow(builder, "stop", window, std::move(skip));
    AddTrainTabs(builder, "trains", tabs);
    if (panel.sidePanel)
        AddSidePanel(builder, panel.sidePanel);
}

} // namespace fa::screens
