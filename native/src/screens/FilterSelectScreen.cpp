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

const Widget* FindWindow()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return nullptr;
    const agui::Gui* gui = agui::applicationGui();
    const Widget* root = gui ? agui::baseWidget(gui) : nullptr;
    if (!root)
        return nullptr;
    // Every chooser is a SelectListGui<T>; the assembler's recipe list is one too, read with its
    // machine's window.
    for (const Widget* child : agui::children(root))
        if (agui::visible(child) && agui::derivesFromTemplate(child, "SelectListGui")
            && !agui::derivesFrom(child, "AssemblingMachineSelectRecipeGui"))
            return child;
    return nullptr;
}

const Widget* SearchField(const Widget* window)
{
    const Widget* popup = FindDescendant(window, "SearchPopup");
    return popup ? FindDescendant(popup, "agui::TextField") : nullptr;
}

// What the chooser asks besides the choice, as a stop of its own, in the order shown: the quality
// condition (a comparison and the qualities), a request's minimum and maximum (the Set request
// window), and the confirm button, which shows only an icon.
void AddExtras(graph::GraphBuilder& builder, const Widget* window)
{
    builder.BeginStop("extras");
    std::vector<const Widget*> skip{agui::member(window, game::layout.frameHeader),
        FindDescendant(window, "SearchPopup"), FindDescendant(window, "agui::TabbedPane")};
    std::vector<const Widget*> fields;
    if (agui::derivesFrom(window, "ItemAndDoubleCountSelectGui"))
    {
        const Widget* counts = agui::member(window, game::layout.requestWindowCounts);
        fields = FindAll(counts, "agui::TextField");
        if (fields.size() == 2)
            skip.push_back(counts);
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
    const Widget* window = FindWindow();
    if (!window || (_window && window != _window))
    {
        // Gone, or a new chooser in its place: one inactive frame pops this screen.
        _window = nullptr;
        return false;
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
    return land ? kSearchKey : nullptr;
}

bool FilterSelectScreen::TypingIn(const graph::GraphNode& node) { return TypingInField(node); }

void FilterSelectScreen::OnCursorMoved(const graph::GraphNode& node) { FollowCursor(node); }

void FilterSelectScreen::OnPop()
{
    _window = nullptr;
    _searching = false;
    _landOnSearch = false;
}

} // namespace fa::screens
