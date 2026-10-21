#include "EquipmentGridCanvas.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <vector>

#include "AguiNodes.hpp"
#include "agui.h"
#include "speech.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::EquipmentGridView;
using agui::EquipmentPiece;
using agui::Widget;

// The cursor over the grid shown; a new grid starts at its top left cell.
struct Cursor
{
    const Widget* holder = nullptr;
    int x = 0;
    int y = 0;
};
Cursor g_cursor;

// The index of the piece covering the cell, or -1 for an empty one.
int PieceAt(const EquipmentGridView& grid, int x, int y)
{
    for (std::size_t i = 0; i < grid.pieces.size(); ++i)
        if (grid.pieces[i].covers(x, y))
            return static_cast<int>(i);
    return -1;
}

std::string BaseName(const EquipmentPiece& piece)
{
    return piece.quality.empty() ? std::string(piece.name) : std::format("{} {}", piece.quality, piece.name);
}

// The piece's name, numbered when others share it: in reading order of their top left cells.
std::string PieceName(const EquipmentGridView& grid, const EquipmentPiece& piece)
{
    std::string name = BaseName(piece);
    int count = 0;
    int number = 1;
    for (const EquipmentPiece& other : grid.pieces)
    {
        if (BaseName(other) != name)
            continue;
        ++count;
        if (other.y < piece.y || (other.y == piece.y && other.x < piece.x))
            ++number;
    }
    return count > 1 ? vocab::kEquipmentNumbered(name, number) : name;
}

// Whether the held equipment fits with its top left on the cell; if not, what is in the way.
std::string FitText(const Widget* holder, const EquipmentGridView& grid, const EquipmentPiece& held, int x, int y,
    bool ghost)
{
    if (agui::canPlaceHeld(holder, x, y, ghost))
        return vocab::kEquipmentFits;
    if (x + held.width > grid.width || y + held.height > grid.height)
        return vocab::kEquipmentDoesNotFit;
    std::vector<int> blocking;
    for (int row = 0; row < held.height; ++row)
        for (int column = 0; column < held.width; ++column)
        {
            if (!held.covers(column, row))
                continue;
            int index = PieceAt(grid, x + column, y + row);
            if (index >= 0 && !std::ranges::contains(blocking, index))
                blocking.push_back(index);
        }
    if (blocking.empty())
        return vocab::kEquipmentDoesNotFit;
    std::string names;
    for (int index : blocking)
        names += (names.empty() ? "" : ", ") + PieceName(grid, grid.pieces[static_cast<std::size_t>(index)]);
    return vocab::kEquipmentBlocked(names);
}

std::string CellText(const Widget* holder, int x, int y)
{
    std::optional<EquipmentGridView> grid = agui::equipmentGrid(holder);
    if (!grid)
        return {};
    std::string text;
    if (int index = PieceAt(*grid, x, y); index >= 0)
    {
        const EquipmentPiece& piece = grid->pieces[static_cast<std::size_t>(index)];
        text = vocab::kEquipmentPiece(PieceName(*grid, piece), piece.width, piece.height);
        if (piece.ghost)
            text = std::format("{}, {}", text, std::string(vocab::kEquipmentGhost));
        if (piece.toBeRemoved)
            text = std::format("{}, {}", text, std::string(vocab::kToBeRemoved));
    }
    else
        text = vocab::kEmpty;
    if (std::optional<EquipmentPiece> held = agui::heldEquipment(holder))
        text = std::format("{}, {}", text, FitText(holder, *grid, *held, x, y, false));
    return text;
}

// The game's click at the cell's centre.
void ClickCell(const Widget* holder, int x, int y, agui::MouseButton button, bool shift, bool control)
{
    auto [width, height] = agui::equipmentCellSize(holder);
    agui::hoverEquipmentCell(holder, x, y);
    agui::pressAt(holder, x * width + width / 2, y * height + height / 2, button, shift, control);
}

// The game's left click on the cell. Equipment in hand over an empty cell goes in with its top left
// there: the game centres it on the mouse, so the click lands on the middle of where it goes (Shift
// places its ghost). Where it does not fit nothing is clicked, so it never lands elsewhere.
void LeftClick(const Widget* holder, int x, int y, bool shift, bool control)
{
    std::optional<EquipmentGridView> grid = agui::equipmentGrid(holder);
    if (!grid)
        return;
    std::optional<EquipmentPiece> held = agui::heldEquipment(holder);
    if (!held || control || PieceAt(*grid, x, y) >= 0)
    {
        ClickCell(holder, x, y, agui::MouseButton::Left, shift, control);
        return;
    }
    if (!agui::canPlaceHeld(holder, x, y, shift))
    {
        speech::say(FitText(holder, *grid, *held, x, y, shift));
        return;
    }
    auto [width, height] = agui::equipmentCellSize(holder);
    agui::hoverEquipmentCell(holder, x, y);
    agui::pressAt(holder, (2 * x + held->width) * width / 2, (2 * y + held->height) * height / 2,
        agui::MouseButton::Left, shift, false);
}

} // namespace

void AddEquipmentGrid(graph::GraphBuilder& builder, const std::string& key, const Widget* holder)
{
    std::optional<EquipmentGridView> grid = agui::equipmentGrid(holder);
    if (!grid || grid->width <= 0 || grid->height <= 0)
        return;
    const int width = grid->width;
    const int height = grid->height;
    if (holder != g_cursor.holder)
        g_cursor = {holder, 0, 0};
    // A grid can shrink under the cursor: a vehicle's changes with its quality.
    g_cursor.x = std::clamp(g_cursor.x, 0, width - 1);
    g_cursor.y = std::clamp(g_cursor.y, 0, height - 1);

    auto inside = [width, height](int x, int y) { return x >= 0 && x < width && y >= 0 && y < height; };

    graph::NodeVtable vtable;
    vtable.HostTag = holder;
    vtable.SpeaksOwnPosition = true;
    vtable.Announcements.emplace_back(
        [holder]() { return CellText(holder, g_cursor.x, g_cursor.y); }, true, graph::AnnouncementKinds::Label);
    vtable.OnMoveWithin = [holder, inside](graph::GraphDir dir, bool skipping)
    {
        int dx = dir == graph::GraphDir::Left ? -1 : dir == graph::GraphDir::Right ? 1 : 0;
        int dy = dir == graph::GraphDir::Up ? -1 : dir == graph::GraphDir::Down ? 1 : 0;
        int x = g_cursor.x + dx;
        int y = g_cursor.y + dy;
        if (!inside(x, y))
            return false;
        if (skipping)
        {
            // On to the first cell another piece covers, or else the last before the edge.
            std::optional<EquipmentGridView> grid = agui::equipmentGrid(holder);
            if (!grid)
                return false;
            int from = PieceAt(*grid, g_cursor.x, g_cursor.y);
            while (inside(x + dx, y + dy) && PieceAt(*grid, x, y) == from)
            {
                x += dx;
                y += dy;
            }
        }
        g_cursor.x = x;
        g_cursor.y = y;
        agui::hoverEquipmentCell(holder, x, y);
        return true;
    };
    vtable.OnEdgeWithin = [holder, width](bool home)
    {
        int x = home ? 0 : width - 1;
        if (x == g_cursor.x)
            return false;
        g_cursor.x = x;
        agui::hoverEquipmentCell(holder, g_cursor.x, g_cursor.y);
        return true;
    };
    vtable.PositionText = []() { return std::format("{}, {}", g_cursor.x + 1, g_cursor.y + 1); };
    vtable.OnActivate = [holder]() { LeftClick(holder, g_cursor.x, g_cursor.y, false, false); };
    vtable.OnActivateShift = [holder]() { LeftClick(holder, g_cursor.x, g_cursor.y, true, false); };
    vtable.OnActivateCtrl = [holder]() { LeftClick(holder, g_cursor.x, g_cursor.y, false, true); };
    vtable.OnSecondary = [holder]()
    { ClickCell(holder, g_cursor.x, g_cursor.y, agui::MouseButton::Right, false, false); };
    vtable.OnTooltip = [holder]()
    {
        agui::hoverEquipmentCell(holder, g_cursor.x, g_cursor.y);
        SpeakGameTooltip(holder);
    };
    builder.AddItem(graph::ControlId::Referenced(holder, key), std::move(vtable));
}

} // namespace fa::screens
