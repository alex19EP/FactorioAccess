#pragma once

// An open dropdown's list, over whatever window holds the dropdown. Enter on a dropdown opens it
// as a click would; while its list is up (the game makes it the top modal), the graph holds only
// its options, starting on the current one. Enter picks an option as a click on it would, which
// closes the list; Escape goes to the game, which closes it unchanged. The window's screen sits
// underneath with its cursor kept, so focus returns to the dropdown.

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class DropDownScreen final : public nav::Screen
{
public:
    std::string Name() const override { return {}; }
    const char* DiagName() const override { return "dropdown list"; }
    int Layer() const override { return 1; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    void OnCursorMoved(const graph::GraphNode& node) override;

private:
    const agui::Widget* _dropDown = nullptr;
};

} // namespace fa::screens
