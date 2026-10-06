#include "MenuScreen.hpp"

#include <string>
#include <vector>

#include "AguiNodes.hpp"

namespace fa::screens
{

bool MenuScreen::Handles(const agui::Widget* window) const { return agui::menuParts(window).main != nullptr; }

void MenuScreen::BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window)
{
    // The buttons, top to bottom: Continue, the menu, Exit or Back.
    agui::MenuParts parts = agui::menuParts(window);
    const agui::Widget* title = agui::frameTitle(window);
    builder.BeginStop("menu");
    for (auto [key, part] : {std::pair{"menu/top", parts.top}, {"menu/main", parts.main}, {"menu/bottom", parts.bottom}})
        if (Shows(part))
            AddSubtree(builder, key, part, {title});

    // The main menu's panels beside it: the language and background selectors, the advert.
    std::vector<const agui::Widget*> panels = agui::mainMenuPanels(window);
    for (std::size_t i = 0; i < panels.size(); ++i)
    {
        if (!Shows(panels[i]))
            continue;
        std::string key = "panel/" + std::to_string(i);
        builder.BeginStop(key);
        AddSubtree(builder, key, panels[i]);
    }

    // The version, in the corner behind the main menu.
    const agui::Widget* version = agui::versionLabel();
    if (!panels.empty() && version && Shows(version) && !LabelText(version).empty())
    {
        builder.BeginStop("version");
        builder.AddItem(graph::ControlId::Referenced(version, "version"),
            TextNode(version, [version]() { return LabelText(version); }));
    }
}

} // namespace fa::screens
