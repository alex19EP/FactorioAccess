#include "SaveGameScreen.hpp"

#include "AguiNodes.hpp"
#include "SaveLists.hpp"
#include "game.h"

namespace fa::screens
{

using agui::Widget;
using game::layout;

bool SaveGameScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "SaveMapGui"); }

void SaveGameScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* info = agui::member(window, layout.saveMapInfo);
    AddSaves(
        builder, window, agui::pointerMember(window, layout.saveMapList), agui::member(info, layout.mapInfoDelete));

    // The subheader over the list: "Save as", the name field, the sort order buttons.
    builder.BeginStop("name");
    const Widget* field = agui::member(window, layout.saveMapNameField);
    const Widget* subheader = agui::member(agui::member(window, layout.saveMapLeftPart), layout.frameSubheader);
    if (const Widget* label = FindDescendant(subheader, "agui::Label"))
        builder.AddItem(graph::ControlId::Referenced(field, "name/field"), ControlNode(field, label));
    else
        AddControl(builder, "name/field", field);

    builder.BeginStop("details");
    AddSubtree(builder, "details", info);

    AddFooter(builder, window);

    AddSaveSearch(builder, window);
}

} // namespace fa::screens
