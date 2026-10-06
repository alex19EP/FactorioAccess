#include "LoadGameScreen.hpp"

#include <string>
#include <vector>

#include "AguiNodes.hpp"
#include "game.h"
#include "text.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;
using game::layout;

// "erik, 9 months ago": the save's name and the age the list shows beside it.
std::string SaveLabel(const Widget* save)
{
    std::string label = text::speakable(agui::text(save));
    for (const Widget* remark : FindAll(save, "agui::Label"))
    {
        std::string phrase = LabelText(remark);
        if (!phrase.empty())
            label += ", " + phrase;
    }
    return label;
}

// The footer's last button is the window's confirm: Load.
const Widget* ConfirmButton(const Widget* window)
{
    const Widget* footer = agui::dialogButtons(window);
    if (!footer)
        return nullptr;
    std::vector<const Widget*> buttons = FindAll(footer, "agui::Button");
    return buttons.empty() ? nullptr : buttons.back();
}

} // namespace

bool LoadGameScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "LoadMapGui"); }

void LoadGameScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* list = agui::member(window, layout.loadMapList);
    const Widget* info = agui::member(window, layout.loadMapInfo);
    const Widget* remove = agui::member(info, layout.mapInfoDelete);

    builder.BeginStop("saves");
    std::vector<const Widget*> saves = agui::listBoxItems(list);
    for (std::size_t i = 0; i < saves.size(); ++i)
    {
        const Widget* save = saves[i];
        if (!Shows(save))
            continue;
        graph::NodeVtable vtable = TextNode(save, [save]() { return SaveLabel(save); });
        auto selected = [save]() { return agui::buttonToggled(save) ? std::string(vocab::kSelected) : std::string(); };
        vtable.Announcements.emplace_back(selected, true, graph::AnnouncementKinds::Selected);
        // A click selects, as in vanilla; on the selected save it loads, as a double-click does.
        vtable.OnActivate = [window, save]()
        {
            if (!agui::buttonToggled(save))
                agui::press(save, agui::MouseButton::Left, false, false);
            else if (const Widget* confirm = ConfirmButton(window))
                agui::press(confirm, agui::MouseButton::Left, false, false);
        };
        vtable.StateText = selected;
        // The delete button acts on the selected save, which may not be this one.
        vtable.OnSecondary = [save, remove]()
        {
            if (agui::buttonToggled(save) && Shows(remove) && agui::enabled(remove))
                agui::press(remove, agui::MouseButton::Left, false, false);
        };
        // Keyed by position, not name: a refreshed list keeps the cursor on the same row.
        std::string key = "saves/" + std::to_string(i);
        builder.AddItem(graph::ControlId::Referenced(save, key), std::move(vtable));
        if (agui::buttonToggled(save))
            builder.SetStart(graph::ControlId::Structural(key));
    }

    builder.BeginStop("details");
    AddSubtree(builder, "details", info);

    AddFooter(builder, window);

    builder.BeginStop("search");
    if (const Widget* search = FindDescendant(window, "SearchBar"))
        AddControl(builder, "search/bar", search);
    if (const Widget* popup = FindDescendant(window, "SearchPopup"))
        AddSubtree(builder, "search/popup", popup);
    if (const Widget* sort = FindDescendant(window, "SortModeSelector"))
        AddSubtree(builder, "search/sort", sort);
}

} // namespace fa::screens
