#include "CraftingQueueScreen.hpp"

#include <format>
#include <string>
#include <vector>

#include "AguiNodes.hpp"
#include "parts.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;

// The crafting queue, while it is on screen over a loaded game with no menu in front of it.
const Widget* FindQueue()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return nullptr;
    const Widget* queue = agui::craftingQueue();
    return queue && agui::visible(queue) ? queue : nullptr;
}

} // namespace

const char* CraftingQueueScreen::Name() const { return vocab::kCraftingQueue.data(); }

bool CraftingQueueScreen::IsActive()
{
    if (parts::current() != parts::Part::CraftingQueue)
        return false;
    _queue = FindQueue();
    // Nothing to go back to once the game is gone.
    if (!agui::inGame())
        parts::close();
    return _queue != nullptr;
}

void CraftingQueueScreen::Build(graph::GraphBuilder& builder)
{
    if (!_queue || FindQueue() != _queue)
        return;
    std::vector<const Widget*> slots = agui::craftingQueueSlots(_queue);
    builder.BeginStop("queue");
    if (slots.empty())
    {
        builder.AddItem(graph::ControlId::Referenced(_queue, "queue/empty"),
            TextNode(_queue, []() { return std::string(vocab::kEmpty); }));
        return;
    }
    // Keyed by place, as the game rebuilds every slot when the queue changes: cancelling an order
    // leaves the cursor on the one that took its place.
    builder.StartRow("queue");
    for (std::size_t i = 0; i < slots.size(); ++i)
        builder.AddItem(graph::ControlId::Referenced(slots[i], std::format("queue/{}", i)), ControlNode(slots[i]));
    builder.EndRow();
}

void CraftingQueueScreen::OnEscape() { parts::close(); }

void CraftingQueueScreen::OnPop() { _queue = nullptr; }

std::string CraftingQueueScreen::LeaveLine() const
{
    // Left with Ctrl+Tab or Escape, not covered by a menu: the crafting queue is no longer in use.
    return parts::current() == parts::Part::None ? std::string(vocab::kMap) : std::string();
}

} // namespace fa::screens
