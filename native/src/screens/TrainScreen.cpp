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

// Each station row of the window's schedule as it reads: its name, and whether the train heads there.
std::vector<std::string> StationLines(const Widget* window)
{
    std::vector<std::string> lines;
    const Widget* schedule = agui::trainWindowParts(window).schedule;
    for (const agui::ScheduleStation& station : agui::scheduleParts(schedule).stations)
    {
        std::string line = LabelText(station.label);
        if (agui::buttonToggled(station.goTo))
            line += ", " + std::string(vocab::kCurrentStop);
        lines.push_back(std::move(line));
    }
    return lines;
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
    // A stop added from the map (Ctrl+click, Shift+click) shows as a new row in the window beside it.
    std::vector<std::string> lines = StationLines(window);
    if (_mapStations && lines.size() > _mapStations->size())
    {
        std::size_t added = 0;
        while (added < _mapStations->size() && lines[added] == (*_mapStations)[added])
            ++added;
        speech::say(lines[added], false);
    }
    _mapStations = std::move(lines);
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
