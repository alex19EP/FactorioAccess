#include "Navigator.hpp"

#include <exception>
#include <format>

#include "graph/GraphAnnouncer.hpp"
#include "log.h"
#include "speech.h"
#include "vocab.h"

namespace fa::nav
{

void Navigator::Attach(Screen* screen, graph::GraphState* state)
{
    _screen = screen;
    _graph = std::make_unique<graph::KeyGraph>(
        [this, state]() -> std::unique_ptr<graph::GraphRender>
        {
            // §7.8 build isolation: a throw must not escape the frame tick — render nothing,
            // log once per attach; focus state survives to reconcile on the next good render.
            try
            {
                graph::GraphBuilder builder(&state->Expanded);
                _screen->Build(builder);
                // Asked AFTER Build, because only building the new content tells a screen that its
                // content was replaced wholesale. `Reconcile` runs straight after this callback
                // returns and consumes the request there (KeyGraph::Rerender).
                if (const char* landing = _screen->TakeSuggestedLanding())
                    state->NextSuggestedMove = graph::ControlId::Structural(landing);
                return builder.Build();
            }
            catch (const std::exception& e)
            {
                if (!_buildFailureLogged)
                {
                    _buildFailureLogged = true;
                    log::error("Screen build failed: {}: {}", _screen->DiagName(), e.what());
                }
            }
            return nullptr;
        },
        state);

    _lastSpoken = {};
    _lastDriven = {};
    _liveBaselineId = {};
    _liveBaseline.clear();
    _adjusting = {};
    _buildFailureLogged = false;
    _framesSinceRerender = kRerenderFrames; // expired: the first differ frame renders immediately
    _renderedThisFrame = false;

    log::info("Navigator attached to {}", _screen->DiagName());
    // §9: the screen name speaks first (queued); the frame differ then announces the landing.
    Speak(std::string(_screen->Name()));
}

void Navigator::Detach()
{
    if (_screen)
        log::info("Navigator detached from {}", _screen->DiagName());
    _screen = nullptr;
    _graph.reset();
    _hasLiveRender = false;
    _framesSinceRerender = 0;
    _renderedThisFrame = false;
    _lastSpoken = {};
    _lastDriven = {};
    _liveBaselineId = {};
    _liveBaseline.clear();
    _adjusting = {};
    if (_claimsActive)
    {
        input::clearClaims();
        _claimsActive = false;
    }
}

void Navigator::Update()
{
    if (!_screen)
    {
        // Unattached: discard input so the queue can't grow, and own no keys.
        input::drain();
        if (_claimsActive)
        {
            input::clearClaims();
            _claimsActive = false;
        }
        return;
    }

    _renderedThisFrame = false;

    // Keys are only ever queued while claimed, and claims follow a live render, so anything
    // drained here was taken from the game on our behalf and must be answered.
    std::vector<input::KeyEvent> events = input::drain();
    if (!events.empty() && _screen->ClaimsKeys())
    {
        // Fresh game reads BEFORE any operation runs: op closures were built from widget
        // pointers only valid in their build frame, and idle frames may have skipped rebuilding.
        if (_graph->Rerender())
        {
            _renderedThisFrame = true;
            for (const input::KeyEvent& e : events)
                HandleKey(e);
            // The frame differ and live watch below resolve this render, so it is rebuilt after
            // the keys acted: an action may have closed the window it was built from.
            _graph->Rerender();
        }
    }

    RunFrameDiffer();
    DriveHostCursor();
}

std::string Navigator::Describe()
{
    if (!_screen)
        return "screen: none\n";
    std::string out = std::format("screen: {}\n", _screen->DiagName());
    if (!_graph->Rerender())
        return out + "render: empty\n";
    graph::GraphNode* focus = _graph->CurrentNode();
    if (focus)
        out += std::format("focus: {}\nspoken: {}\n", focus->Id.StructuralKey, graph::GraphAnnouncer::ComposeFull(focus));
    if (_adjusting.IsValid())
        out += "adjusting\n";
    const std::string* stop = nullptr;
    for (const graph::GraphNode* node : _graph->Current()->Order)
    {
        if (!stop || node->StopKey != *stop)
        {
            stop = &node->StopKey;
            out += std::format("[{}]\n", *stop);
        }
        out += std::format("{} {}: {}\n", node == focus ? '>' : ' ', node->Id.StructuralKey,
            graph::GraphAnnouncer::LeafText(node));
    }
    return out;
}

void Navigator::HandleKey(const input::KeyEvent& e)
{
    if (!e.down)
        return;

    switch (e.key)
    {
    // Arrows act on autorepeat too (held-key scrolling).
    case input::keys::Up:
        HandleArrow(graph::GraphDir::Up, e.ctrl);
        break;
    case input::keys::Down:
        HandleArrow(graph::GraphDir::Down, e.ctrl);
        break;
    case input::keys::Left:
        HandleArrow(graph::GraphDir::Left, e.ctrl);
        break;
    case input::keys::Right:
        HandleArrow(graph::GraphDir::Right, e.ctrl);
        break;
    // One-shot chords act on the fresh press only.
    case input::keys::Tab:
        if (!e.repeat)
            HandleTab(e.shift);
        break;
    case input::keys::Home:
        if (!e.repeat)
            HandleHomeEnd(true);
        break;
    case input::keys::End:
        if (!e.repeat)
            HandleHomeEnd(false);
        break;
    // [ and ] are the left and right mouse buttons, as in the world (bind-mouse-keys.ps1).
    case input::keys::Return:
    case input::keys::LeftBracket:
        if (!e.repeat)
            HandleEnter(e.shift, e.ctrl);
        break;
    case input::keys::Space:
    case input::keys::F1:
        if (!e.repeat)
            HandleTooltip();
        break;
    case input::keys::Backspace:
    case input::keys::RightBracket:
        if (!e.repeat)
            HandleContextMenu();
        break;
    // \ is the middle mouse button.
    case input::keys::Backslash:
        if (!e.repeat)
            HandleMiddleClick();
        break;
    // Only claimed while adjusting; otherwise it stays the game's Back.
    case input::keys::Escape:
        if (!e.repeat && _adjusting.IsValid())
            StopAdjusting();
        break;
    default:
        break;
    }
}

void Navigator::HandleArrow(graph::GraphDir dir, bool ctrl)
{
    // Ctrl+Up/Down: region jump within the current stop.
    if (ctrl && (dir == graph::GraphDir::Up || dir == graph::GraphDir::Down))
    {
        graph::MoveResult r = _graph->MoveRegion(dir == graph::GraphDir::Down ? +1 : -1);
        if (r.Moved)
            AnnounceMove(r);
        return;
    }

    // §7.1: a focused adjustable control adjusts on Left/Right instead of navigating; one among
    // the cells of a grid row only while its adjust mode is on.
    graph::GraphNode* node = _graph->CurrentNode();
    bool gated = node && node->Vtable.AdjustOnEnter && node->Id != _adjusting;
    if ((dir == graph::GraphDir::Left || dir == graph::GraphDir::Right) && !gated)
    {
        int sign = dir == graph::GraphDir::Right ? +1 : -1;
        if (_graph->TryAdjust(sign, /*large*/ ctrl))
        {
            SpeakStateFeedback();
            return;
        }
    }

    graph::MoveResult r = _graph->Move(dir);
    if (r.Moved)
    {
        AnnounceMove(r);
        return;
    }

    // At an edge: Left/Right get tree semantics when the focused node is in a tree.
    if ((dir == graph::GraphDir::Left || dir == graph::GraphDir::Right) && r.To
        && graph::KeyGraph::InTree(r.To))
    {
        graph::KeyGraph::TreeResult tr =
            dir == graph::GraphDir::Right ? _graph->TreeRight() : _graph->TreeLeft();
        AnnounceTree(tr);
    }
}

void Navigator::HandleTab(bool back)
{
    graph::MoveResult r = _graph->MoveStop(back ? -1 : +1, /*wrap*/ true);
    if (r.Moved)
        AnnounceMove(r);
}

void Navigator::HandleHomeEnd(bool home)
{
    graph::GraphNode* node = _graph->CurrentNode();
    // Inside a tree or a flyout column, Home/End mean the first/last SIBLING at this level; the
    // plain edge walk would climb out of the column through its head.
    graph::MoveResult r =
        (node && (graph::KeyGraph::InTree(node) || graph::KeyGraph::InFlyout(node)))
        ? _graph->MoveToSiblingEdge(/*first*/ home)
        : _graph->MoveToEdge(home ? graph::GraphDir::Up : graph::GraphDir::Down);
    if (r.Moved)
        AnnounceMove(r);
}

void Navigator::HandleEnter(bool shift, bool ctrl)
{
    graph::GraphNode* node = _graph->CurrentNode();
    if (!shift && !ctrl && node && node->Vtable.AdjustOnEnter && node->Vtable.OnAdjust)
    {
        if (node->Id == _adjusting)
            StopAdjusting();
        else
        {
            _adjusting = node->Id;
            Speak(vocab::kAdjusting, kInterruptOnKeypress);
        }
        return;
    }

    bool ok = shift ? _graph->ActivateShift()
        : ctrl      ? _graph->ActivateCtrl()
                    : _graph->Activate();
    // A refusal is silent: the game's own voice or nothing.
    if (ok)
        SpeakStateFeedback();
}

void Navigator::StopAdjusting()
{
    _adjusting = {};
    // The value it was left at.
    SpeakStateFeedback();
}

void Navigator::HandleTooltip()
{
    if (!_graph->Tooltip())
        Speak(vocab::kNoTooltip, kInterruptOnKeypress);
}

// Backspace is ONE concept with two shapes — the right-click equivalent. A node owning a flyout
// column opens it (and closes it again from inside, so the key toggles); anything else runs its
// secondary action. Escape is never claimed: the game's own Back handling stays live (§7.3).
void Navigator::HandleContextMenu()
{
    graph::MoveResult leave = _graph->LeaveFlyout();
    if (leave.Moved)
    {
        AnnounceMove(leave);
        return;
    }

    graph::MoveResult enter = _graph->EnterFlyout();
    if (enter.Moved)
    {
        AnnounceMove(enter);
        return;
    }

    if (!_graph->Secondary())
        Speak(vocab::kNoAction, kInterruptOnKeypress);
}

void Navigator::HandleMiddleClick()
{
    if (!_graph->Tertiary())
        Speak(vocab::kNoAction, kInterruptOnKeypress);
}

void Navigator::AnnounceMove(const graph::MoveResult& result)
{
    if (!result.To)
        return;
    std::string line = graph::GraphAnnouncer::Compose(result.From, result.To, result.TransitionLabel);
    if (!line.empty())
        Speak(line, kInterruptOnKeypress);
    _lastSpoken = result.To->Id; // the differ stays silent about this landing
}

void Navigator::AnnounceTree(const graph::KeyGraph::TreeResult& result)
{
    using TreeMove = graph::KeyGraph::TreeMove;
    switch (result.Kind)
    {
    case TreeMove::Expanded:
    case TreeMove::Collapsed:
    {
        // Focus stayed on the header — speak its new state word.
        graph::GraphNode* node = _graph->CurrentNode();
        if (!node)
            break;
        std::string state = graph::GraphAnnouncer::ExpandedStateText
            ? graph::GraphAnnouncer::ExpandedStateText(node->Expanded)
            : std::string();
        Speak(state.empty() ? graph::GraphAnnouncer::LeafText(node) : state, kInterruptOnKeypress);
        _lastSpoken = node->Id;
        break;
    }
    case TreeMove::Descended:
    case TreeMove::Ascended:
        AnnounceMove(result.Move);
        break;
    case TreeMove::EmptyGroup:
    case TreeMove::Leaf:
    case TreeMove::None:
        break;
    }
}

void Navigator::SpeakStateFeedback()
{
    // The action may have changed or freed what the last render read; nothing of it is resolved
    // again. An empty render means the screen went away with the action.
    if (!_graph->Rerender())
        return;
    graph::GraphNode* node = _graph->CurrentNode();
    if (!node)
        return;
    if (node->Vtable.StateText)
    {
        std::string state = node->Vtable.StateText();
        if (!state.empty())
            Speak(state, /*interrupt*/ true);
    }
    // Rebaseline the live watch so the same change isn't spoken twice (§7.4).
    _liveBaselineId = node->Id;
    _liveBaseline = ResolveLiveParts(node);
}

void Navigator::RunFrameDiffer()
{
    // Rebuild cadence (see the header): op frames rendered already; idle frames rebuild only
    // every kRerenderFrames-th frame to catch game-driven changes. Skipped frames serve the
    // retained render for claims/cursor identity and MUST NOT announce or live-watch — those
    // paths resolve build closures, which capture frame-scoped widget pointers.
    if (!_renderedThisFrame && ++_framesSinceRerender >= kRerenderFrames)
    {
        _graph->Rerender();
        _renderedThisFrame = true;
    }
    if (_renderedThisFrame)
        _framesSinceRerender = 0;

    bool haveRender = _graph->Current() != nullptr;
    _hasLiveRender = haveRender;
    UpdateClaims(haveRender);
    if (!haveRender)
        return; // closed/empty: keep state, stay silent
    if (!_renderedThisFrame)
        return; // stale idle frame: claims/cursor served, nothing new to diff

    graph::GraphNode* node = _graph->CurrentNode();
    if (!node)
        return;

    bool focusChanged = !(node->Id == _lastSpoken);
    if (focusChanged)
    {
        graph::GraphNode* from = _lastSpoken.IsValid() ? _graph->Current()->NodeAt(_lastSpoken) : nullptr;
        std::string line = graph::GraphAnnouncer::Compose(from, node);
        if (!line.empty())
            Speak(line); // differ landings queue (they follow whatever caused them)
        _lastSpoken = node->Id;
    }

    RunLiveWatch(focusChanged);
}

void Navigator::RunLiveWatch(bool focusChangedThisFrame)
{
    graph::GraphNode* node = _graph->CurrentNode();
    if (!node)
        return;

    if (focusChangedThisFrame || !(node->Id == _liveBaselineId))
    {
        // New identity: rebaseline silently — the focus announcement spoke the initial state.
        _liveBaselineId = node->Id;
        _liveBaseline = ResolveLiveParts(node);
        return;
    }

    std::vector<std::string> parts = ResolveLiveParts(node);
    if (parts.size() == _liveBaseline.size())
    {
        for (std::size_t i = 0; i < parts.size(); i++)
            if (parts[i] != _liveBaseline[i] && !parts[i].empty())
                Speak(parts[i]); // just the changed part, queued
    }
    _liveBaseline = std::move(parts);
}

void Navigator::DriveHostCursor()
{
    if (!_hasLiveRender || !_screen || !_renderedThisFrame)
        return;
    graph::GraphNode* node = _graph->CurrentNode();
    if (!node || node->Id == _lastDriven)
        return;
    _lastDriven = node->Id;
    try
    {
        _screen->OnCursorMoved(*node);
    }
    catch (...)
    {
    }
}

void Navigator::UpdateClaims(bool haveRender)
{
    bool want = haveRender && _screen != nullptr && _screen->ClaimsKeys();
    if (!want)
    {
        if (_claimsActive)
        {
            input::clearClaims();
            _claimsActive = false;
        }
        return;
    }

    graph::GraphNode* node = _graph->CurrentNode();
    // Moving off the slider being adjusted ends its adjust mode.
    if (_adjusting.IsValid() && (!node || node->Id != _adjusting))
        _adjusting = {};
    bool typing = node && _screen->TypingIn(*node);
    bool adjusting = _adjusting.IsValid();
    if (_claimsActive && typing == _claimsTyping && adjusting == _claimsAdjusting)
    {
        input::keepAlive();
        return;
    }

    using namespace input;
    constexpr uint8_t plain = mods::None;
    constexpr uint8_t shiftable = mods::None | mods::Shift;
    std::vector<Claim> claims;
    if (typing)
    {
        // The field edits with everything else; these are the ways out of it.
        claims = {{keys::Tab, shiftable}, {keys::Up, plain}, {keys::Down, plain}};
    }
    else
    {
        // Escape is deliberately absent: the game's own Back handling stays live (§7.3). Alt
        // chords never match, so Alt+F4 and friends always reach the game.
        claims = {{keys::Up, plain | mods::Ctrl}, {keys::Down, plain | mods::Ctrl},
            {keys::Left, plain | mods::Ctrl}, {keys::Right, plain | mods::Ctrl}, {keys::Tab, shiftable},
            {keys::Home, plain}, {keys::End, plain}, {keys::Return, plain | mods::Shift | mods::Ctrl},
            {keys::KeypadEnter, plain | mods::Shift | mods::Ctrl}, {keys::Space, plain}, {keys::F1, plain},
            {keys::LeftBracket, plain | mods::Shift | mods::Ctrl},
            // A held Shift or Control reaches the game's own handling of the right click.
            {keys::Backspace, plain | mods::Shift | mods::Ctrl},
            {keys::RightBracket, plain | mods::Shift | mods::Ctrl},
            {keys::Backslash, plain | mods::Shift | mods::Ctrl}};
        // Leaving adjust mode is the one Escape the game must not see.
        if (adjusting)
            claims.push_back({keys::Escape, plain});
    }
    input::setClaims(std::move(claims));
    _claimsActive = true;
    _claimsTyping = typing;
    _claimsAdjusting = adjusting;
}

void Navigator::Speak(const std::string& text, bool interrupt)
{
    if (!text.empty())
        speech::say(text, interrupt);
}

std::vector<std::string> Navigator::ResolveLiveParts(graph::GraphNode* node) const
{
    std::vector<std::string> texts;
    for (const graph::NodeAnnouncement& a : graph::GraphAnnouncer::EffectiveAnnouncements(node))
    {
        if (!a.Live)
            continue;
        std::string t;
        if (a.Text)
        {
            try
            {
                t = a.Text();
            }
            catch (...)
            {
            }
        }
        texts.push_back(std::move(t));
    }
    return texts;
}

} // namespace fa::nav
