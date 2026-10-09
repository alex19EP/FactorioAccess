#include "ScenarioMessageScreen.hpp"

#include <string>

#include "AguiNodes.hpp"
#include "vocab.h"

namespace fa::screens
{

bool ScenarioMessageScreen::IsActive()
{
    if (_confirm)
    {
        _confirm = false;
        _message = nullptr;
        agui::confirmScenarioMessage();
        return false;
    }
    const agui::Widget* message = agui::scenarioMessage();
    if (!message || !agui::visible(message))
    {
        _message = nullptr;
        return false;
    }
    if (_message && message != _message)
    {
        // The next queued message: one inactive frame pops this screen, so it starts fresh.
        _message = nullptr;
        return false;
    }
    _message = message;
    return true;
}

void ScenarioMessageScreen::Build(graph::GraphBuilder& builder)
{
    if (!_message)
        return;
    builder.BeginStop("message");
    AddSubtree(builder, "text", agui::scenarioMessageLabel(_message));

    graph::NodeVtable vtable;
    vtable.Announcements.emplace_back(
        []() { return std::string(vocab::kContinue); }, false, graph::AnnouncementKinds::Label);
    vtable.Announcements.emplace_back(
        []() { return std::string(vocab::kButton); }, false, graph::AnnouncementKinds::Role);
    vtable.OnActivate = [this]() { _confirm = true; };
    builder.AddItem(graph::ControlId::Structural("continue"), std::move(vtable));
}

void ScenarioMessageScreen::OnPop() { _message = nullptr; }

} // namespace fa::screens
