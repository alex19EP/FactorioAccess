#pragma once

// A screen recipe (graph-a11y-spec §9): an IsActive() predicate over game state, a Build()
// declaring the parallel tree from live game state, and lifecycle hooks. Screens hold NO view
// state — every render reads the game fresh (axiom A2); the only persistent thing is the
// per-screen GraphState the manager keeps while the screen is live.
//
// Ported from CyberAccess (src/Navigator/Screen.hpp), without screen keys, quick-nav and holds,
// which nothing here needs yet.

#include <string>

#include "graph/GraphBuilder.hpp"

namespace fa::nav
{

class Screen
{
public:
    virtual ~Screen() = default;

    /// Spoken (queued) when the screen gains the navigator's focus. Empty = nothing is said, and
    /// the landing announcement (its context path) introduces the screen instead.
    virtual std::string Name() const = 0;

    /// Identity for LOGS, never spoken, and the same in every language.
    virtual const char* DiagName() const = 0;

    /// Whether the cursor survives the screen going away, to be back where it was when the same
    /// thing (its DiagName) shows again. For windows the game destroys and rebuilds around a
    /// dialog; a popup that should open on its current choice leaves it false.
    virtual bool RemembersCursor() const { return false; }

    /// Stacking order: the live screen with the highest layer owns the navigator.
    virtual int Layer() const { return 0; }

    /// Polled every tick (exception-isolated by the manager). True while this screen's UI is up.
    virtual bool IsActive() = 0;

    /// Declare the parallel tree from live game state. Called per operation and per frame; must
    /// be cheap. Throwing is tolerated (§7.8): the frame renders nothing and focus state
    /// survives to reconcile on the next good render.
    virtual void Build(graph::GraphBuilder& builder) = 0;

    /// A one-shot landing request, consumed by the render that Build just produced. ONLY for
    /// content that was replaced wholesale (a new window opened): it overrides the player's
    /// cursor, so it is cleared by the read. Return a STRUCTURAL key, or nullptr.
    virtual const char* TakeSuggestedLanding() { return nullptr; }

    /// False for announce-only screens: the navigator claims NO keys and only speaks what the
    /// frame differ sees.
    virtual bool ClaimsKeys() const { return true; }

    /// True while the game itself is taking typed text on the focused node (an editable text
    /// field). The navigator then lets every editing key through and keeps only the keys that
    /// leave the field: Tab, Up and Down.
    virtual bool TypingIn(const graph::GraphNode& node)
    {
        (void)node;
        return false;
    }

    /// True for a screen the game knows nothing of as a window (a part of the HUD), whose Escape
    /// must not reach the game's own Back. The navigator then takes Escape and calls OnEscape.
    virtual bool ClaimsEscape() const { return false; }
    virtual void OnEscape() {}

    /// Said when this screen goes away with no screen to take the navigator after it, so the player
    /// knows where they are (a part of the HUD left for the map). Asked after OnPop. Empty: nothing.
    virtual std::string LeaveLine() const { return {}; }

    /// The focused identity changed (arrow move, differ jump, attach landing). Drive the game's
    /// own focus here (P11 write-back). Called after this frame's Build on a live render;
    /// exception-isolated by the navigator.
    virtual void OnCursorMoved(const graph::GraphNode& node) { (void)node; }

    // Lifecycle (optional).
    virtual void OnPush() {}
    virtual void OnPop() {}
    virtual void OnFocus() {}
    virtual void OnUnfocus() {}
};

} // namespace fa::nav
