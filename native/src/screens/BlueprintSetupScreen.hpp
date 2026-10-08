#pragma once

// A blueprint's setup window (BlueprintSetupGui), opened by a right click on the blueprint in a
// slot or by selecting an area with a blank one: its settings beside the preview picture, and the
// button that saves or creates the blueprint.
//
// Stops, in the window's title context:
//   - the blueprint: its name (Enter starts renaming in the game's own field, Enter again confirms),
//     then the buttons for it in a row (new contents, copy, upgrade, parametrise, export, delete);
//   - its icons, a row of four ("Icon, 1, assembling machine"), its description, and the button in
//     it that puts an icon in the text;
//   - snap to grid: the checkbox, then the snapping table top to bottom, each line in the context
//     of its name ("Grid size, Width:, 4, edit"); its fields read disabled while the box is clear,
//     as the game greys them;
//   - the components, ten to a row as the game lays them out: ] takes every entity of one kind out
//     of the blueprint, Enter puts them back, as the right and left mouse buttons do. One taken out
//     reads "inserter 0, removed", where the game shows the slot red;
//   - what to include (modules, tiles, trains, fuel, ...), only while the game shows it;
//   - the preview, its header and the game's hint, then the picture as a canvas: a cursor over the
//     blueprint's tiles (see PreviewCanvas.hpp);
//   - the footer's save or create button.
//
// Every control is the game's own, pressed as the Gui presses it, so what it does is vanilla.

#include "EntityWindowScreen.hpp"
#include "PreviewCanvas.hpp"

namespace fa::screens
{

class BlueprintSetupScreen final : public EntityWindowScreen
{
protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;

private:
    PreviewCanvas _canvas;
};

} // namespace fa::screens
