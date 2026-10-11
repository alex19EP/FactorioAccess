#include "TooltipScreen.hpp"

#include <string>
#include <utility>

#include "navigator/ScreenManager.hpp"
#include "speech.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

// As many lines as are said at once; a longer tooltip opens to be read a line at a time.
constexpr std::size_t kSpokenLines = 3;

// The open tooltip, and the screen it came from. Main thread only.
struct Reader
{
    std::vector<TooltipLine> lines;
    const nav::Screen* owner = nullptr;
};
Reader g_reader;

void Close() { g_reader = {}; }

} // namespace

void ReadTooltip(std::vector<TooltipLine> lines)
{
    if (lines.empty())
    {
        speech::say(std::string(vocab::kNoTooltip), true);
        return;
    }
    if (lines.size() <= kSpokenLines)
    {
        std::string text;
        for (const TooltipLine& line : lines)
            text += (text.empty() ? "" : "\n") + line.text;
        speech::say(text, true);
        return;
    }
    g_reader = {std::move(lines), nav::ScreenManager::Get().Focused()};
}

bool TooltipScreen::IsActive()
{
    if (g_reader.lines.empty())
        return false;
    if (g_reader.owner && !nav::ScreenManager::Get().IsLive(g_reader.owner))
    {
        Close();
        return false;
    }
    return true;
}

void TooltipScreen::Build(graph::GraphBuilder& builder)
{
    builder.BeginStop("lines");
    int section = 0;
    builder.SetRegion("section/0");
    for (std::size_t i = 0; i < g_reader.lines.size(); ++i)
    {
        const TooltipLine& line = g_reader.lines[i];
        if (line.section)
            builder.SetRegion("section/" + std::to_string(++section));
        graph::NodeVtable vtable;
        vtable.Announcements.emplace_back(
            [text = line.text]() { return text; }, false, graph::AnnouncementKinds::Label);
        // The tooltip key again puts it away.
        vtable.OnTooltip = []() { Close(); };
        builder.AddItem(graph::ControlId::Structural("line/" + std::to_string(i)), std::move(vtable));
    }
    builder.SetRegion("");
}

void TooltipScreen::OnEscape() { Close(); }

void TooltipScreen::OnPop() { Close(); }

} // namespace fa::screens
