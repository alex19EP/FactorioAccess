#include "TrainScreen.hpp"

#include "ScheduleNodes.hpp"
#include "game.h"
#include "parts.h"
#include "speech.h"
#include "vocab.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fa::screens
{

using agui::Widget;

namespace
{

// The names of the window's schedule's stations, in order.
std::vector<std::string> StationNames(const std::vector<agui::ScheduleStation>& stations)
{
    std::vector<std::string> names;
    for (const agui::ScheduleStation& station : stations)
        names.push_back(LabelText(station.label));
    return names;
}

} // namespace

bool TrainScreen::IsActive()
{
    const Widget* window = FindWindow().window;
    parts::setAvailable(parts::Part::Map, window != nullptr);
    if (parts::current() == parts::Part::Map)
    {
        if (window)
        {
            WatchSchedule(window);
            return false;
        }
        parts::close();
    }
    _mapStations.reset();
    return EntityWindowScreen::IsActive();
}

void TrainScreen::WatchSchedule(const Widget* window)
{
    // A stop added from the map (Ctrl+click, Shift+click) shows as a new row in the window beside it,
    // found by name: the go-to marker moves to it when the train heads there at once.
    std::vector<agui::ScheduleStation> stations = agui::scheduleParts(agui::trainWindowParts(window).schedule).stations;
    std::vector<std::string> names = StationNames(stations);
    if (_mapStations && names.size() > _mapStations->size())
    {
        std::size_t added = 0;
        while (added < _mapStations->size() && names[added] == (*_mapStations)[added])
            ++added;
        std::string line = names[added];
        if (agui::buttonToggled(stations[added].goTo))
            line += ", " + std::string(vocab::kCurrentStop);
        speech::say(line);
    }
    _mapStations = std::move(names);
}

std::string TrainScreen::LeaveLine() const
{
    return parts::current() == parts::Part::Map ? std::string(vocab::kMap) : std::string();
}

bool TrainScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "TrainGui"); }

void TrainScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    agui::TrainWindowParts parts = agui::trainWindowParts(window);
    AddTitledWindow(builder, "train", window, {parts.tabs, parts.schedule});
    const bool tabs = Shows(parts.tabs);
    if (tabs)
        if (const Widget* header = FindDescendant(parts.tabs, "agui::Table"))
            AddSubtree(builder, "train/tabs", header);

    // The schedule keeps its visible flag while the other tab shows: the selected tab tells.
    if (Shows(parts.schedule) && (!tabs || agui::tabSelected(parts.scheduleTab)))
    {
        AddSchedule(builder, "schedule", parts.schedule,
            [this](std::string key, std::function<bool()> ready) { LandWhen(std::move(key), std::move(ready)); });
        return;
    }
    if (tabs)
    {
        builder.BeginStop("cargo");
        AddSubtree(builder, "cargo", agui::member(parts.tabs, game::layout.tabbedPaneContent));
    }
}

} // namespace fa::screens
