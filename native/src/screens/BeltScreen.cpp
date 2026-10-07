#include "BeltScreen.hpp"

namespace fa::screens
{

bool BeltScreen::Handles(const agui::Widget* window) const { return agui::derivesFrom(window, "TransportBeltGui"); }

void BeltScreen::BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window)
{
    agui::EntityPanelParts parts = agui::entityPanelParts(window);
    AddTitledWindow(builder, "belt", parts.titled, {parts.sidePanel});
    AddSidePanel(builder, parts.sidePanel);
    AddModViews(builder, parts.unitNumber);
}

} // namespace fa::screens
