#include "AlertsConfigScreen.hpp"

#include <algorithm>
#include <optional>
#include <span>
#include <string>

#include "game.h"
#include "text.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;
using game::layout;

// The table's columns, in the game's order (AlertsConfigGui's ColumnDefinitions).
enum Column : std::size_t
{
    Icon,
    Name,
    Muted,
    Disabled,
    Latest,
    Columns,
};

// A column heading's caption ("Muted"), as the game shows it.
std::string Heading(std::span<const Widget* const> cells, std::size_t column)
{
    const Widget* label = column < cells.size() ? FindDescendant(cells[column], "agui::Label") : nullptr;
    return label ? LabelText(label) : std::string();
}

// The order a column sorts the table in for the game's flag (Header::ascending), as the column's
// sortingOrder lambdas in the AlertsConfigGui constructor compare: with the flag off, names A to Z,
// unmuted and enabled alerts first, the oldest first; with it on, the alert types in the game's
// order (the table's default).
vocab::Word Order(std::size_t column, bool ascending)
{
    switch (column)
    {
    case Icon:
        return ascending ? vocab::kGameOrder : vocab::kReverseOrder;
    case Name:
        return ascending ? vocab::kSortedZToA : vocab::kSortedAToZ;
    case Muted:
        return ascending ? vocab::kMutedFirst : vocab::kUnmutedFirst;
    case Disabled:
        return ascending ? vocab::kDisabledFirst : vocab::kEnabledFirst;
    default:
        return ascending ? vocab::kNewestFirst : vocab::kOldestFirst;
    }
}

// One alert type's checkbox, named for the alert and its column ("Entity destroyed, Muted").
graph::NodeVtable CheckBoxNode(const Widget* checkbox, const Widget* name, std::string heading, bool muted)
{
    graph::NodeVtable node = ControlNode(checkbox,
        [name, heading]()
        {
            std::string alert = LabelText(name);
            return heading.empty() ? alert : alert + ", " + heading;
        });
    // The mute checkbox is greyed out for an alert that plays no sound: "disabled" would read as the
    // Disabled column.
    if (muted)
        for (graph::NodeAnnouncement& part : node.Announcements)
            if (part.Kind == graph::AnnouncementKinds::Enabled)
                part.Text = [checkbox]()
                { return agui::enabled(checkbox) ? std::string() : std::string(vocab::kNoSound); };
    return node;
}

void AddAlerts(graph::GraphBuilder& builder, const Widget* table)
{
    std::span<const Widget* const> cells = agui::children(table);
    if (agui::tableColumns(table) != Columns)
        return;
    std::string mutedHeading = Heading(cells, Muted);
    std::string disabledHeading = Heading(cells, Disabled);

    builder.BeginStop("alerts");
    // The header row first, then an AlertRecord's widgets per row.
    for (std::size_t row = 1; (row + 1) * Columns <= cells.size(); ++row)
    {
        const Widget* name = cells[row * Columns + Name];
        const Widget* muted = cells[row * Columns + Muted];
        const Widget* disabled = cells[row * Columns + Disabled];
        const Widget* latest = cells[row * Columns + Latest];
        if (!Shows(name))
            continue;
        // Keyed by the alert, not the row: sorting moves the rows, and the cursor stays on its alert.
        std::string key = "alerts/" + LabelText(name);
        builder.StartRow("alert");
        builder.AddItem(graph::ControlId::Referenced(name, key),
            TextNode(name,
                [name, latest]()
                {
                    std::string when = LabelText(latest);
                    return when.empty() ? LabelText(name) : LabelText(name) + ", " + when;
                }));
        if (Shows(muted))
            builder.AddItem(
                graph::ControlId::Referenced(muted, key + "/muted"), CheckBoxNode(muted, name, mutedHeading, true));
        if (Shows(disabled))
            builder.AddItem(graph::ControlId::Referenced(disabled, key + "/disabled"),
                CheckBoxNode(disabled, name, disabledHeading, false));
        builder.EndRow();
    }
}

void AddSortButtons(graph::GraphBuilder& builder, const Widget* table)
{
    std::span<const Widget* const> cells = agui::children(table);
    builder.BeginStop("sort");
    builder.StartRow();
    for (std::size_t column = 0; column < Columns && column < cells.size(); ++column)
    {
        const Widget* sort = FindDescendant(cells[column], "agui::TextButton");
        const Widget* caption = FindDescendant(cells[column], "agui::Label");
        if (!sort || !caption || !Shows(sort))
            continue;
        auto order = [table, column]()
        {
            std::optional<agui::TableOrdering> ordering = agui::tableOrdering(table);
            if (!ordering || ordering->column != column)
                return std::string();
            return std::string(Order(column, ordering->ascending));
        };
        graph::NodeVtable node =
            ControlNode(sort, [caption]() { return std::string(vocab::kSortBy(LabelText(caption))); });
        for (graph::NodeAnnouncement& part : node.Announcements)
            if (part.Kind == graph::AnnouncementKinds::Value)
                part.Text = order;
        node.StateText = order;
        builder.AddItem(graph::ControlId::Referenced(sort, "sort/" + std::to_string(column)), std::move(node));
    }
    builder.EndRow();
}

} // namespace

bool AlertsConfigScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "AlertsConfigGui"); }

void AlertsConfigScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* title = agui::frameTitle(window);
    std::string context = title ? text::speakable(agui::text(title)) : std::string();
    if (!context.empty())
        builder.PushContext(context);

    const Widget* table = agui::member(window, layout.alertsConfigTable);
    AddAlerts(builder, table);
    AddSortButtons(builder, table);

    if (!context.empty())
        builder.PopContext();
}

} // namespace fa::screens
