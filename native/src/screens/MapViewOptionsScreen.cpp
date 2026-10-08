#include "MapViewOptionsScreen.hpp"

#include <string>

#include "AguiNodes.hpp"
#include "parts.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;

// The map view options, while they are on screen over a loaded game with no menu in front of them.
const Widget* FindOptions()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return nullptr;
    return agui::mapViewOptions();
}

} // namespace

std::string MapViewOptionsScreen::Name() const { return vocab::kMapViewOptions; }

bool MapViewOptionsScreen::IsActive()
{
    if (parts::current() != parts::Part::MapViewOptions)
        return false;
    _options = FindOptions();
    // Nothing to go back to once the game is gone.
    if (!agui::inGame())
        parts::close();
    return _options != nullptr;
}

void MapViewOptionsScreen::Build(graph::GraphBuilder& builder)
{
    if (!_options || FindOptions() != _options)
        return;
    builder.BeginStop("options");
    AddSubtree(builder, "options", _options);
}

void MapViewOptionsScreen::OnEscape() { parts::close(); }

void MapViewOptionsScreen::OnPop() { _options = nullptr; }

std::string MapViewOptionsScreen::LeaveLine() const
{
    // Left with Ctrl+Tab or Escape, not covered by a menu: the options are no longer in use.
    return parts::current() == parts::Part::None ? std::string(vocab::kMap) : std::string();
}

} // namespace fa::screens
