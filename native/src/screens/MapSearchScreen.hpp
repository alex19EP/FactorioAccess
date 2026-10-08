#pragma once

// The map search in remote view: the search field that the game's focus-search control (Ctrl+F)
// opens under remote view's title bar, and the results the game lists at the right as it is typed
// (recipes in machines, map tags, train stops, resource patches, tiles). Active while the field
// shows.
//
// One stop. The field first, where typing searches as in vanilla; Down leaves it for the results
// under it, a line per result as the game words it ("[item=iron-ore] Iron ore 402k"). Enter clicks a
// result as the mouse would, which moves the camera there, and the FA cursor with it; Right reaches
// the result's pin button. Escape closes the search, as clicking the title bar's search button
// does, and leaves the player on the map wherever the last result took them, for the scanner to
// scan around.

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class MapSearchScreen final : public nav::Screen
{
public:
    std::string Name() const override { return {}; }
    const char* DiagName() const override { return "map search"; }
    // Over the map, as the parts of the HUD are.
    int Layer() const override { return 1; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    const char* TakeSuggestedLanding() override;
    bool TypingIn(const graph::GraphNode& node) override;
    void OnCursorMoved(const graph::GraphNode& node) override;
    bool ClaimsEscape() const override { return true; }
    void OnEscape() override;
    void OnPush() override;
    void OnPop() override;
    std::string LeaveLine() const override;

private:
    const agui::Widget* _field = nullptr;
    bool _landOnField = false;
};

} // namespace fa::screens
