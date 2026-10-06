#pragma once

// The screen manager (graph-a11y-spec §9): polls every registered screen's IsActive() each tick
// (exception-isolated), diffs the active set against the live list, and attaches the navigator
// to the highest-layer live screen. Poll-and-diff — not event subscription — is what makes this
// robust to the game recreating its UI objects at will.
//
// Per-screen cursor state: each live screen keeps its own GraphState; a screen covered by a
// higher layer keeps its state and restores exactly where the user was when focus returns. A
// popped screen's state is dropped — reopening starts fresh.
//
// Ported from CyberAccess (src/Navigator/ScreenManager.*).

#include <memory>
#include <vector>

#include "Navigator.hpp"
#include "Screen.hpp"

namespace fa::nav
{

class ScreenManager
{
public:
    static ScreenManager& Get();

    /// Register a screen recipe. Call at load; the manager owns the screen.
    void Register(std::unique_ptr<Screen> screen);

    /// Once per frame, on the game thread.
    void Update();

    /// Drop every screen and release the keys, for good: used when our own code faulted and
    /// nothing may touch the game again.
    void Shutdown();

private:
    ScreenManager();

    struct LiveEntry
    {
        Screen* Target;
        std::unique_ptr<graph::GraphState> State;
    };

    bool IsLive(const Screen* screen) const;

    // A screen attaches only after IsActive holds for this many consecutive frames, which keeps
    // Build's deep reads out of the frames where the game is still constructing the screen.
    static constexpr int kAttachSettleFrames = 10;

    std::vector<std::unique_ptr<Screen>> _registry;
    std::vector<int> _settle; // consecutive-active frame count, aligned with _registry
    std::vector<LiveEntry> _live;
    Navigator _navigator;
    bool _shutdown = false;
};

} // namespace fa::nav
