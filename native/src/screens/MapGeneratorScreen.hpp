#pragma once

// The Map generator window (MapGeneratorGui), opened from New game. Stops:
//   preset   — the preset, its reset button, the map seed and the random seed button, then the
//              preset's description
//   tabs     — Resources, Terrain, Enemy, Advanced
//   settings — the selected tab's page, a region per table or section (Ctrl+Up/Down). Its tables
//              are header grids: a row per resource (named with the planets it is on), a cell per
//              column, Up/Down keeping the column; a slider in them adjusts after Enter. A setting
//              with a slider and a value field reads the field and steps the slider on Left/Right;
//              Enter types into the field
//   exchange — map exchange string import and export
//   buttons  — Back, Preview and Play

#include "WindowScreen.hpp"

namespace fa::screens
{

class MapGeneratorScreen final : public WindowScreen
{
public:
    bool Handles(const agui::Widget* window) const override;

protected:
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
