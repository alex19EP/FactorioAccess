#pragma once

// Base of the screens for the application Gui's menu windows: the main menu, Settings, Mods, Load
// game and whatever else the menus open, and over a loaded game the game menu (Escape) and what it
// opens. Each screen is a recipe for the windows it Handles(); GenericWindowScreen takes every
// window no recipe claims.
//
// Only the topmost window is navigable, or the one holding an open modal, as for a sighted
// player. Over a loaded game that is the window of the menu state on top of the app's state stack,
// and nothing at all during plain play, so the game's own windows stay with the mod. A screen stays one window: when its window is replaced (the main menu opening Single
// player), it goes inactive for a frame, so the manager pops it and attaches afresh, landing on
// the new window's start.
//
// Cursor write-back (P11) gives the game's keyboard focus to the widget under our cursor and
// scrolls it into view. The mouse is never touched.

#include <string>

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class WindowScreen : public nav::Screen
{
public:
    /// The Gui whose logic() is running, set before every manager update. Screens are only active
    /// while that is the application Gui.
    static void SetGui(const agui::Gui* gui) { s_gui = gui; }

    /// The window that would be navigable now, or null.
    static const agui::Widget* TopWindow();

    const char* Name() const override { return ""; }
    const char* DiagName() const override { return _class.c_str(); }
    bool RemembersCursor() const override { return true; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    bool TypingIn(const graph::GraphNode& node) override;
    void OnCursorMoved(const graph::GraphNode& node) override;
    void OnPop() override;

    /// Whether this recipe reads the window.
    virtual bool Handles(const agui::Widget* window) const = 0;

protected:
    /// Declares the window's stops, inside the window title's context.
    virtual void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) = 0;

    /// The footer of a dialog window (Back, Confirm, ...) as a stop of its own, a row.
    static void AddFooter(graph::GraphBuilder& builder, const agui::Widget* window);

    /// The window this screen is reading; valid during Build and the hooks after it.
    const agui::Widget* Window() const { return _window; }

    static const agui::Gui* s_gui;

private:
    const agui::Widget* _window = nullptr;
    std::string _class;
};

} // namespace fa::screens
