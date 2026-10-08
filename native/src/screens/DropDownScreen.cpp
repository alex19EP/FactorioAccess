#include "DropDownScreen.hpp"

#include <string>
#include <vector>

#include "AguiNodes.hpp"
#include "vocab.h"

namespace fa::screens
{

bool DropDownScreen::IsActive()
{
    // In a menu window or a game window alike: a list is open only while a window holds it.
    _dropDown = agui::openDropDown(agui::applicationGui());
    return _dropDown != nullptr;
}

void DropDownScreen::Build(graph::GraphBuilder& builder)
{
    // Choosing an option closes the list between IsActive and this build.
    if (!_dropDown || agui::openDropDown(agui::applicationGui()) != _dropDown)
        return;
    const agui::Widget* dropDown = _dropDown;
    std::vector<const agui::Widget*> options = agui::listBoxItems(agui::dropDownList(dropDown));
    for (std::size_t i = 0; i < options.size(); ++i)
    {
        const agui::Widget* option = options[i];
        if (!Shows(option))
            continue;
        graph::NodeVtable vtable = TextNode(option, [option]() { return OwnText(option); });
        int index = static_cast<int>(i);
        vtable.Announcements.emplace_back(
            [dropDown, index]()
            {
                return agui::dropDownSelected(dropDown) == index ? std::string(vocab::kSelected) : std::string();
            },
            false, graph::AnnouncementKinds::Selected);
        vtable.OnActivate = [option]() { agui::press(option, agui::MouseButton::Left, false, false); };
        std::string key = "option/" + std::to_string(i);
        builder.AddItem(graph::ControlId::Referenced(option, key), std::move(vtable));
        if (agui::dropDownSelected(dropDown) == index)
            builder.SetStart(graph::ControlId::Structural(key));
    }
}

void DropDownScreen::OnCursorMoved(const graph::GraphNode& node)
{
    if (auto* option = static_cast<const agui::Widget*>(node.Vtable.HostTag))
        agui::scrollIntoView(option);
}

} // namespace fa::screens
