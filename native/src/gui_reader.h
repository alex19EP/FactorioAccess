#pragma once

namespace fa::agui {
struct Gui;
}

namespace fa::gui_reader {

// Called on the main thread right after every agui::Gui::logic, when the widget tree is settled
// for the frame. Rebuilds what to say from the live tree each time (immediate mode) and speaks
// what changed: newly shown windows, keyboard focus, and the widget under the mouse.
void afterGuiLogic(const agui::Gui* gui);

} // namespace fa::gui_reader
