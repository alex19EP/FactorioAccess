#include "PipeScreen.hpp"

namespace fa::screens
{

bool PipeScreen::Handles(const agui::Widget* window) const
{
    return agui::derivesFrom(window, "SingleFluidBoxEntityGui");
}

void PipeScreen::BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window)
{
    agui::EntityPanelParts parts = agui::entityPanelParts(window);
    agui::FluidBoxParts fluid = agui::fluidBoxParts(window);
    AddTitledWindow(builder, "pipe", parts.titled, {parts.sidePanel, fluid.icon, fluid.bar});
    AddSidePanel(builder, parts.sidePanel);
}

} // namespace fa::screens
