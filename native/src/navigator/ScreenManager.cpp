#include "ScreenManager.hpp"

#include <algorithm>

#include "graph/GraphAnnouncer.hpp"
#include "input.h"
#include "vocab.h"

namespace fa::nav
{

ScreenManager& ScreenManager::Get()
{
    static ScreenManager instance;
    return instance;
}

ScreenManager::ScreenManager()
{
    // Host wiring for the announcer's pluggable vocabulary (A8).
    graph::GraphAnnouncer::PositionText = &vocab::position;
    graph::GraphAnnouncer::ExpandedStateText = &vocab::expandedState;
    graph::GraphAnnouncer::FlyoutHintText = &vocab::flyoutHint;
}

void ScreenManager::Register(std::unique_ptr<Screen> screen)
{
    _registry.push_back(std::move(screen));
    _settle.push_back(0);
}

bool ScreenManager::IsLive(const Screen* screen) const
{
    for (const LiveEntry& e : _live)
        if (e.Target == screen)
            return true;
    return false;
}

void ScreenManager::Shutdown()
{
    if (_shutdown)
        return;
    _shutdown = true;
    _navigator.Detach(); // also releases the key claims
    _live.clear();
    input::clearClaims();
}

void ScreenManager::Update()
{
    if (_shutdown)
        return;

    // Poll and diff the active set (exception-isolated: a throwing IsActive reads as inactive).
    // Attach is DEBOUNCED: a screen goes live only after kAttachSettleFrames consecutive active
    // frames. Detach is immediate.
    for (std::size_t i = 0; i < _registry.size(); ++i)
    {
        const auto& screen = _registry[i];
        bool activeNow = false;
        try
        {
            activeNow = screen->IsActive();
        }
        catch (...)
        {
        }

        int& settle = _settle[i];
        settle = activeNow ? std::min(settle + 1, kAttachSettleFrames) : 0;
        bool active = settle >= kAttachSettleFrames;

        bool live = IsLive(screen.get());
        if (active && !live)
        {
            screen->OnPush();
            _live.push_back({screen.get(), std::make_unique<graph::GraphState>()});
        }
        else if (!active && live)
        {
            if (_navigator.AttachedScreen() == screen.get())
            {
                screen->OnUnfocus();
                _navigator.Detach();
            }
            screen->OnPop();
            std::erase_if(_live, [&](const LiveEntry& e) { return e.Target == screen.get(); });
        }
    }

    // The highest layer owns the navigator; ties go to the most recently pushed.
    Screen* top = nullptr;
    graph::GraphState* topState = nullptr;
    for (LiveEntry& e : _live)
        if (!top || e.Target->Layer() >= top->Layer())
        {
            top = e.Target;
            topState = e.State.get();
        }

    if (top != _navigator.AttachedScreen())
    {
        if (_navigator.IsAttached())
        {
            _navigator.AttachedScreen()->OnUnfocus();
            _navigator.Detach();
        }
        if (top)
        {
            _navigator.Attach(top, topState);
            top->OnFocus();
        }
    }

    _navigator.Update();
}

} // namespace fa::nav
