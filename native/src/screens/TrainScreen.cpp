#include "TrainScreen.hpp"

#include "ScheduleNodes.hpp"
#include "game.h"
#include "navigator/ScreenManager.hpp"
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

// A wait condition's row as the cursor reads it, without its drag handle and remove button.
std::string ConditionText(const Widget* condition)
{
    return RowText(condition, {agui::scheduleConditionDrag(condition), agui::scheduleConditionRemove(condition)});
}

// The window's own labels outside its schedule and tabs, as shown: its title, the train's group and
// its status ("Waiting at stop").
std::vector<std::pair<const Widget*, std::string>> TrainLabels(const Widget* window)
{
    agui::TrainWindowParts parts = agui::trainWindowParts(window);
    std::vector<std::pair<const Widget*, std::string>> labels;
    for (const Widget* label : FindAll(window, "agui::Label"))
    {
        bool inPart = false;
        for (const Widget* up = label; up && !inPart; up = agui::parent(up))
            inPart = up == parts.schedule || up == parts.tabs;
        if (!inPart && Shows(label))
            labels.emplace_back(label, LabelText(label));
    }
    return labels;
}

// Where `now` first differs from `before`, a list one longer: the row added.
std::size_t AddedAt(const std::vector<std::string>& before, const std::vector<std::string>& now)
{
    std::size_t added = 0;
    while (added < before.size() && now[added] == before[added])
        ++added;
    return added;
}

} // namespace

bool TrainScreen::IsActive()
{
    // Watched on the map beside the window as well.
    if (const Widget* window = FindWindow().window)
    {
        WatchSchedule(window);
        WatchStatus(window);
    }
    else
    {
        _seen.reset();
        _labels.clear();
    }
    return EntityWindowScreen::IsActive();
}

void TrainScreen::WatchSchedule(const Widget* window)
{
    // A new row is found by name: the go-to marker moves to a stop added from the map when the train
    // heads there at once.
    std::vector<agui::ScheduleStation> stations = agui::scheduleParts(agui::trainWindowParts(window).schedule).stations;
    ScheduleSeen now;
    for (const agui::ScheduleStation& station : stations)
    {
        now.stations.push_back(LabelText(station.label));
        std::vector<std::string>& conditions = now.conditions.emplace_back();
        for (const Widget* condition : station.conditions)
            conditions.push_back(ConditionText(condition));
    }
    if (_seen && now.stations.size() == _seen->stations.size() + 1)
    {
        std::size_t added = AddedAt(_seen->stations, now.stations);
        std::string line = now.stations[added];
        if (agui::buttonToggled(stations[added].goTo))
            line += ", " + std::string(vocab::kCurrentStop);
        speech::say(line);
    }
    else if (_seen && now.stations == _seen->stations)
    {
        for (std::size_t i = 0; i < now.conditions.size(); ++i)
            if (now.conditions[i].size() == _seen->conditions[i].size() + 1)
                speech::say(now.conditions[i][AddedAt(_seen->conditions[i], now.conditions[i])]);
    }
    _seen = std::move(now);
}

void TrainScreen::WatchStatus(const Widget* window)
{
    std::vector<std::pair<const Widget*, std::string>> labels = TrainLabels(window);
    // The cursor on the label hears it change already.
    const void* focused = nav::ScreenManager::Get().FocusedHostTag();
    if (labels.size() == _labels.size())
        for (std::size_t i = 0; i < labels.size(); ++i)
            if (labels[i].second != _labels[i] && !labels[i].second.empty() && labels[i].first != focused)
                speech::say(labels[i].second);
    _labels.clear();
    for (auto& [label, text] : labels)
        _labels.push_back(std::move(text));
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
