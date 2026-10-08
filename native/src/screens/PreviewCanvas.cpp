#include "PreviewCanvas.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <utility>

#include "preview.h"

namespace fa::screens
{

void PreviewCanvas::Add(graph::GraphBuilder& builder, const agui::Widget* picture, const std::string& key,
    std::function<std::string()> gridPosition)
{
    std::optional<preview::Box> extent = preview::extent(picture);
    if (!extent)
        return;
    const preview::Box box = *extent;
    // A new picture starts at its top left corner; the include checkboxes can shrink the blueprint
    // under the cursor.
    if (picture != _picture)
    {
        _picture = picture;
        _x = box.left;
        _y = box.top;
        _gridSet = false;
    }
    _x = std::clamp(_x, box.left, box.right - 1);
    _y = std::clamp(_y, box.top, box.bottom - 1);

    auto inside = [box](int x, int y) { return x >= box.left && x < box.right && y >= box.top && y < box.bottom; };

    graph::NodeVtable vtable;
    vtable.HostTag = picture;
    vtable.SpeaksOwnPosition = true;
    vtable.Announcements.emplace_back([this, picture]() { return preview::describe(picture, _x, _y); }, true,
        graph::AnnouncementKinds::Label);
    vtable.OnMoveWithin = [this, picture, inside](graph::GraphDir dir, bool skip)
    {
        int dx = dir == graph::GraphDir::Left ? -1 : dir == graph::GraphDir::Right ? 1 : 0;
        int dy = dir == graph::GraphDir::Up ? -1 : dir == graph::GraphDir::Down ? 1 : 0;
        int x = _x + dx;
        int y = _y + dy;
        if (!inside(x, y))
            return false;
        // As the map cursor skips: on to the first tile that reads differently, or the edge.
        if (skip)
        {
            std::string from = preview::describe(picture, _x, _y);
            while (inside(x + dx, y + dy) && preview::describe(picture, x, y) == from)
            {
                x += dx;
                y += dy;
            }
        }
        _x = x;
        _y = y;
        return true;
    };
    vtable.OnEdgeWithin = [this, box](bool home)
    {
        int x = home ? box.left : box.right - 1;
        if (x == _x)
            return false;
        _x = x;
        return true;
    };
    vtable.PositionText = [this, box]() { return std::format("{}, {}", _x - box.left + 1, _y - box.top + 1); };
    if (preview::editable(picture))
    {
        vtable.OnActivate = [this, picture]() { preview::click(picture, _x, _y, false); };
        vtable.OnSecondary = [this, picture]() { preview::click(picture, _x, _y, true); };
        if (gridPosition)
            vtable.OnActivateShift = [this, picture]()
            {
                preview::setGridPosition(picture, _x, _y);
                _gridSet = true;
            };
        vtable.StateText = [this, picture, gridPosition]()
        {
            if (std::exchange(_gridSet, false) && gridPosition)
                return gridPosition();
            return preview::describe(picture, _x, _y);
        };
    }
    builder.AddItem(graph::ControlId::Referenced(picture, key), std::move(vtable));
}

} // namespace fa::screens
