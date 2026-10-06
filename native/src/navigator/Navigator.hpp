#pragma once

// The navigator (graph-a11y-spec §7): wires drained key events to kernel operations and owns ALL
// speech. Normative policies implemented here:
//   §7.1 standard key model (arrows/adjust/tree, Tab stops, Home/End, Ctrl+arrows regions,
//        Enter family, tooltip verb, secondary)
//   §7.2 frame differ — a focus change is spoken exactly once, no matter what caused it
//   §7.4 synchronous StateText feedback after activate/adjust (interrupting)
//   §7.5 live watch — Live parts of the focused node re-speak on change
//   §7.8 build isolation — a throwing Build renders nothing this frame and is logged once
//
// Ported from CyberAccess (src/Navigator/Navigator.*). Keys arrive as SDL3 keycodes from the
// input hook instead of Windows virtual keys, and the screen-key, quick-nav and tap-vs-hold
// machinery is left out until a screen needs it.
//
// Speech interrupt policy (spec A7): every keypress interrupts what is being said, so the reply
// to the key the player just pressed is never queued behind stale speech. The frame differ and
// the live watch queue, since they follow whatever caused them.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Screen.hpp"
#include "graph/KeyGraph.hpp"
#include "input.h"

namespace fa::nav
{

class Navigator
{
public:
    /// Attach to a screen (the manager's choice of the top live screen). Resets the differ
    /// memory, announces the screen name (queued), and lets the frame differ announce the
    /// landing.
    void Attach(Screen* screen, graph::GraphState* state);

    void Detach();

    bool IsAttached() const { return _screen != nullptr; }
    Screen* AttachedScreen() const { return _screen; }

    /// True when the attached screen produced a non-empty render this frame.
    bool HasLiveRender() const { return _hasLiveRender; }

    /// Once per frame, on the game thread: drain input, run operations, run the frame differ and
    /// the live watch, publish the claim set.
    void Update();

    /// For the dev server: the attached screen, the focused node with its full spoken line, and
    /// every node in declaration order under its Tab-stop. Rebuilds the render to read it fresh.
    std::string Describe();

private:
    static constexpr bool kInterruptOnKeypress = true;

    void HandleKey(const input::KeyEvent& e);
    void HandleArrow(graph::GraphDir dir, bool ctrl);
    void HandleTab(bool back);
    void HandleHomeEnd(bool home);
    void HandleEnter(bool shift, bool ctrl);
    void HandleTooltip();
    void HandleContextMenu();
    /// Leaves a grid slider's adjust mode (NodeVtable::AdjustOnEnter), saying its value.
    void StopAdjusting();

    /// Speak a completed move (path-diffed) and update the differ memory.
    void AnnounceMove(const graph::MoveResult& result);
    void AnnounceTree(const graph::KeyGraph::TreeResult& result);

    /// §7.4: after an activation/adjust, speak the focused node's StateText interrupting and
    /// rebaseline the live watch so the same change isn't spoken twice.
    void SpeakStateFeedback();

    /// §7.2: rebuild + reconcile; if the focused identity differs from the identity last spoken,
    /// speak the path-diffed line (queued) and update the memory.
    void RunFrameDiffer();

    /// §7.5: while a node stays focused, watch its Live parts; on change speak just that part.
    void RunLiveWatch(bool focusChangedThisFrame);

    /// Notify the screen once per focused-identity change so it can drive the game's own focus.
    void DriveHostCursor();

    /// Publish the claim set: the keys the layer owns while attached with a live render.
    void UpdateClaims(bool haveRender);

    void Speak(const std::string& text, bool interrupt = false);
    void Speak(std::string_view text, bool interrupt = false) { Speak(std::string(text), interrupt); }

    std::vector<std::string> ResolveLiveParts(graph::GraphNode* node) const;

    Screen* _screen = nullptr;
    std::unique_ptr<graph::KeyGraph> _graph;

    graph::ControlId _lastSpoken;              // §7.2 differ memory
    graph::ControlId _lastDriven;              // host-cursor drive memory
    graph::ControlId _liveBaselineId;          // §7.5 watch identity
    std::vector<std::string> _liveBaseline;    // §7.5 resolved Live part texts

    bool _buildFailureLogged = false;          // §7.8 log once per attach
    bool _claimsActive = false;
    bool _claimsTyping = false;                // the published set is the reduced typing one
    bool _claimsAdjusting = false;             // the published set includes Escape
    graph::ControlId _adjusting;               // the grid slider in adjust mode, if any
    bool _hasLiveRender = false;

    // Rebuild cadence: Build runs on key-op frames plus every kRerenderFrames-th idle frame, to
    // catch changes the game makes on its own. Idle frames in between serve the retained render
    // for claims and cursor identity only and never resolve its closures.
    static constexpr int kRerenderFrames = 6;
    int _framesSinceRerender = 0;
    bool _renderedThisFrame = false;
};

} // namespace fa::nav
