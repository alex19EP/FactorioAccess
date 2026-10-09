#pragma once

// The game's info panel for what the cursor points at (see selectedinfo.h), opened by the Y key in
// the world: the entity's name, status, recipe, contents, power, health and the rest (in the map
// editor a tile's too), as the game shows them beside the mouse. Up and Down read it a line at a time; the generic
// walker reads its rows and tables as it does any window's.
//
// It is what the game said when Y was pressed. It closes on Escape, when the cursor points at
// something else or what it described is gone, and when a window opens over the map or Ctrl+Tab
// moves to a part of the HUD.

#include "navigator/Screen.hpp"

namespace fa::screens
{

class SelectedInfoScreen final : public nav::Screen
{
public:
    // The landing on its first line, the name, introduces it.
    std::string Name() const override { return {}; }
    const char* DiagName() const override { return "selected info"; }
    // Over the map, as the parts of the HUD are.
    int Layer() const override { return 1; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    bool ClaimsEscape() const override { return true; }
    void OnEscape() override;
    void OnPush() override { _escaped = false; }
    void OnPop() override;
    std::string LeaveLine() const override;

private:
    bool _escaped = false;
};

} // namespace fa::screens
