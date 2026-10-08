#include "BlueprintSetupScreen.hpp"

#include <format>
#include <string>
#include <vector>

#include "AguiNodes.hpp"
#include "game.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Kind;
using agui::Widget;
using game::layout;

// The caption of the bordered frame a widget sits in ("Icon", "Components"): the frame's first
// label.
const Widget* CaptionOf(const Widget* widget)
{
    const Widget* frame = agui::parent(widget);
    while (frame && !agui::derivesFrom(frame, "agui::Frame"))
        frame = agui::parent(frame);
    return frame ? FindDescendant(frame, "agui::Label") : nullptr;
}

std::string CaptionText(const Widget* widget)
{
    const Widget* caption = CaptionOf(widget);
    return caption ? LabelText(caption) : std::string();
}

// The name, then every button for the blueprint in the order the game shows them, in the window's
// title context so that the title is said as the window opens. While renaming, the game's text
// field takes the name's place: under its own key, so that the swap is announced either way, and
// the same reference, so that the cursor follows it.
void AddBlueprint(graph::GraphBuilder& builder, const Widget* window, const Widget* settings)
{
    const Widget* name = agui::member(settings, layout.blueprintName);
    const Widget* label = agui::member(name, layout.editableLabelText);
    const Widget* field = agui::pointerMember(name, layout.editableLabelField);
    const Widget* pencil = agui::member(name, layout.editableLabelButton);
    const Widget* title = agui::frameTitle(window);
    std::string titleText = title && Shows(title) ? LabelText(title) : std::string();
    builder.BeginStop("blueprint");
    if (!titleText.empty())
        builder.PushContext(titleText);
    builder.StartRow();
    if (field && Shows(field))
        builder.AddItem(graph::ControlId::Referenced(name, "blueprint/name/edit"),
            ControlNode(field, []() { return std::string(); }));
    else if (Shows(pencil))
        builder.AddItem(graph::ControlId::Referenced(name, "blueprint/name"), ControlNode(pencil, label));
    else if (Shows(label))
        builder.AddItem(graph::ControlId::Referenced(name, "blueprint/name"),
            TextNode(label, [label]() { return LabelText(label); }));
    int index = 0;
    for (const Widget* button : FindAll(agui::member(settings, layout.frameSubheader), "agui::Button"))
        if (!Contains(name, button))
            AddControl(builder, "blueprint/" + std::to_string(index++), button);
    builder.EndRow();
    if (!titleText.empty())
        builder.PopContext();
}

// The four icon slots as a row in their frame's context, each by its number; then the description,
// and the button in it that puts an icon in its text.
void AddIcons(graph::GraphBuilder& builder, const Widget* settings)
{
    builder.BeginStop("icons");
    const Widget* description = agui::member(settings, layout.blueprintDescription);
    std::vector<const Widget*> icons;
    for (const Widget* button : FindAll(agui::member(settings, layout.blueprintScroll), "ChooseButtonBase"))
        if (!Contains(description, button))
            icons.push_back(button);
    if (!icons.empty())
    {
        builder.PushContext(CaptionText(icons.front()), "", /*positions*/ false);
        builder.StartRow("icons");
        for (std::size_t i = 0; i < icons.size(); ++i)
        {
            graph::NodeVtable node = ControlNode(icons[i], [i]() { return std::to_string(i + 1); });
            // The number already says which icon it is.
            node.SpeaksOwnPosition = true;
            builder.AddItem(graph::ControlId::Referenced(icons[i], std::format("icons/{}", i)), std::move(node));
        }
        builder.EndRow();
        builder.PopContext();
    }
    if (Shows(description))
    {
        const Widget* caption = CaptionOf(description);
        builder.AddItem(graph::ControlId::Referenced(description, "icons/description"),
            caption ? ControlNode(description, caption) : ControlNode(description));
        // Below the field rather than beside it: Left and Right move the caret while typing.
        int index = 0;
        for (const Widget* button : FindAll(description, "ChooseButtonBase"))
            AddControl(builder, "icons/description/" + std::to_string(index++), button,
                []() { return std::string(vocab::kInsertIcon); });
    }
}

// What a cell of the snapping table shows: a label (some wrapped with an info icon beside them),
// a field or a radio button.
const Widget* CellLeaf(const Widget* cell)
{
    Kind kind = agui::kind(cell);
    if (kind == Kind::Label || kind == Kind::TextBox || kind == Kind::RadioButton)
        return cell;
    return FindDescendant(cell, "agui::Label");
}

// The checkbox, then the snapping table top to bottom, each line in the context of what opens it: a
// label ("Grid size") or a radio button ("Absolute"), whose X and Y are its own. Each field is named
// by the label before it ("Width:"). Not rows: Left and Right move the caret in a field, so only Up
// and Down can leave one.
void AddSnapping(graph::GraphBuilder& builder, const Widget* settings)
{
    builder.BeginStop("snap");
    if (const Widget* checkbox = agui::member(settings, layout.blueprintSnapCheckbox); Shows(checkbox))
    {
        graph::NodeVtable node = ControlNode(checkbox);
        node.SpeaksOwnPosition = true;
        builder.AddItem(graph::ControlId::Referenced(checkbox, "snap/checkbox"), std::move(node));
    }
    const Widget* table = agui::parent(agui::member(settings, layout.blueprintGridWidth));
    const unsigned columns = agui::tableColumns(table);
    auto cells = agui::children(table);
    for (std::size_t start = 0; columns > 0 && start < cells.size(); start += columns)
    {
        std::vector<const Widget*> leaves;
        for (std::size_t i = start; i < cells.size() && i < start + columns; ++i)
            if (agui::visible(cells[i]))
                if (const Widget* leaf = CellLeaf(cells[i]))
                    leaves.push_back(leaf);
        if (leaves.empty())
            continue;
        const std::size_t row = start / columns;
        const Widget* lead = leaves.front();
        std::string rowName = agui::kind(lead) == Kind::Label ? LabelText(lead) : NameOf(lead);
        const bool named = !rowName.empty();
        if (named)
            builder.PushContext(rowName, "", /*positions*/ false);
        for (std::size_t i = 0; i < leaves.size(); ++i)
        {
            const Widget* leaf = leaves[i];
            const bool isLabel = agui::kind(leaf) == Kind::Label;
            // A label before a field is that field's name.
            if (isLabel && i + 1 < leaves.size() && agui::kind(leaves[i + 1]) == Kind::TextBox)
                continue;
            graph::NodeVtable node = isLabel ? TextNode(leaf, [leaf]() { return LabelText(leaf); })
                : i > 0 && agui::kind(leaves[i - 1]) == Kind::Label ? ControlNode(leaf, leaves[i - 1])
                                                                     : ControlNode(leaf);
            // The line's name and each field's label already say where the cursor is.
            node.SpeaksOwnPosition = true;
            builder.AddItem(graph::ControlId::Referenced(leaf, std::format("snap/{}/{}", row, i)), std::move(node));
        }
        if (named)
            builder.PopContext();
    }
}

// A component as the game shows it, and as taken out of the blueprint where the game shows the slot
// red with a count of 0.
std::string ComponentText(const Widget* slot)
{
    agui::SlotButton shown = agui::slotButton(slot);
    if (shown.count != 0 || shown.name.empty())
        return SlotText(slot);
    std::string name = shown.quality.empty() ? std::string(shown.name) : std::format("{} {}", shown.quality, shown.name);
    return std::format("{} 0, {}", name, vocab::kRemoved);
}

// The components ten to a row, in their frame's context. Enter puts a kind back and ] takes it out,
// as the left and right mouse buttons do. The game remakes and re-sorts the slots on every change,
// so each is keyed by its item: the cursor stays on the kind it changed.
void AddComponents(graph::GraphBuilder& builder, const Widget* settings)
{
    const Widget* table = agui::member(settings, layout.blueprintComponents);
    // A blueprint in the library's preview has no components frame.
    if (!agui::parent(table) || !Shows(table))
        return;
    builder.BeginStop("components");
    builder.PushContext(CaptionText(table));
    const unsigned columns = agui::tableColumns(table);
    std::vector<const Widget*> slots;
    for (const Widget* cell : agui::children(table))
        if (agui::visible(cell) && agui::isSlotButton(cell))
            slots.push_back(cell);
    for (std::size_t i = 0; i < slots.size(); ++i)
    {
        if (columns > 0 && i % columns == 0)
        {
            if (i > 0)
                builder.EndRow();
            builder.StartRow("components");
        }
        const Widget* slot = slots[i];
        agui::SlotButton shown = agui::slotButton(slot);
        graph::NodeVtable node = ControlNode(slot);
        for (graph::NodeAnnouncement& part : node.Announcements)
            if (part.Kind == graph::AnnouncementKinds::Label)
                part.Text = [slot]() { return ComponentText(slot); };
        // A click changes the window at once, not through an input action, so Enter can say it.
        node.StateText = [slot]() { return ComponentText(slot); };
        builder.AddItem(graph::ControlId::Referenced(slot, std::format("components/{}/{}", shown.quality, shown.name)),
            std::move(node));
    }
    if (!slots.empty() && columns > 0)
        builder.EndRow();
    builder.PopContext();
}

// The include checkboxes the game added to its filters frame, in its order.
void AddInclude(graph::GraphBuilder& builder, const Widget* settings)
{
    const Widget* first = nullptr;
    for (uint32_t offset : layout.blueprintInclude)
        if (const Widget* checkbox = agui::member(settings, offset); agui::parent(checkbox) && Shows(checkbox))
        {
            first = checkbox;
            break;
        }
    if (!first)
        return;
    const Widget* frame = agui::parent(first);
    builder.BeginStop("include");
    builder.PushContext(CaptionText(first));
    int index = 0;
    for (const Widget* checkbox : FindAll(frame, "agui::CheckBox"))
        AddControl(builder, "include/" + std::to_string(index++), checkbox);
    builder.PopContext();
}

// The preview's header as the context of the game's hint beside it.
void AddPreview(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* frame = agui::member(window, layout.blueprintPreview);
    while (frame && !agui::derivesFrom(frame, "FrameWithSubheader"))
        frame = agui::parent(frame);
    if (!frame)
        return;
    std::vector<const Widget*> labels = FindAll(agui::member(frame, layout.frameSubheader), "agui::Label");
    if (labels.empty())
        return;
    builder.BeginStop("preview");
    builder.PushContext(LabelText(labels.front()));
    for (std::size_t i = 1; i < labels.size(); ++i)
    {
        const Widget* label = labels[i];
        builder.AddItem(graph::ControlId::Referenced(label, std::format("preview/{}", i)),
            TextNode(label, [label]() { return LabelText(label); }));
    }
    builder.PopContext();
}

} // namespace

bool BlueprintSetupScreen::Handles(const Widget* window) const
{
    return agui::derivesFrom(window, "BlueprintSetupGui");
}

void BlueprintSetupScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* settings = agui::member(window, layout.blueprintSettings);
    AddBlueprint(builder, window, settings);
    AddIcons(builder, settings);
    AddSnapping(builder, settings);
    AddComponents(builder, settings);
    AddInclude(builder, settings);
    AddPreview(builder, window);

    const Widget* footer = agui::dialogButtons(window);
    if (footer && Shows(footer) && HasContent(footer))
    {
        builder.BeginStop("buttons");
        AddSubtree(builder, "buttons", footer);
    }
}

} // namespace fa::screens
