#include "GuiDump.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "log.h"

namespace fa::screens
{

namespace
{

using agui::Kind;
using agui::Widget;

constexpr int kMaxDepth = 40;
// Larger than any window class (MapGeneratorGui is 0x1e838 bytes): a widget this close after the
// window's address is taken to be one of its members.
constexpr std::uintptr_t kMaxWindowSize = 0x40000;
constexpr size_t kMaxStates = 32;

std::string FileNameOf(std::string_view className)
{
    std::string name(className);
    for (char& c : name)
        if (std::string_view("<>:\"/\\|?*").find(c) != std::string_view::npos)
            c = '_';
    return name + ".txt";
}

const char* CheckWord(agui::CheckState state)
{
    switch (state)
    {
    case agui::CheckState::Checked:
        return "checked";
    case agui::CheckState::Intermediate:
        return "intermediate";
    default:
        return "unchecked";
    }
}

std::string StateOf(const Widget* widget)
{
    switch (agui::kind(widget))
    {
    case Kind::Button:
        return agui::buttonToggled(widget) ? " toggled" : "";
    case Kind::CheckBox:
    case Kind::RadioButton:
        return std::format(" {}", CheckWord(agui::checkState(widget)));
    case Kind::DropDown:
        return std::format(" selected={}", agui::dropDownSelected(widget));
    case Kind::Slider:
    {
        agui::SliderValue slider = agui::sliderValue(widget);
        return std::format(
            " value={:g} min={:g} max={:g} step={:g}", slider.value, slider.min, slider.max, slider.step);
    }
    case Kind::Switch:
        switch (agui::switchState(widget))
        {
        case agui::SwitchState::Left:
            return " left";
        case agui::SwitchState::Right:
            return " right";
        default:
            return " none";
        }
    case Kind::TextBox:
        return std::format("{} content=\"{}\"", agui::readOnly(widget) ? " readonly" : "", agui::textBoxText(widget));
    case Kind::Tab:
        return agui::tabSelected(widget) ? " selected" : "";
    case Kind::Table:
        return std::format(" columns={}", agui::tableColumns(widget));
    case Kind::ListBox:
        return std::format(" items={}", agui::listBoxItems(widget).size());
    default:
        return {};
    }
}

// One state of a window: the full text, and its structure alone (classes, offsets, visibility),
// which tells states apart while values and texts change.
struct State
{
    std::string text;
    std::string structure;
};

void DumpWidget(State& state, const Widget* window, const Widget* widget, int depth)
{
    std::string line(depth * 2, ' ');
    line += agui::className(widget);
    auto distance = reinterpret_cast<std::uintptr_t>(widget) - reinterpret_cast<std::uintptr_t>(window);
    if (widget != window && distance < kMaxWindowSize)
        line += std::format(" @+{:#x}", distance);
    if (!agui::visible(widget))
        line += " hidden";
    state.structure += line;
    state.structure += '\n';
    if (!agui::enabled(widget))
        line += " disabled";
    if (std::string_view text = agui::text(widget); !text.empty())
        line += std::format(" \"{}\"", text);
    agui::ToolTip tip = agui::toolTip(widget);
    if (!tip.title.empty() || !tip.text.empty())
        line += std::format(" tooltip=\"{}\"/\"{}\"", tip.title, tip.text);
    if (std::string_view icon = agui::iconName(widget); !icon.empty())
        line += std::format(" icon=\"{}\"", icon);
    line += StateOf(widget);
    state.text += line;
    state.text += '\n';

    if (depth >= kMaxDepth)
        return;
    for (const Widget* child : agui::children(widget))
        DumpWidget(state, window, child, depth + 1);
    // Private children (a frame's content, a dropdown's list) follow, marked as such.
    auto hidden = agui::privateChildren(widget);
    if (!hidden.empty())
        state.text += std::string(depth * 2 + 2, ' ') + "-- private --\n";
    for (const Widget* child : hidden)
        DumpWidget(state, window, child, depth + 1);
}

// The states seen this run, per window class, in the order first seen.
std::map<std::string, std::vector<State>, std::less<>> g_states;

void Write(std::string_view className, const std::vector<State>& states)
{
    std::filesystem::path directory = log::directory() / "gui-dumps";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    std::filesystem::path file = directory / FileNameOf(className);
    std::ofstream out(file, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out)
    {
        log::warn("Cannot write {}", file.string());
        return;
    }
    for (size_t i = 0; i < states.size(); ++i)
        out << std::format("==== {} state {} of {} ====\n", className, i + 1, states.size()) << states[i].text << '\n';
}

} // namespace

void DumpWindow(const Widget* window, std::string_view className)
{
    State state;
    DumpWidget(state, window, window, 0);

    auto it = g_states.find(className);
    if (it == g_states.end())
        it = g_states.emplace(std::string(className), std::vector<State>{}).first;
    std::vector<State>& states = it->second;
    auto same = std::ranges::find(states, state.structure, &State::structure);
    if (same == states.end())
    {
        if (states.size() >= kMaxStates)
            return;
        states.push_back(std::move(state));
        log::info("Window {} state {} dumped to gui-dumps\\{}", className, states.size(), FileNameOf(className));
    }
    else if (same->text != state.text)
        same->text = std::move(state.text);
    else
        return;
    Write(className, states);
}

std::string DescribeTree(const Widget* window)
{
    State state;
    DumpWidget(state, window, window, 0);
    return std::move(state.text);
}

} // namespace fa::screens
