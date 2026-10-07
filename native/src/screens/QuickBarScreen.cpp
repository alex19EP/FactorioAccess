#include "QuickBarScreen.hpp"

#include <algorithm>
#include <format>
#include <functional>
#include <string>
#include <vector>

#include "AguiNodes.hpp"
#include "parts.h"
#include "speech.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;

std::string PageName(uint8_t page) { return std::format("{} {}", vocab::kPage, page + 1); }

// "bar 2, page 4": which bar, the first the number keys use, then the page it shows. Bars may show
// the same page, so the bar comes first and tells them apart.
std::string BarName(std::size_t bar, uint8_t page)
{
    return std::format("{} {}, {}", vocab::kBar, bar + 1, PageName(page));
}

// "transport belt 50", or "transport belt 0" for an item the player carries none of; "empty" with
// nothing on the slot.
std::string SlotContent(const Widget* slot)
{
    agui::SlotButton button = agui::slotButton(slot);
    if (button.name.empty())
        return std::string(vocab::kEmpty);
    std::string name =
        button.quality.empty() ? std::string(button.name) : std::format("{} {}", button.quality, button.name);
    return std::format("{} {:.0f}", name, button.count);
}

// A quickbar slot, clicked as any slot, read with its count even when that is 0.
graph::NodeVtable QuickBarSlotNode(const Widget* slot, std::function<std::string()> name = {})
{
    graph::NodeVtable vtable = ControlNode(slot, std::move(name));
    // The content is the slot node's first live part.
    for (graph::NodeAnnouncement& announcement : vtable.Announcements)
        if (announcement.Live)
        {
            announcement.Text = [slot]() { return SlotContent(slot); };
            break;
        }
    return vtable;
}

std::string SlotKey(const std::string& prefix, std::size_t row, std::size_t slot)
{
    return std::format("{}/{}/{}", prefix, row, slot);
}

// The quickbar, while it is on screen over a loaded game with no menu in front of it.
const Widget* FindBar()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return nullptr;
    const Widget* bar = agui::quickBar();
    return bar && agui::visible(bar) ? bar : nullptr;
}

void AddBars(graph::GraphBuilder& builder, const std::vector<agui::QuickBarRow>& rows)
{
    builder.BeginStop("bars");
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        builder.PushContext(BarName(i, rows[i].page));
        builder.StartRow("bar");
        for (std::size_t j = 0; j < rows[i].slots.size(); ++j)
            builder.AddItem(graph::ControlId::Referenced(rows[i].slots[j], SlotKey("bars", i, j)),
                QuickBarSlotNode(rows[i].slots[j]));
        builder.EndRow();
        builder.PopContext();
    }

    builder.BeginStop("pages");
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        uint8_t page = rows[i].page;
        builder.AddItem(graph::ControlId::Referenced(rows[i].button, std::format("pages/{}", i)),
            ControlNode(rows[i].button, [i, page]() { return BarName(i, page); }));
    }
}

// A row per page in its own context, so that Up and Down say the page: its button, which shows the
// page on the bar the picker opened for, then its slots numbered as the quickbar keys take them.
void AddPicker(graph::GraphBuilder& builder, std::vector<agui::QuickBarRow> pages, std::size_t bar)
{
    // The game stacks page 10 on top; page 1 comes first here, as the keys count them.
    std::ranges::sort(pages, {}, &agui::QuickBarRow::page);
    std::string show = std::format("{} {}", vocab::kShowOnBar, bar + 1);
    builder.BeginStop("picker");
    for (const agui::QuickBarRow& row : pages)
    {
        // The slots carry their numbers and the context its page, so the row is a line, which leaves
        // its items unpositioned; lines keep the column on Up and Down all the same.
        builder.PushContext(PageName(row.page));
        builder.StartLine("page");
        builder.AddItem(graph::ControlId::Referenced(row.button, std::format("picker/{}", row.page)),
            ControlNode(row.button, [show]() { return show; }));
        for (std::size_t j = 0; j < row.slots.size(); ++j)
            builder.AddItem(graph::ControlId::Referenced(row.slots[j], SlotKey("picker", row.page, j)),
                QuickBarSlotNode(row.slots[j], [j]() { return std::to_string(j + 1); }));
        builder.EndRow();
        builder.PopContext();
    }
}

} // namespace

const char* QuickBarScreen::Name() const { return vocab::kQuickBar.data(); }

bool QuickBarScreen::IsActive()
{
    if (parts::current() != parts::Part::QuickBar)
        return false;
    _bar = FindBar();
    // Nothing to go back to once the game is gone.
    if (!agui::inGame())
        parts::close();
    return _bar != nullptr;
}

void QuickBarScreen::Build(graph::GraphBuilder& builder)
{
    if (!_bar || FindBar() != _bar)
        return;
    std::vector<agui::QuickBarRow> rows = agui::quickBarRows(_bar);
    int picking = agui::quickBarPickingFor(_bar);
    if (picking >= 0 && static_cast<std::size_t>(picking) < rows.size())
    {
        // Opening the picker lands on the page the bar shows.
        if (_picking != picking)
            _landing = std::format("picker/{}", rows[picking].page);
        AddPicker(builder, agui::quickBarPickerRows(_bar), static_cast<std::size_t>(picking));
    }
    else
    {
        // Choosing a page lands on the bar it now shows; Escape back on the bar's page button.
        if (_picking >= 0 && static_cast<std::size_t>(_picking) < rows.size())
            _landing = _cancelled ? std::format("pages/{}", _picking) : SlotKey("bars", _picking, 0);
        _cancelled = false;
        picking = -1;
        AddBars(builder, rows);
    }
    _picking = picking;
}

const char* QuickBarScreen::TakeSuggestedLanding()
{
    if (_landing.empty())
        return nullptr;
    // Copied by the render straight after this call.
    _taken = std::move(_landing);
    _landing.clear();
    return _taken.c_str();
}

void QuickBarScreen::OnEscape()
{
    // The page button toggles its picker, so pressing it again closes the picker unchanged.
    if (_bar && _picking >= 0)
    {
        std::vector<agui::QuickBarRow> rows = agui::quickBarRows(_bar);
        if (static_cast<std::size_t>(_picking) < rows.size())
        {
            _cancelled = true;
            agui::press(rows[_picking].button, agui::MouseButton::Left, false, false);
            return;
        }
    }
    parts::close();
}

void QuickBarScreen::OnPop()
{
    _bar = nullptr;
    _picking = -1;
    _cancelled = false;
    _landing.clear();
}

std::string QuickBarScreen::LeaveLine() const
{
    // Left with Ctrl+Tab or Escape, not covered by a menu: the quickbar part is no longer in use.
    return parts::current() == parts::Part::None ? std::string(vocab::kMap) : std::string();
}

void QuickBarScreen::WatchPage()
{
    static const Widget* lastBar = nullptr;
    static int lastPage = -1;
    const Widget* bar = FindBar();
    std::vector<agui::QuickBarRow> rows;
    if (bar)
        rows = agui::quickBarRows(bar);
    int page = rows.empty() ? -1 : rows[0].page;
    bool changed = bar == lastBar && page != lastPage && page >= 0 && lastPage >= 0;
    lastBar = bar;
    lastPage = page;
    // In the quickbar, the cursor moving to the new page says it.
    if (!changed || parts::current() == parts::Part::QuickBar)
        return;
    std::string line = PageName(rows[0].page);
    if (!rows[0].slots.empty())
        line += ", " + SlotContent(rows[0].slots[0]);
    speech::say(line, true);
}

} // namespace fa::screens
