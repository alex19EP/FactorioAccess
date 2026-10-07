#include "SelectedInfoScreen.hpp"

#include <string>

#include "AguiNodes.hpp"
#include "agui.h"
#include "parts.h"
#include "selectedinfo.h"
#include "speech.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

// Only over the map itself: any window or part of the HUD the player moves to takes over.
bool OverMap()
{
    return agui::inGame() && !agui::menuStateWindow() && !agui::gameWindowOpen() &&
           parts::current() == parts::Part::None && !agui::factoriopedia().window &&
           !agui::technologyWindow().window;
}

} // namespace

bool SelectedInfoScreen::IsActive()
{
    const bool overMap = OverMap();
    if (selectedinfo::takeRequest() && overMap && !selectedinfo::open())
        speech::say(std::string(vocab::kNothingHere), true);
    if (!selectedinfo::window())
        return false;
    if (!overMap)
    {
        selectedinfo::close();
        return false;
    }
    return selectedinfo::check();
}

void SelectedInfoScreen::Build(graph::GraphBuilder& builder)
{
    if (const agui::Widget* panel = selectedinfo::window())
        AddSubtree(builder, "panel", panel);
}

void SelectedInfoScreen::OnEscape()
{
    _escaped = true;
    selectedinfo::close();
}

void SelectedInfoScreen::OnPop()
{
    selectedinfo::close();
}

std::string SelectedInfoScreen::LeaveLine() const
{
    // Escape goes back to the map; anything else that closed it speaks for itself.
    return _escaped ? std::string(vocab::kMap) : std::string();
}

} // namespace fa::screens
