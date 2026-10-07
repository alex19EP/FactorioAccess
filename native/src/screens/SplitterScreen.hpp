#pragma once

// The game's window for a splitter (SplitterGui), opened by its own open-gui control: the input and
// output priority, each a checkbox that turns it on and a switch between left and right, and the
// filter slot that sends an item to the output priority side. The circuit network button in the
// title bar opens its panel beside the window.
//
// Stops: the window, then the open network panel.

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class SplitterScreen final : public EntityWindowScreen
{
protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
