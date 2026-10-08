#include "ElectricNetworkScreen.hpp"

#include <string>
#include <vector>

namespace fa::screens
{

namespace
{

void AddColumn(graph::GraphBuilder& builder, const std::string& key, const agui::Widget* frame,
    const std::vector<const agui::Widget*>& graphs)
{
    if (!Shows(frame) || !HasContent(frame))
        return;
    builder.BeginStop(key);
    AddSubtree(builder, key, frame, graphs);
}

} // namespace

bool ElectricNetworkScreen::Handles(const agui::Widget* window) const
{
    return agui::derivesFromTemplate(window, "ElectricNetworkGuiWindow");
}

void ElectricNetworkScreen::BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window)
{
    agui::ElectricNetworkParts parts = agui::electricNetworkParts(window);
    AddTitledWindow(builder, "network", window, {parts.flows});
    AddColumn(builder, "production", parts.production, parts.graphs);
    AddColumn(builder, "consumption", parts.consumption, parts.graphs);
    AddColumn(builder, "storage", parts.storage, parts.graphs);
    if (parts.unitNumber)
        AddModViews(builder, parts.unitNumber);
}

} // namespace fa::screens
