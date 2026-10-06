#include "ControlSettingsScreen.hpp"

#include <string>
#include <vector>

#include "AguiNodes.hpp"
#include "SettingsScreen.hpp"
#include "game.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;
using game::layout;

// The first label that says something: a section's title, a binding's action.
const Widget* FirstText(const Widget* widget)
{
    for (const Widget* label : FindAll(widget, "agui::Label"))
        if (!LabelText(label).empty())
            return label;
    return nullptr;
}

} // namespace

bool ControlSettingsScreen::Handles(const Widget* window) const
{
    return agui::derivesFrom(window, "ControlSettingsGui");
}

bool ControlSettingsScreen::ClaimsKeys() const
{
    return !Window() || !agui::pointerMember(Window(), layout.controlsSetting);
}

void ControlSettingsScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* content = agui::member(window, layout.settingsContent);
    const Widget* scrollPane = agui::member(window, layout.controlsScrollPane);

    builder.BeginStop("method");
    AddSubtree(builder, "method", content, {scrollPane, agui::member(window, layout.settingsReset)});

    builder.BeginStop("bindings");
    std::vector<const Widget*> panes = VisibleChildren(scrollPane);
    if (!panes.empty())
    {
        std::vector<const Widget*> sections = VisibleChildren(panes.front());
        for (std::size_t i = 0; i < sections.size(); ++i)
            BuildSection(builder, sections[i], i);
    }

    AddSettingsButtons(builder, window);
    AddSearchStop(builder, window);
}

void ControlSettingsScreen::BuildSection(graph::GraphBuilder& builder, const Widget* section, std::size_t index)
{
    std::string prefix = "bindings/" + std::to_string(index);
    builder.SetRegion(prefix);

    const Widget* title = FirstText(section);
    const Widget* fold = FindDescendant(section, "ExpandCollapseButtons");
    if (title && fold)
    {
        // The fold's two buttons: expand, shown while collapsed, and collapse.
        std::vector<const Widget*> buttons = FindAll(fold, "agui::Button");
        graph::NodeVtable header = TextNode(title, [title]() { return LabelText(title); });
        header.Announcements.emplace_back(
            [fold]()
            {
                std::span<const Widget* const> both = agui::children(fold);
                return vocab::expandedState(both.size() > 1 && agui::visible(both[1]));
            },
            true, graph::AnnouncementKinds::Value);
        header.OnActivate = [fold]()
        {
            for (const Widget* button : FindAll(fold, "agui::Button"))
            {
                agui::press(button, agui::MouseButton::Left, false, false);
                return;
            }
        };
        builder.AddItem(graph::ControlId::Referenced(title, prefix), std::move(header));
    }

    std::vector<const Widget*> lines = FindAll(section, "ControlSettingsGui::Section::SubSection::Line");
    if (lines.empty())
    {
        // Not key bindings (the mouse and vehicle options): read as it stands, minus the header.
        AddSubtree(builder, prefix + "/content", section, {title ? agui::parent(title) : nullptr});
        return;
    }
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        const Widget* action = FirstText(lines[i]);
        std::vector<const Widget*> keys = FindAll(lines[i], "agui::TextButton");
        if (!action || keys.empty())
            continue;
        std::string key = prefix + "/" + std::to_string(i);
        builder.StartRow("binding");
        const Widget* primary = keys[0];
        AddControl(builder, key + "/1", primary,
            [action, primary]() { return LabelText(action) + ", " + OwnText(primary); });
        if (keys.size() > 1)
        {
            const Widget* alternative = keys[1];
            AddControl(builder, key + "/2", alternative,
                [alternative]() { return OwnText(alternative) + ", " + std::string(vocab::kAlternative); });
        }
        builder.EndRow();
    }
}

} // namespace fa::screens
