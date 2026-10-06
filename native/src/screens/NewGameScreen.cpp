#include "NewGameScreen.hpp"

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

// The item whose Enter last pressed the window's confirm, compared and never read.
const Widget* g_confirmedItem = nullptr;

// The footer's last button is the window's confirm: Continue (or Play).
const Widget* ConfirmButton(const Widget* window)
{
    const Widget* footer = agui::dialogButtons(window);
    if (!footer)
        return nullptr;
    std::vector<const Widget*> buttons = FindAll(footer, "agui::Button");
    return buttons.empty() ? nullptr : buttons.back();
}

// A list whose items select on a click, the selected one confirming the window. Disabled items are
// group headings: the items after one are read in its context.
void AddSelectList(graph::GraphBuilder& builder, const std::string& prefix, const Widget* list, const Widget* window)
{
    bool inGroup = false;
    std::vector<const Widget*> items = agui::listBoxItems(list);
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        const Widget* item = items[i];
        if (!Shows(item))
            continue;
        if (!agui::enabled(item))
        {
            if (inGroup)
                builder.PopContext();
            builder.PushContext(text::speakable(agui::text(item)));
            inGroup = true;
            continue;
        }
        graph::NodeVtable vtable = TextNode(item, [item]() { return text::speakable(agui::text(item)); });
        auto selected = [item]() { return agui::buttonToggled(item) ? std::string(vocab::kSelected) : std::string(); };
        vtable.Announcements.emplace_back(selected, true, graph::AnnouncementKinds::Selected);
        vtable.OnActivate = [window, item]()
        {
            g_confirmedItem = nullptr;
            if (!agui::buttonToggled(item))
                agui::press(item, agui::MouseButton::Left, false, false);
            else if (const Widget* confirm = ConfirmButton(window))
            {
                g_confirmedItem = item;
                agui::press(confirm, agui::MouseButton::Left, false, false);
            }
        };
        // Selecting says "selected"; confirming leaves the window, whose successor speaks instead.
        vtable.StateText = [item, selected]() { return item == g_confirmedItem ? std::string() : selected(); };
        std::string key = prefix + "/" + std::to_string(i);
        builder.AddItem(graph::ControlId::Referenced(item, key), std::move(vtable));
        if (agui::buttonToggled(item))
            builder.SetStart(graph::ControlId::Structural(key));
    }
    if (inGroup)
        builder.PopContext();
}

} // namespace

bool NewGameScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "NewGameGui"); }

void NewGameScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    builder.BeginStop("scenarios");
    AddSelectList(builder, "scenarios", agui::member(window, layout.newGameMaps), window);

    const Widget* levels = agui::member(window, layout.newGameLevels);
    if (Shows(levels))
    {
        builder.BeginStop("levels");
        if (const Widget* list = FindDescendant(levels, "agui::ListBox"))
            AddSelectList(builder, "levels", list, window);
    }

    const Widget* difficulty = agui::member(window, layout.newGameDifficulty);
    if (Shows(difficulty))
    {
        builder.BeginStop("difficulty");
        AddSubtree(builder, "difficulty", difficulty);
    }

    // As laid out: the name with the replay checkbox and delete beside it, the description below.
    builder.BeginStop("details");
    const Widget* name = agui::member(window, layout.newGameName);
    const Widget* replay = agui::member(window, layout.newGameReplay);
    const Widget* remove = agui::member(window, layout.newGameDelete);
    builder.StartRow();
    builder.AddItem(graph::ControlId::Referenced(name, "details/name"),
        TextNode(name, [name]() { return LabelText(name); }));
    AddControl(builder, "details/replay", replay);
    AddControl(builder, "details/delete", remove);
    builder.EndRow();
    AddSubtree(builder, "details/description", agui::member(window, layout.newGameDescription));

    AddFooter(builder, window);

    builder.BeginStop("search");
    if (const Widget* search = FindDescendant(window, "SearchBar"))
        AddControl(builder, "search/bar", search);
    if (const Widget* popup = FindDescendant(window, "SearchPopup"))
        AddSubtree(builder, "search/popup", popup);
}

} // namespace fa::screens
