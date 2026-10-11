#pragma once

// A tooltip too long to hear at once, read a line at a time. The tooltip key (Y) says a tooltip of up
// to three lines; a longer one opens here, over the screen it came from, on its first line. Up and
// Down read its lines, Ctrl+Up and Ctrl+Down move between the sections the game draws a line
// between (a recipe's ingredients, its total raw), and Y or Escape closes it.
//
// It is what the game said when Y was pressed. It closes too when the screen it came from goes
// away.

#include <vector>

#include "AguiNodes.hpp"
#include "navigator/Screen.hpp"

namespace fa::screens
{

/// The tooltip key's answer: up to three lines said at once, more read in TooltipScreen, none said
/// as no tooltip. Main thread, from a key the navigator handles.
void ReadTooltip(std::vector<TooltipLine> lines);

class TooltipScreen final : public nav::Screen
{
public:
    // The landing on its first line, the tooltip's title, introduces it.
    std::string Name() const override { return {}; }
    const char* DiagName() const override { return "tooltip"; }
    // Over any screen a tooltip comes from, a dropdown's list too.
    int Layer() const override { return 2; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    bool ClaimsEscape() const override { return true; }
    void OnEscape() override;
    void OnPop() override;
};

} // namespace fa::screens
