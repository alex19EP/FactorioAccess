#pragma once

// Settings > Controls (ControlSettingsGui). Stops:
//   method   — keyboard and mouse or controller
//   bindings — every section in turn (Ctrl+Up/Down jumps between them). A section's header says
//              whether it is expanded and Enter folds it; each key binding is a row, "Move up, W"
//              then its alternative key. Enter on a key waits for the new key, which then goes
//              to the game untouched; Backspace clears it (the game's right click).
//   buttons  — reset and the footer
//   search   — the search field

#include "WindowScreen.hpp"

namespace fa::screens
{

class ControlSettingsScreen final : public WindowScreen
{
public:
    bool Handles(const agui::Widget* window) const override;
    /// While a binding waits for its new key, every key is the game's.
    bool ClaimsKeys() const override;

protected:
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;

private:
    void BuildSection(graph::GraphBuilder& builder, const agui::Widget* section, std::size_t index);
};

} // namespace fa::screens
