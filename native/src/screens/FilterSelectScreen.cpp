#include "FilterSelectScreen.hpp"

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "AguiNodes.hpp"
#include "game.h"
#include "text.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;

constexpr const char* kSearchKey = "search/field";

// The choosers shown, the topmost last: one can open another over it (Set orbital request's planet).
std::vector<const Widget*> OpenChoosers()
{
    std::vector<const Widget*> open;
    if (!agui::inGame() || agui::menuStateWindow())
        return open;
    const agui::Gui* gui = agui::applicationGui();
    const Widget* root = gui ? agui::baseWidget(gui) : nullptr;
    if (!root)
        return open;
    // Every chooser is a SelectListGui<T>; the assembler's recipe list is one too, read with its
    // machine's window.
    for (const Widget* child : agui::children(root))
        if (agui::visible(child) && agui::derivesFromTemplate(child, "SelectListGui")
            && !agui::derivesFrom(child, "AssemblingMachineSelectRecipeGui"))
            open.push_back(child);
    return open;
}

const Widget* FindWindow()
{
    std::vector<const Widget*> open = OpenChoosers();
    return open.empty() ? nullptr : open.back();
}

const Widget* SearchField(const Widget* window)
{
    const Widget* popup = FindDescendant(window, "SearchPopup");
    return popup ? FindDescendant(popup, "agui::TextField") : nullptr;
}

// What the Set orbital request window asks besides the counts, below them: a minimum payload, and
// the planet to import from with where to request from there.
struct OrbitalParts
{
    const Widget* payload = nullptr;
    const Widget* payloadField = nullptr;
    const Widget* import = nullptr;
};

// What the chooser asks besides the choice, as a stop of its own, in the order shown: the quality
// condition (a comparison and the qualities), a request's minimum and maximum (the Set request
// window), an orbital request's minimum payload and planet, and the confirm button, which shows only
// an icon.
void AddExtras(graph::GraphBuilder& builder, const Widget* window)
{
    builder.BeginStop("extras");
    std::vector<const Widget*> skip{agui::member(window, game::layout.frameHeader),
        FindDescendant(window, "SearchPopup"), FindDescendant(window, "agui::TabbedPane")};
    // The choices, a stop of their own. A chooser without groups (a planet's) shows them outside the
    // tabs.
    for (const Widget* table : FindAll(window, "agui::Table"))
        if (std::ranges::any_of(agui::children(table), [](const Widget* cell) { return agui::isSlotButton(cell); }))
            skip.push_back(table);
    std::vector<const Widget*> fields;
    OrbitalParts orbital;
    if (agui::derivesFrom(window, "ItemAndDoubleCountSelectGui"))
    {
        const Widget* counts = agui::member(window, game::layout.requestWindowCounts);
        fields = FindAll(counts, "agui::TextField");
        if (fields.size() == 2)
            skip.push_back(counts);
    }
    else if (agui::derivesFrom(window, "OrbitalRequestSelectGui"))
    {
        // The fields stand for their sliders, as in the Set request window.
        fields = {agui::member(window, game::layout.orbitalWindowMinimum),
            agui::member(window, game::layout.orbitalWindowMaximum)};
        orbital = {agui::member(window, game::layout.orbitalWindowPayload),
            agui::member(window, game::layout.orbitalWindowPayloadField),
            agui::member(window, game::layout.orbitalWindowImport)};
        skip.insert(
            skip.end(), {agui::parent(fields[0]), orbital.payload, agui::parent(orbital.payloadField), orbital.import});
    }
    // The icon buttons left, but for the qualities: the confirm button, last on screen. Named by its
    // tooltip once a choice is made.
    std::vector<const Widget*> confirms;
    for (const Widget* button : FindAll(window, "IconButton"))
        if (std::ranges::none_of(skip, [button](const Widget* part) { return Contains(part, button); })
            && !agui::derivesFrom(agui::parent(button), "QualitySelector"))
            confirms.push_back(button);
    std::ranges::copy(confirms, std::back_inserter(skip));

    AddSubtree(builder, "extras", window, std::move(skip));
    // One under the other: Left and Right move a field's caret.
    if (fields.size() == 2)
    {
        AddControl(builder, "extras/minimum", fields[0], []() { return std::string(vocab::kMinimum); });
        AddControl(builder, "extras/maximum", fields[1], []() { return std::string(vocab::kMaximum); });
    }
    if (orbital.payload)
    {
        // The payload's field is named by the checkbox above it, which unlocks it.
        const Widget* payload = orbital.payload;
        AddControl(builder, "extras/payload", payload);
        AddControl(builder, "extras/payload/field", orbital.payloadField, [payload]() { return NameOf(payload); });
        // The planet's button, named by the label beside it, which heads the radio buttons too.
        const Widget* planet = agui::member(window, game::layout.orbitalWindowImportButton);
        const Widget* label = FindDescendant(orbital.import, "agui::Label");
        builder.AddItem(graph::ControlId::Referenced(planet, "extras/import"), ControlNode(planet, label));
        AddSubtree(builder, "extras/import", orbital.import, {planet, label});
    }
    for (std::size_t i = 0; i < confirms.size(); ++i)
    {
        const Widget* confirm = confirms[i];
        AddControl(builder, "extras/confirm/" + std::to_string(i), confirm,
            [confirm]()
            {
                std::string name = NameOf(confirm);
                return name.empty() ? std::string(vocab::kConfirm) : name;
            });
    }
}

} // namespace

bool FilterSelectScreen::IsActive()
{
    std::vector<const Widget*> open = OpenChoosers();
    const Widget* window = open.empty() ? nullptr : open.back();
    if (!window || (_window && window != _window))
    {
        // Gone, or a new chooser in its place: one inactive frame pops this screen. One opened over
        // this one gives the cursor back where it was when it closes.
        if (window && std::ranges::contains(open, _window))
        {
            _under = _window;
            _underKey = _focusKey;
        }
        _window = nullptr;
        return false;
    }
    if (!_window && _under)
    {
        if (window == _under)
            _returnKey = std::move(_underKey);
        if (window == _under || !std::ranges::contains(open, _under))
            _under = nullptr;
    }
    _window = window;
    return true;
}

void FilterSelectScreen::Build(graph::GraphBuilder& builder)
{
    if (!_window || FindWindow() != _window)
        return;
    const Widget* search = SearchField(_window);
    if (search)
    {
        builder.BeginStop("search");
        builder.AddItem(graph::ControlId::Referenced(search, kSearchKey), ControlNode(search));
    }
    _landOnSearch = search && !_searching;
    _searching = search != nullptr;

    builder.BeginStop("choices");
    std::string title;
    if (const Widget* label = agui::frameTitle(_window))
        title = text::speakable(agui::text(label));
    if (!title.empty())
        builder.PushContext(title);

    AddChoices(builder, "", _window);

    if (!title.empty())
        builder.PopContext();

    AddExtras(builder, _window);
}

const char* FilterSelectScreen::TakeSuggestedLanding()
{
    bool land = _landOnSearch;
    _landOnSearch = false;
    if (!_returnKey.empty())
    {
        _landing = std::move(_returnKey);
        _returnKey.clear();
        return _landing.c_str();
    }
    return land ? kSearchKey : nullptr;
}

bool FilterSelectScreen::TypingIn(const graph::GraphNode& node) { return TypingInField(node); }

void FilterSelectScreen::OnCursorMoved(const graph::GraphNode& node)
{
    _focusKey = node.Id.StructuralKey;
    FollowCursor(node);
}

void FilterSelectScreen::OnPop()
{
    _window = nullptr;
    _searching = false;
    _landOnSearch = false;
}

} // namespace fa::screens
