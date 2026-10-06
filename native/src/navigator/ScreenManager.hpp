#pragma once

// The screen manager (graph-a11y-spec §9): polls every registered screen's IsActive() each tick
// (exception-isolated), diffs the active set against the live list, and attaches the navigator
// to the highest-layer live screen. Poll-and-diff — not event subscription — is what makes this
// robust to the game recreating its UI objects at will.
//
// Per-screen cursor state: each live screen keeps its own GraphState; a screen covered by a
// higher layer keeps its state and restores exactly where the user was when focus returns. A
// popped screen's state is dropped — reopening starts fresh — unless the screen remembers its
// cursor (Screen::RemembersCursor): the game destroys a window behind a dialog it opens and builds
// it anew afterwards, and the player expects to be back where they were.
//
// Ported from CyberAccess (src/Navigator/ScreenManager.*).

#include <memory>
#include <string>
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

    /// For the dev server: the live screens, then what the navigator shows (Navigator::Describe).
    std::string Describe();

private:
    ScreenManager();

    struct LiveEntry
    {
        Screen* Target;
        std::unique_ptr<graph::GraphState> State;
    };

    bool IsLive(const Screen* screen) const;

    /// Keeps a popped screen's cursor if it asks for that (Screen::RemembersCursor), and hands it
    /// back when the same screen next comes up showing the same thing (its DiagName).
    void Park(Screen* screen, std::unique_ptr<graph::GraphState> state);
    std::unique_ptr<graph::GraphState> Unpark(Screen* screen);

    struct ParkedEntry
    {
        Screen* Target;
        std::string Name;
        std::unique_ptr<graph::GraphState> State;
    };
    static constexpr std::size_t kParkedKept = 16;
    std::vector<ParkedEntry> _parked; // oldest first

    // A screen attaches only after IsActive holds for this many consecutive frames. The game builds
    // a window whole within one call and the manager runs after Gui::logic, so one frame of
    // confirmation is enough; every extra frame is a delay the player hears.
    static constexpr int kAttachSettleFrames = 2;

    std::vector<std::unique_ptr<Screen>> _registry;
    std::vector<int> _settle; // consecutive-active frame count, aligned with _registry
    std::vector<LiveEntry> _live;
    Navigator _navigator;
    bool _shutdown = false;
};

} // namespace fa::nav
