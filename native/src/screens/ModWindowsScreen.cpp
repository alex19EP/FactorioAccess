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
std::vector<const Widget*> FindWindows()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return {};
    return agui::modScreenWindows();
}

std::string TitleOf(const Widget* window)
{
    const Widget* title = agui::frameTitle(window);
    return title && Shows(title) ? LabelText(title) : std::string();
}

} // namespace

std::string ModWindowsScreen::Name() const { return vocab::kModWindows; }

bool ModWindowsScreen::IsActive()
{
    // A menu hides the windows without closing them: nothing is announced again when it closes.
    if (agui::inGame() && agui::menuStateWindow())
        return false;
    std::vector<const Widget*> windows = FindWindows();
    AnnounceNew(windows);
    parts::setAvailable(parts::Part::ModWindows, !windows.empty());
    if (parts::current() != parts::Part::ModWindows)
    {
        _labels.clear();
        return false;
    }
    // The last window closed, or the game is gone: back to what is open.
    if (windows.empty())
    {
        parts::close();
        _labels.clear();
        return false;
    }
    SayChangedLabels(windows);
    return true;
}

void ModWindowsScreen::Build(graph::GraphBuilder& builder)
{
    std::vector<const Widget*> windows = FindWindows();
    // The topmost first: the one a sighted player sees in front.
    for (std::size_t i = windows.size(); i-- > 0;)
    {
        const Widget* window = windows[i];
        if (!HasContent(window))
            continue;
        std::string key = std::format("window/{}", windows.size() - 1 - i);
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
    // Left with Ctrl+Tab or Escape, not covered by a menu: the part is no longer in use.
    return parts::current() == parts::Part::None ? std::string(vocab::kMap) : std::string();
}

void ModWindowsScreen::AnnounceNew(const std::vector<const Widget*>& windows)
{
    if (parts::current() != parts::Part::ModWindows)
        for (const Widget* window : windows)
        {
            if (std::ranges::find(_seen, window) != _seen.end() || !HasContent(window))
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
