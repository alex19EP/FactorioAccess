#include "SplitterScreen.hpp"

namespace fa::screens
{

bool SplitterScreen::Handles(const agui::Widget* window) const { return agui::derivesFrom(window, "SplitterGui"); }

void SplitterScreen::BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window)
{
    agui::EntityPanelParts parts = agui::entityPanelParts(window);
    AddTitledWindow(builder, "splitter", parts.titled, {parts.sidePanel});
    AddSidePanel(builder, parts.sidePanel);
}

} // namespace fa::screens
