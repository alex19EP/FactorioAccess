#include "ModsScreen.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "AguiNodes.hpp"
#include "game.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;
using game::layout;

// The first child of the given kind, or null.
const Widget* ChildOfKind(const Widget* widget, agui::Kind kind)
{
    for (const Widget* child : VisibleChildren(widget))
        if (agui::kind(child) == kind)
            return child;
    return nullptr;
}

// The texts of a mod portal search result, as ExploreModsPane::addResult lays it out: a top flow
// of the thumbnail and a column of the title line (name, version, "deprecated"), the author and the
// summary; under it a flow of the category, when it was last updated, the downloads and "Space Age
// mod", each but the last after an icon.
struct Result
{
    std::vector<const Widget*> title;
    const Widget* author = nullptr;
    const Widget* summary = nullptr;
    std::vector<const Widget*> facts; // category, updated, downloads, then the Space Age label
};

std::optional<Result> ReadResult(const Widget* cell)
{
    std::vector<const Widget*> parts = VisibleChildren(cell);
    if (parts.size() < 2)
        return std::nullopt;
    std::vector<const Widget*> top = VisibleChildren(parts[0]);
    if (top.empty())
        return std::nullopt;
    std::vector<const Widget*> column = VisibleChildren(top.back());
    std::vector<const Widget*> title =
        column.empty() ? std::vector<const Widget*>() : FindAll(column[0], "agui::Label");
    if (column.size() < 3 || title.empty())
        return std::nullopt;
    return Result{std::move(title), column[1], column[2], FindAll(parts[1], "agui::Label")};
}

// "Rate Calculator 3.4.1, by raiguard, Utilities, 2 months ago, 344013 downloads": the name first,
// so a list is skimmed by its first words.
std::string ResultLine(const Result& result)
{
    // The version follows the name as on screen; "deprecated" is a part of its own.
    std::vector<std::string> parts{LabelText(result.title[0])};
    for (std::size_t i = 1; i < result.title.size(); ++i)
    {
        std::string phrase = LabelText(result.title[i]);
        if (phrase.empty())
            continue;
        if (i == 1)
            parts[0] += ' ' + phrase;
        else
            parts.push_back(phrase);
    }
    if (std::string author = LabelText(result.author); !author.empty())
        parts.push_back(vocab::kByAuthor(author));
    for (std::size_t i = 0; i < result.facts.size(); ++i)
    {
        std::string fact = LabelText(result.facts[i]);
        if (fact.empty())
            continue;
        parts.push_back(i == 2 ? vocab::kDownloads(fact) : fact);
    }
    std::string line;
    for (const std::string& part : parts)
        line += (line.empty() ? "" : ", ") + part;
    return line;
}

} // namespace

bool ModsScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "ModsGui"); }

void ModsScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* tabs = agui::member(window, layout.modsTabs);

    builder.BeginStop("tabs");
    if (const Widget* header = ChildOfKind(tabs, agui::Kind::Table))
        AddSubtree(builder, "tabs", header);

    if (agui::tabSelected(agui::member(window, layout.modsManageTab)))
    {
        BuildManage(builder, window);
    }
    else if (agui::tabSelected(agui::member(window, layout.modsExploreTab)))
    {
        BuildExplore(builder, window);
    }
    else
    {
        builder.BeginStop("content");
        if (const Widget* page = ChildOfKind(tabs, agui::Kind::Container))
            AddSubtree(builder, "content", page);
    }

    AddFooter(builder, window);
}

void ModsScreen::BuildExplore(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* pane = agui::pointerMember(window, layout.modsExplorePane);
    const Widget* filters = agui::member(pane, layout.exploreFilters);
    const Widget* results = agui::member(pane, layout.exploreResults);
    const Widget* count = agui::member(pane, layout.exploreResultsCount);
    const Widget* pages = agui::member(pane, layout.explorePages);

    // The sort order, the search field and what the game says about the search (no results, the
    // portal unreachable).
    builder.BeginStop("search");
    AddSubtree(builder, "search", pane, {filters, results, count, pages});

    // A result a line, its summary beside it. Enter selects it, as a click on its row does, and
    // the mod's details show.
    builder.BeginStop("results");
    std::span<const Widget* const> rows = agui::children(results);
    for (std::size_t row = 0; row < rows.size(); ++row)
    {
        std::optional<Result> result = ReadResult(rows[row]);
        if (!result || !Shows(rows[row]))
            continue;
        auto selected = [results, row]()
        { return agui::selectedRow(results) == row ? std::string(vocab::kSelected) : std::string(); };
        const Widget* name = result->title[0];
        graph::NodeVtable mod = TextNode(name, [result = *result]() { return ResultLine(result); });
        mod.Announcements.emplace_back(selected, true, graph::AnnouncementKinds::Selected);
        mod.OnActivate = [results, name]() { agui::pressOver(results, name, agui::MouseButton::Left, false, false); };
        mod.StateText = selected;

        std::string key = "results/" + std::to_string(row);
        builder.StartLine("result");
        builder.AddItem(graph::ControlId::Referenced(name, key), std::move(mod));
        const Widget* summary = result->summary;
        builder.AddItem(graph::ControlId::Referenced(summary, key + "/summary"),
            TextNode(summary, [summary]() { return LabelText(summary); }));
        builder.EndRow();
        if (agui::selectedRow(results) == row)
            builder.SetStart(graph::ControlId::Structural(key));
    }
    // How many mods the search found, and the pages they are shown on.
    if (Shows(count))
        builder.AddItem(graph::ControlId::Referenced(count, "results/count"),
            TextNode(count, [count]() { return LabelText(count); }));
    std::vector<const Widget*> pageButtons = VisibleChildren(pages);
    if (Shows(pages) && !pageButtons.empty())
    {
        builder.StartRow("pages");
        for (std::size_t i = 0; i < pageButtons.size(); ++i)
        {
            const Widget* button = pageButtons[i];
            std::function<std::string()> name;
            if (i == 0)
                name = []() { return std::string(vocab::kPreviousPage); };
            else if (i + 1 == pageButtons.size())
                name = []() { return std::string(vocab::kNextPage); };
            else
                name = [button]() { return vocab::kPage(OwnText(button)); };
            AddControl(builder, "results/pages/" + std::to_string(i), button, std::move(name));
        }
        builder.EndRow();
    }

    // The filters, in groups under the game's headings (expansions, categories, tags, filters),
    // each group a Ctrl+arrow region announced as focus enters it. A portal filter is its checkbox,
    // read with the mods it matches, and beside it the button that excludes it from the search.
    builder.BeginStop("filters");
    bool inGroup = false;
    std::span<const Widget* const> cells = agui::children(filters);
    for (std::size_t i = 0; i < cells.size(); ++i)
    {
        const Widget* cell = cells[i];
        if (!Shows(cell))
            continue;
        std::string key = "filters/" + std::to_string(i);
        switch (agui::kind(cell))
        {
        case agui::Kind::Label:
            if (inGroup)
                builder.PopContext();
            builder.SetRegion(key);
            builder.PushContext(LabelText(cell));
            inGroup = true;
            break;
        case agui::Kind::CheckBox:
            AddControl(builder, key, cell);
            break;
        default:
        {
            const Widget* box = ChildOfKind(cell, agui::Kind::CheckBox);
            if (!box)
                break;
            const Widget* hits = agui::member(box, layout.searchFilterHits);
            const Widget* exclude = agui::member(box, layout.searchFilterExclude);
            graph::NodeVtable filter = ControlNode(box,
                [box, hits]()
                {
                    std::string name = NameOf(box);
                    std::string count = LabelText(hits);
                    return count.empty() ? name : name + ' ' + count;
                });
            // An excluded filter's checkbox is disabled: it says why.
            for (graph::NodeAnnouncement& part : filter.Announcements)
                if (part.Kind == graph::AnnouncementKinds::Enabled)
                    part.Text = [box, exclude]()
                    {
                        if (agui::buttonToggled(exclude))
                            return std::string(vocab::kExcluded);
                        return agui::enabled(box) ? std::string() : std::string(vocab::kDisabled);
                    };
            builder.StartLine("filter");
            builder.AddItem(graph::ControlId::Referenced(box, key), std::move(filter));
            AddControl(builder, key + "/exclude", exclude);
            builder.EndRow();
            break;
        }
        }
    }
    if (inGroup)
        builder.PopContext();
    builder.SetRegion("");
}

void ModsScreen::BuildManage(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* pane = agui::member(window, layout.modsManagePane);
    const Widget* table = agui::member(pane, layout.manageModsTable);

    std::span<const Widget* const> cells = agui::children(table);
    std::size_t columns = std::max(agui::tableColumns(table), 1u);
    std::size_t rows = cells.size() / columns;

    builder.BeginStop("mods");
    if (rows > 0)
    {
        if (const Widget* all = FindDescendant(cells[0], "agui::CheckBox"))
            AddControl(builder, "mods/all", all,
                [all]()
                {
                    std::string name = NameOf(all);
                    return name.empty() ? std::string(vocab::kAllMods) : name;
                });
    }
    for (std::size_t row = 1; row < rows; ++row)
    {
        const Widget* enabled = FindDescendant(cells[row * columns], "agui::CheckBox");
        if (!enabled)
            continue;
        // Every other cell of the row is text: the name, the version.
        std::vector<const Widget*> texts;
        for (std::size_t column = 1; column < columns; ++column)
            for (const Widget* label : FindAll(cells[row * columns + column], "agui::Label"))
                texts.push_back(label);
        auto name = [texts]()
        {
            std::string line;
            for (const Widget* label : texts)
            {
                std::string phrase = LabelText(label);
                if (phrase.empty())
                    continue;
                if (!line.empty())
                    line += ' ';
                line += phrase;
            }
            return line;
        };
        if (texts.empty() || !Shows(enabled))
            continue;

        // The name: a click on it selects the mod, as the table picks the row under the mouse.
        auto selected = [table, row]()
        { return agui::selectedRow(table) == row ? std::string(vocab::kSelected) : std::string(); };
        graph::NodeVtable mod = TextNode(texts.front(), name);
        mod.Announcements.emplace_back(selected, true, graph::AnnouncementKinds::Selected);
        const Widget* target = texts.front();
        mod.OnActivate = [table, target]() { agui::pressOver(table, target, agui::MouseButton::Left, false, false); };
        mod.StateText = selected;

        std::string key = "mods/" + std::to_string(row);
        builder.StartRow("mod");
        builder.AddItem(graph::ControlId::Referenced(target, key), std::move(mod));
        // Then its checkbox, which enables or disables it.
        AddControl(builder, key + "/enabled", enabled);
        builder.EndRow();
        if (agui::selectedRow(table) == row)
            builder.SetStart(graph::ControlId::Structural(key));
    }

    builder.BeginStop("info");
    AddSubtree(builder, "info", agui::member(pane, layout.manageModsInfo));

    builder.BeginStop("search");
    const Widget* search = agui::member(pane, layout.manageModsSearch);
    AddControl(builder, "search/bar", search);
    if (const Widget* popup = FindDescendant(pane, "SearchPopup"))
        AddSubtree(builder, "search/popup", popup);
    // The header's sort buttons, named by the column they sort.
    for (std::size_t column = 0; rows > 0 && column < columns; ++column)
    {
        const Widget* sort = FindDescendant(cells[column], "agui::TextButton");
        const Widget* caption = FindDescendant(cells[column], "agui::Label");
        if (!sort || !caption)
            continue;
        AddControl(builder, "search/sort" + std::to_string(column), sort,
            [caption]() { return vocab::kSortBy(LabelText(caption)); });
    }
}

} // namespace fa::screens
