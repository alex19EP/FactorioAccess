#include "ScreenManager.hpp"

#include <algorithm>
#include <format>

#include "graph/GraphAnnouncer.hpp"
#include "input.h"
#include "speech.h"
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

namespace
{

// The ids without their backing objects: those belonged to the window that went away, and a new
// widget at a reused address must not be taken for one of them. Structural keys carry over.
graph::ControlId Unreferenced(const graph::ControlId& id)
{
    return id.IsValid() ? graph::ControlId::Structural(id.StructuralKey) : graph::ControlId();
}

void Unreference(graph::GraphState& state)
{
    state.CurKey = Unreferenced(state.CurKey);
    for (graph::ControlId& id : state.KeyOrder)
        id = Unreferenced(id);
    state.NextSuggestedMove = {};
    for (auto& [stop, id] : state.StopMemory)
        id = Unreferenced(id);
    graph::ControlIdSet expanded;
    for (const graph::ControlId& id : state.Expanded)
        expanded.insert(Unreferenced(id));
    state.Expanded = std::move(expanded);
}

} // namespace

void ScreenManager::Park(Screen* screen, std::unique_ptr<graph::GraphState> state)
{
    if (!state || !screen->RemembersCursor())
        return;
    std::string name = screen->DiagName();
    std::erase_if(_parked, [&](const ParkedEntry& e) { return e.Target == screen && e.Name == name; });
    Unreference(*state);
    _parked.push_back({screen, std::move(name), std::move(state)});
    if (_parked.size() > kParkedKept)
        _parked.erase(_parked.begin());
}

std::unique_ptr<graph::GraphState> ScreenManager::Unpark(Screen* screen)
{
    std::string name = screen->DiagName();
    for (auto it = _parked.begin(); it != _parked.end(); ++it)
        if (it->Target == screen && it->Name == name)
        {
            std::unique_ptr<graph::GraphState> state = std::move(it->State);
            _parked.erase(it);
            return state;
        }
    return std::make_unique<graph::GraphState>();
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

std::string ScreenManager::Describe()
{
    std::string out = "live:";
    for (const LiveEntry& e : _live)
        out += std::format(" {}", e.Target->DiagName());
    out += _shutdown ? " (shut down)\n" : "\n";
    return out + _navigator.Describe();
}

void ScreenManager::Update()
{
    if (_shutdown)
        return;

    // Poll and diff the active set (exception-isolated: a throwing IsActive reads as inactive).
    // Attach is DEBOUNCED: a screen goes live only after kAttachSettleFrames consecutive active
    // frames. Detach is immediate.
    std::string leaveLine;
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
            _live.push_back({screen.get(), Unpark(screen.get())});
        }
        else if (!active && live)
        {
            bool wasAttached = _navigator.AttachedScreen() == screen.get();
            if (wasAttached)
            {
                screen->OnUnfocus();
                _navigator.Detach();
            }
            for (LiveEntry& e : _live)
                if (e.Target == screen.get())
                    Park(screen.get(), std::move(e.State));
            screen->OnPop();
            if (wasAttached)
                leaveLine = screen->LeaveLine();
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
    // A screen taking over announces itself instead.
    if (!top && !leaveLine.empty())
        speech::say(leaveLine, true);

    _navigator.Update();
}

} // namespace fa::nav
