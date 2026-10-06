#include "MapGeneratorScreen.hpp"

#include <string>
#include <vector>

#include "AguiNodes.hpp"
#include "game.h"

namespace fa::screens
{

namespace
{

using agui::Widget;
using game::layout;

// The label a control's row starts with ("Map seed" before its field), or null.
const Widget* RowLabel(const Widget* control)
{
    const Widget* row = agui::parent(control);
    return row ? FindDescendant(row, "agui::Label") : nullptr;
}

} // namespace

bool MapGeneratorScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "MapGeneratorGui"); }

void MapGeneratorScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    builder.BeginStop("preset");
    // The preset dropdown is always there, so the row is never empty.
    builder.StartRow();
    AddControl(builder, "preset/preset", agui::member(window, layout.mapGenPresets));
    AddControl(builder, "preset/reset", agui::member(window, layout.mapGenPresetReset));
    const Widget* seed = agui::member(window, layout.mapGenSeed);
    if (Shows(seed))
    {
        const Widget* seedLabel = RowLabel(seed);
        builder.AddItem(graph::ControlId::Referenced(seed, "preset/seed"),
            seedLabel ? ControlNode(seed, seedLabel) : ControlNode(seed));
    }
    AddControl(builder, "preset/random", agui::member(window, layout.mapGenRandomSeed));
    builder.EndRow();
    AddSubtree(builder, "preset/description", agui::member(window, layout.mapGenPresetDescription));

    const Widget* tabs = agui::member(window, layout.mapGenTabs);
    builder.BeginStop("tabs");
    if (const Widget* header = FindDescendant(tabs, "agui::Table"))
        AddSubtree(builder, "tabs", header);

    // Every page is a member of the window, but only the selected tab's page hangs in its tree; the
    // others keep their visible flag while detached.
    builder.BeginStop("settings");
    for (std::size_t p = 0; p < std::size(layout.mapGenPages); ++p)
    {
        const Widget* page = agui::member(window, layout.mapGenPages[p]);
        if (!Shows(page) || !Contains(window, page))
            continue;
        const Widget* content = FindDescendant(page, "agui::VerticalFlow");
        if (!content)
            continue;
        std::vector<const Widget*> sections = VisibleChildren(content);
        for (std::size_t i = 0; i < sections.size(); ++i)
        {
            std::string key = "settings/" + std::to_string(p) + "/" + std::to_string(i);
            builder.SetRegion(key);
            AddSubtree(builder, key, sections[i]);
        }
        builder.SetRegion("");
    }

    builder.BeginStop("exchange");
    const Widget* import = agui::member(window, layout.mapGenImport);
    const Widget* exportButton = agui::member(window, layout.mapGenExport);
    // "Map exchange string" heads the row its two buttons sit in.
    const Widget* buttons = agui::parent(import);
    const Widget* heading = buttons ? RowLabel(buttons) : nullptr;
    std::string headingText = heading ? LabelText(heading) : std::string();
    if (Shows(import) || Shows(exportButton))
    {
        if (!headingText.empty())
            builder.PushContext(headingText);
        builder.StartRow();
        AddControl(builder, "exchange/import", import);
        AddControl(builder, "exchange/export", exportButton);
        builder.EndRow();
        if (!headingText.empty())
            builder.PopContext();
    }

    builder.BeginStop("buttons");
    AddSubtree(builder, "buttons", agui::member(window, layout.mapGenButtons));
}

} // namespace fa::screens
