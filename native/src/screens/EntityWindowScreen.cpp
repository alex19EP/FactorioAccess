#include "EntityWindowScreen.hpp"

#include <algorithm>
#include <format>
#include <iterator>
#include <memory>
#include <utility>

#include "GuiDump.hpp"
#include "entityviews.h"
#include "game.h"

namespace fa::screens
{

namespace
{

using agui::Widget;

graph::ControlId CellId(std::size_t view, std::size_t column, std::size_t row)
{
    return graph::ControlId::Structural(std::format("views/{}/{}/{}", view, column, row));
}

// A view's columns side by side: Up and Down within a column, Left and Right to the next column
// that has cells, at the same row or its last when it is shorter.
void AddView(graph::GraphBuilder& builder, std::size_t index, const entityviews::View& view)
{
    builder.BeginStop(std::format("views/{}", index));
    builder.PushContext(view.title);
    std::vector<std::size_t> filled;
    for (std::size_t c = 0; c < view.columns.size(); c++)
    {
        const entityviews::Column& column = view.columns[c];
        if (column.cells.empty())
            continue;
        filled.push_back(c);
        if (!column.title.empty())
            builder.PushContext(column.title);
        for (std::size_t r = 0; r < column.cells.size(); r++)
        {
            graph::NodeVtable vtable;
            vtable.Announcements.push_back(graph::NodeAnnouncement::Static(column.cells[r]));
            builder.AddNode(CellId(index, c, r), std::move(vtable));
            if (r > 0)
            {
                builder.Connect(CellId(index, c, r - 1), graph::GraphDir::Down, CellId(index, c, r));
                builder.Connect(CellId(index, c, r), graph::GraphDir::Up, CellId(index, c, r - 1));
            }
        }
        if (!column.title.empty())
            builder.PopContext();
    }
    for (std::size_t i = 1; i < filled.size(); i++)
    {
        std::size_t left = filled[i - 1];
        std::size_t right = filled[i];
        std::size_t leftRows = view.columns[left].cells.size();
        std::size_t rightRows = view.columns[right].cells.size();
        for (std::size_t r = 0; r < leftRows; r++)
            builder.Connect(CellId(index, left, r), graph::GraphDir::Right,
                CellId(index, right, std::min(r, rightRows - 1)));
        for (std::size_t r = 0; r < rightRows; r++)
            builder.Connect(CellId(index, right, r), graph::GraphDir::Left,
                CellId(index, left, std::min(r, leftRows - 1)));
    }
    builder.PopContext();
}

} // namespace

const Widget* EntityWindowScreen::FindWindow() const
{
    // Only over plain play: the game menu takes over.
    if (!agui::inGame() || agui::menuStateWindow())
        return nullptr;
    const agui::Gui* gui = agui::applicationGui();
    const Widget* root = gui ? agui::baseWidget(gui) : nullptr;
    if (!root)
        return nullptr;
    const Widget* top = nullptr;
    for (const Widget* child : agui::children(root))
        if (agui::visible(child) && Handles(child))
            top = child;
    return top;
}

bool EntityWindowScreen::IsActive()
{
    const Widget* window = FindWindow();
    if (!window)
    {
        _window = nullptr;
        return false;
    }
    // Keyed by class too, not address alone: the game reuses a closed window's memory.
    const std::string& cls = agui::className(window);
    if (_window && (window != _window || cls != _class))
    {
        // A different window: one inactive frame pops this screen, so the next one starts fresh.
        _window = nullptr;
        return false;
    }
    _window = window;
    _class = cls;
    return true;
}

void EntityWindowScreen::Build(graph::GraphBuilder& builder)
{
    // A click can close the window or replace it, so it is looked up again before anything in it
    // is read.
    if (!_window || FindWindow() != _window)
        return;
    DumpWindow(_window, _class);
    BuildWindow(builder, _window);
}

bool EntityWindowScreen::TypingIn(const graph::GraphNode& node) { return TypingInField(node); }

void EntityWindowScreen::OnCursorMoved(const graph::GraphNode& node) { FollowCursor(node); }

void EntityWindowScreen::OnPop() { _window = nullptr; }

void EntityWindowScreen::AddTitledWindow(graph::GraphBuilder& builder, const std::string& key, const Widget* window,
    std::vector<const Widget*> skip, Attachments attachments)
{
    builder.BeginStop(key);
    const Widget* header = agui::member(window, game::layout.frameHeader);
    skip.push_back(header);
    // The title bar after the content, so the stop opens on the entity's status.
    const Widget* title = agui::frameTitle(window);
    std::vector<const Widget*> headerSkip{title};
    std::ranges::copy(FindAll(header, "CloseButton"), std::back_inserter(headerSkip));
    std::ranges::copy(FindAll(header, "SearchBar"), std::back_inserter(headerSkip));

    std::string titleText = title && Shows(title) ? LabelText(title) : std::string();
    if (!titleText.empty())
        builder.PushContext(titleText);
    AddSubtree(builder, key, window, std::move(skip), std::move(attachments));
    AddSubtree(builder, key + "/header", header, std::move(headerSkip));
    if (!titleText.empty())
        builder.PopContext();
}

void EntityWindowScreen::AddSidePanel(graph::GraphBuilder& builder, const Widget* sidePanel)
{
    if (!Shows(sidePanel) || !HasContent(sidePanel))
        return;
    builder.BeginStop("panel");
    AddSubtree(builder, "panel", sidePanel);
}

void EntityWindowScreen::AddModViews(graph::GraphBuilder& builder, uint64_t unitNumber)
{
    std::shared_ptr<const entityviews::Views> views = entityviews::current();
    if (!views || views->unitNumber != unitNumber)
        return;
    for (std::size_t i = 0; i < views->views.size(); i++)
        AddView(builder, i, views->views[i]);
}

} // namespace fa::screens
