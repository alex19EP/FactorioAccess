#include "GenericWindowScreen.hpp"

#include <algorithm>

#include "AguiNodes.hpp"

namespace fa::screens
{

bool GenericWindowScreen::Handles(const agui::Widget* window) const
{
    return std::ranges::none_of(_recipes, [window](const WindowScreen* recipe) { return recipe->Handles(window); });
}

void GenericWindowScreen::BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window)
{
    builder.BeginStop("content");
    AddSubtree(builder, "content", window, {agui::frameTitle(window), agui::dialogButtons(window)});
    AddFooter(builder, window);
}

} // namespace fa::screens
