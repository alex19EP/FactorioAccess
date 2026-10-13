#include "ProductionScreen.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "TooltipScreen.hpp"
#include "game.h"
#include "text.h"
#include "vocab.h"

namespace fa::screens
{

using agui::Widget;
using graph::AnnouncementKinds::Label;
using graph::AnnouncementKinds::Value;

namespace
{

constexpr const char* kSearchKey = "controls/search/field";

// A row of a column's table (a TableItem): the entry's slot, with the total drawn on it, and the
// label of its rate.
struct Row
{
    const Widget* slot = nullptr;
    const Widget* rate = nullptr;
};

// The rows in the game's order, most first. The table is two wide and the game fills it down its
// first column, then its second, so its children (row by row) alternate between the two halves.
std::vector<Row> Rows(const Widget* table)
{
    std::vector<const Widget*> cells;
    for (const Widget* item : agui::children(table))
        if (agui::visible(item))
            cells.push_back(item);
    const std::size_t columns = std::max(1u, agui::tableColumns(table));
    const std::size_t lines = (cells.size() + columns - 1) / columns;
    std::vector<Row> rows;
    for (std::size_t i = 0; i < lines * columns; ++i)
    {
        std::size_t at = (i % lines) * columns + i / lines;
        if (at >= cells.size())
            continue;
        const Widget* item = cells[at];
        // Both in the row frame's content flow; its title flow is hidden.
        Row row{FindDescendant(item, "SlotButtonBase"), FindDescendant(item, "agui::Label")};
        if (row.slot && row.rate)
            rows.push_back(row);
    }
    return rows;
}

std::string EntryName(const Widget* slot)
{
    agui::SlotButton button = agui::slotButton(slot);
    return button.quality.empty() ? std::string(button.name) : std::format("{} {}", button.quality, button.name);
}

graph::NodeVtable RowNode(const Row& row, int index, int count)
{
    graph::NodeVtable vtable;
    vtable.HostTag = row.slot;
    const Widget* slot = row.slot;
    const Widget* rate = row.rate;
    vtable.Announcements.emplace_back([slot]() { return EntryName(slot); }, false, Label);
    // Both change every tick; read when the row is reached, not watched.
    vtable.Announcements.emplace_back([rate]() { return LabelText(rate); }, false, Value);
    vtable.Announcements.emplace_back([slot]() { return agui::slotNumberText(slot); }, false, Value);
    vtable.Announcements.emplace_back(
        [index, count]() { return vocab::position(index, count); }, false, graph::AnnouncementKinds::Position);
    vtable.SpeaksOwnPosition = true;
    vtable.OnTooltip = [slot, rate]()
    {
        // The rate's tooltip is the exact rate ("12,345/m"); the slot's, the entry's own.
        std::vector<TooltipLine> lines;
        agui::ToolTip exact = agui::toolTip(rate);
        if (std::string line = text::speakable(exact.text.empty() ? exact.title : exact.text); !line.empty())
            lines.push_back({std::move(line)});
        bool created = false;
        if (const Widget* tooltip = agui::showTooltip(slot, created))
            for (TooltipLine& line : TooltipLines(tooltip))
                lines.push_back(std::move(line));
        if (created)
            agui::removeTooltip(slot);
        ReadTooltip(std::move(lines));
    };
    vtable.SearchText = [slot]() { return EntryName(slot); };
    return vtable;
}

// A row of the window's own controls (its tabs, its time span buttons), left to right.
void AddControlRow(graph::GraphBuilder& builder, const std::string& prefix, const std::vector<const Widget*>& controls)
{
    if (controls.empty())
        return;
    builder.StartRow(prefix);
    for (std::size_t i = 0; i < controls.size(); ++i)
        builder.AddItem(
            graph::ControlId::Referenced(controls[i], std::format("{}/{}", prefix, i)), ControlNode(controls[i]));
    builder.EndRow();
}

// A column's nodes in the order the arrows walk them: the controls at its top, then its rows.
// Declared rows first, so that a stop entered afresh starts on the left column's first row.
struct Column
{
    std::vector<graph::ControlId> controls;
    std::vector<graph::ControlId> rows;
    // Each row's entry, the end of its key, and back to the row.
    std::vector<std::string> entries;
    std::unordered_map<std::string, std::size_t> rowOf;

    std::vector<graph::ControlId> Walk() const
    {
        std::vector<graph::ControlId> walk = controls;
        walk.insert(walk.end(), rows.begin(), rows.end());
        return walk;
    }
};

Column AddColumn(graph::GraphBuilder& builder, int index, const agui::ProductionColumn& parts)
{
    Column column;
    if (!parts.table)
        return column;
    const Widget* caption = FindDescendant(parts.subheader, "agui::Label");
    std::string name = caption ? LabelText(caption) : std::string();
    if (!name.empty())
        builder.PushContext(name);

    std::vector<Row> rows = Rows(parts.table);
    // An entry's key follows it as the rates reorder the rows; the same name twice keeps both.
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        std::string entry = EntryName(rows[i].slot);
        while (!column.rowOf.try_emplace(entry, i).second)
            entry += '+';
        graph::ControlId id = graph::ControlId::Structural(std::format("lists/{}/{}", index, entry));
        builder.AddNode(id, RowNode(rows[i], static_cast<int>(i + 1), static_cast<int>(rows.size())));
        column.rows.push_back(std::move(id));
        column.entries.push_back(std::move(entry));
    }
    for (const auto& [control, key] :
        {std::pair{parts.global, "global"}, std::pair{parts.quality, "quality"}, std::pair{parts.reset, "reset"}})
    {
        if (!Shows(control))
            continue;
        graph::ControlId id = graph::ControlId::Referenced(control, std::format("lists/{}/{}", index, key));
        builder.AddNode(id, ControlNode(control));
        column.controls.push_back(std::move(id));
    }

    if (!name.empty())
        builder.PopContext();
    return column;
}

// Up and Down follow a column. Left and Right go from a row to the same entry in the other column,
// as each column has its own order; to the same place, said as not listed, when the entry is not
// there, and from a control to the same place.
void WireColumns(graph::GraphBuilder& builder, const Column& left, const Column& right)
{
    const Column* columns[2] = {&left, &right};
    std::vector<graph::ControlId> walks[2] = {left.Walk(), right.Walk()};
    const std::string notListed(vocab::kNotListed);
    for (int side = 0; side < 2; ++side)
    {
        const Column& column = *columns[side];
        const Column& otherColumn = *columns[1 - side];
        const std::vector<graph::ControlId>& walk = walks[side];
        const std::vector<graph::ControlId>& other = walks[1 - side];
        const graph::GraphDir across = side == 0 ? graph::GraphDir::Right : graph::GraphDir::Left;
        for (std::size_t i = 0; i < walk.size(); ++i)
        {
            if (i > 0)
                builder.Connect(walk[i], graph::GraphDir::Up, walk[i - 1]);
            if (i + 1 < walk.size())
                builder.Connect(walk[i], graph::GraphDir::Down, walk[i + 1]);
            if (other.empty())
                continue;
            const graph::ControlId& place = other[std::min(i, other.size() - 1)];
            if (i < column.controls.size())
            {
                builder.Connect(walk[i], across, place);
                continue;
            }
            auto same = otherColumn.rowOf.find(column.entries[i - column.controls.size()]);
            if (same != otherColumn.rowOf.end())
                builder.Connect(walk[i], across, otherColumn.rows[same->second]);
            else
                builder.Connect(walk[i], across, place, notListed);
        }
    }
}

} // namespace

bool ProductionScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "ProductionGui"); }

void ProductionScreen::OnPop()
{
    EntityWindowScreen::OnPop();
    _searching = false;
}

void ProductionScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    agui::ProductionParts parts = agui::productionParts(window);
    const Widget* title = agui::frameTitle(window);
    std::string titleText = title && Shows(title) ? LabelText(title) : std::string();
    if (!titleText.empty())
        builder.PushContext(titleText);

    builder.BeginStop("controls");
    AddControlRow(builder, "controls/tab", FindAll(parts.tabs, "agui::Tab"));
    AddControlRow(builder, "controls/span", FindAll(parts.precisions, "agui::Button"));
    // The search field opens in the title bar, beside its button, which is a SearchBar.
    const Widget* header = agui::member(window, game::layout.frameHeader);
    const Widget* field = FindDescendant(window, "agui::TextField");
    std::vector<const Widget*> search = FindAll(header, "SearchBar");
    if (!search.empty() || field)
    {
        builder.StartRow("controls/search");
        for (const Widget* button : search)
            builder.AddItem(graph::ControlId::Referenced(button, "controls/search/button"), ControlNode(button));
        if (field)
            builder.AddItem(graph::ControlId::Referenced(field, kSearchKey), ControlNode(field));
        builder.EndRow();
    }
    // Opening the search lands on its field.
    if (field && !_searching)
        LandWhen(kSearchKey, []() { return true; });
    _searching = field != nullptr;

    builder.BeginStop("lists");
    Column left = AddColumn(builder, 0, parts.columns[0]);
    Column right = AddColumn(builder, 1, parts.columns[1]);
    WireColumns(builder, left, right);
    if (!left.rows.empty())
        builder.SetStart(left.rows.front());
    else if (!right.rows.empty())
        builder.SetStart(right.rows.front());

    if (!titleText.empty())
        builder.PopContext();
}

} // namespace fa::screens
