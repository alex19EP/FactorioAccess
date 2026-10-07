#pragma once

// The game's window for a pipe, a pipe to ground or a storage tank (SingleFluidBoxEntityGui),
// opened by its own open-gui control.
//
// The window (a FluidBoxGui inside): the entity's status, the fluid with how full it is, its
// temperature and the range a building's filter allows, what this entity and the whole pipeline
// hold, each with its flush button, the visualize pipeline checkbox, and the buildings whose filters
// set the pipeline's fluid, each group a slot that moves the camera to them. A storage tank's title
// bar adds the circuit network button. The fluid's icon and the bar beside its line are left out:
// the line says the same ("Water 100%").
//
// Stops: the window, the open network panel, then the mod's views (scripts/ui/fluid-views.lua):
// where this entity connects, and what else the pipeline reaches.

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class PipeScreen final : public EntityWindowScreen
{
protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
