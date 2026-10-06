#pragma once

namespace fa::agui {
struct Gui;
}

namespace fa::ui {

// Registers the screens. Call once before the hooks go live.
void start();

// Called on the main thread right after every agui::Gui::logic, when the widget tree is settled
// for the frame: runs the screen manager and the navigator for that Gui.
void afterGuiLogic(const agui::Gui* gui);

} // namespace fa::ui
