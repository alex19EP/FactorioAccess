#pragma once

// Any window no recipe claims (the main menu and its button menus, the changelog, confirmation
// dialogs): its content read generically as one stop, its dialog footer as a second.

#include <vector>

#include "WindowScreen.hpp"

namespace fa::screens
{

class GenericWindowScreen final : public WindowScreen
{
public:
    /// `recipes` are the screens whose windows this one leaves alone.
    explicit GenericWindowScreen(std::vector<const WindowScreen*> recipes) : _recipes(std::move(recipes)) {}

    bool Handles(const agui::Widget* window) const override;

protected:
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;

private:
    std::vector<const WindowScreen*> _recipes;
};

} // namespace fa::screens
