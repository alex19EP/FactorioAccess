#pragma once

// Base of the screens for the game's windows of entities over a loaded game, opened by its own
// open-gui control. Each subclass is the recipe for the window classes it Handles(), and reads the
// topmost such window while the game menu is closed. A screen stays one window: when its window is
// replaced (choosing a recipe opens the assembler's own window), it goes inactive for a frame, so
// the manager pops it and attaches afresh.
//
// The elements a mod attached to the window (LuaGuiElement anchor), FA's own views of the entity
// among them, follow the window's stops, a stop each.

#include <string>
#include <vector>

#include "AguiNodes.hpp"
#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class EntityWindowScreen : public nav::Screen
{
public:
    const char* Name() const override { return ""; }
    const char* DiagName() const override { return _class.c_str(); }
    bool RemembersCursor() const override { return true; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    bool TypingIn(const graph::GraphNode& node) override;
    void OnCursorMoved(const graph::GraphNode& node) override;
    void OnPop() override;

protected:
    /// Whether this recipe reads the window.
    virtual bool Handles(const agui::Widget* window) const = 0;

    /// Declares the window's stops.
    virtual void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) = 0;

    /// A window titled with the entity's name as one stop, read in the title's context: its content,
    /// then the title bar's network buttons, without the title (the context already), close
    /// (Escape) or search. Widgets in `skip` are left out.
    static void AddTitledWindow(graph::GraphBuilder& builder, const std::string& key, const agui::Widget* window,
        std::vector<const agui::Widget*> skip = {}, Attachments attachments = {});

    /// The player's inventory beside the window, in its title's context ("Character"), as a stop of
    /// its own.
    static void AddInventory(graph::GraphBuilder& builder, const agui::EntityWindowParts& parts);
    /// The panel a network button in the title bar opened beside the window, as a stop of its own,
    /// while one is open.
    static void AddSidePanel(graph::GraphBuilder& builder, const agui::Widget* sidePanel);

private:
    /// The window, and the wrapper the game put it in when a mod attached elements to it.
    struct Found
    {
        const agui::Widget* window = nullptr;
        const agui::Widget* wrapper = nullptr;
    };
    Found FindWindow() const;

    /// The mod elements in the wrapper's flows, a stop each.
    static void AddRelativeElements(graph::GraphBuilder& builder, const agui::Widget* wrapper);

    const agui::Widget* _window = nullptr;
    std::string _class;
};

} // namespace fa::screens
