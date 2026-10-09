#include "ModWindowsScreen.hpp"

#include <algorithm>
#include <format>
#include <ranges>
#include <string>

#include "AguiNodes.hpp"
#include "parts.h"
#include "speech.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;

// The windows, while they are on screen over a loaded game with no menu in front of them.
agui::ModScreenWindows FindWindows()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return {};
    return agui::modScreenWindows();
}

// The windows in reading order: the open one first, then the others topmost first, the one a
// sighted player sees in front.
std::vector<const Widget*> ReadingOrder(const agui::ModScreenWindows& found)
{
    std::vector<const Widget*> order;
    if (found.opened)
        order.push_back(found.opened);
    for (const Widget* window : found.windows | std::views::reverse)
        if (window != found.opened)
            order.push_back(window);
    return order;
}

std::string TitleOf(const Widget* window)
{
    const Widget* title = agui::frameTitle(window);
    return title && Shows(title) ? LabelText(title) : std::string();
}

} // namespace

std::string ModWindowsScreen::Name() const
{
    // An open window introduces itself by its title, as the game's own windows do.
    return _opened ? std::string() : std::string(vocab::kModWindows);
}

bool ModWindowsScreen::IsActive()
{
    // A menu hides the windows without closing them: nothing is announced again when it closes.
    if (agui::inGame() && agui::menuStateWindow())
        return false;
    agui::ModScreenWindows found = FindWindows();
    AnnounceNew(found);
    // While a mod has a window open, it is what is open and the part would only lead back to it.
    parts::setAvailable(parts::Part::ModWindows, !found.windows.empty() && !found.opened);
    bool part = parts::current() == parts::Part::ModWindows;
    // The last window closed, the game is gone, or a mod opened one: back to what is open.
    if (part && (found.windows.empty() || found.opened))
        parts::close();
    if (found.opened != _opened)
    {
        // Opened or closed: one inactive frame pops this screen, so it lands afresh on what is open.
        _opened = found.opened;
        _labels.clear();
        return false;
    }
    if (!_opened && !(part && !found.windows.empty()))
    {
        _labels.clear();
        return false;
    }
    SayChangedLabels(found.windows);
    return true;
}

void ModWindowsScreen::Build(graph::GraphBuilder& builder)
{
    std::vector<const Widget*> windows = ReadingOrder(FindWindows());
    for (std::size_t i = 0; i < windows.size(); ++i)
    {
        const Widget* window = windows[i];
        if (!HasContent(window))
            continue;
        std::string key = std::format("window/{}", i);
        builder.BeginStop(key);
        std::string title = TitleOf(window);
        if (!title.empty())
            builder.PushContext(title);
        AddSubtree(builder, key, window, {agui::frameTitle(window)});
        if (!title.empty())
            builder.PopContext();
    }
}

bool ModWindowsScreen::TypingIn(const graph::GraphNode& node) { return TypingInField(node); }

void ModWindowsScreen::OnCursorMoved(const graph::GraphNode& node) { FollowCursor(node); }

void ModWindowsScreen::OnEscape() { parts::close(); }

void ModWindowsScreen::OnPop() { _labels.clear(); }

std::string ModWindowsScreen::LeaveLine() const
{
    // Left with Ctrl+Tab or Escape, or the open window closed, not covered by a menu: back on the
    // map. Not while another window of the mod's opens in its place.
    return parts::current() == parts::Part::None && !_opened ? std::string(vocab::kMap) : std::string();
}

void ModWindowsScreen::AnnounceNew(const agui::ModScreenWindows& found)
{
    const std::vector<const Widget*>& windows = found.windows;
    if (parts::current() != parts::Part::ModWindows)
        for (const Widget* window : windows)
        {
            // An open window is landed on instead.
            if (window == found.opened || std::ranges::find(_seen, window) != _seen.end() || !HasContent(window))
                continue;
            std::string title = TitleOf(window);
            speech::say(title.empty() ? std::string(vocab::kUntitledModWindowShown)
                                      : std::string(vocab::kModWindowShown(title)),
                false);
        }
    _seen = windows;
}

void ModWindowsScreen::SayChangedLabels(const std::vector<const Widget*>& windows)
{
    std::unordered_map<const Widget*, std::string> labels;
    for (const Widget* window : windows)
        for (const Widget* label : FindAll(window, "agui::Label"))
            labels.emplace(label, LabelText(label));
    for (const auto& [label, text] : labels)
    {
        auto before = _labels.find(label);
        if (before != _labels.end() && before->second != text && !text.empty())
            speech::say(text, false);
    }
    _labels = std::move(labels);
}

} // namespace fa::screens
