#pragma once

// The scenario's message dialog over a loaded game (show_message_dialog; the freeplay welcome).
// It is a speech bubble rather than a window, so no window screen sees it. Its text reads a line
// per node, followed by "continue", which closes it as the game's Confirm message control does.
// Tab, the key the message names, stays the game's, so Confirm message runs as for anyone.
// The game draws the bubble over its windows, pointing at one (the tutorial's recipes), so the
// message takes the keys before an open window; the game menu (Escape) takes them while it is up,
// then hands them back.

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class ScenarioMessageScreen final : public nav::Screen
{
public:
    std::string Name() const override { return {}; }
    const char* DiagName() const override { return "scenario message"; }
    int Layer() const override { return 1; }
    bool GameTab() const override { return true; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    void OnPop() override;

private:
    const agui::Widget* _message = nullptr;
    // Closing frees the bubble the render points into, so it waits for the next poll, where the
    // screen then goes inactive before anything reads the render again.
    bool _confirm = false;
};

} // namespace fa::screens
