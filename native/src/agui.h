#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Views of Factorio's agui widgets and the few calls we make into them. Only use these on the
// game's main thread from inside the owning Gui's logic(), while the widget tree is not being
// changed.
namespace fa::agui {

struct Gui;    // agui::Gui
struct Widget; // agui::Widget or any subclass

const Gui* instance(); // agui::Gui::instance
// The application's Gui, which holds the main menu and its screens; null before it exists.
const Gui* applicationGui();
// Whether a game (or the map editor) is loaded, as opposed to the pre-game menus.
bool inGame();
// The window of the app state on top (the main menu, the game menu, Settings, Save game, ...), or
// null while the top state owns none: InGame, plain play, is one.
const Widget* menuStateWindow();

// The game's version, drawn in a corner behind the main menu (AppManager::backgroundVersionLabel),
// or null.
const Widget* versionLabel();
// The panels the main menu shows beside its window: the language selector, the background
// simulation selector and the Space Age advert, those that exist. Empty for any other window.
std::vector<const Widget*> mainMenuPanels(const Widget* mainMenu);

// The scenario's message dialog shown over the loaded game (LuaGameScript show_message_dialog, the
// freeplay welcome), or null. Its text is the label scenarioMessageLabel returns.
const Widget* scenarioMessage();
const Widget* scenarioMessageLabel(const Widget* message);
// Closes the scenario message dialog as the game's Confirm message control does; the next queued
// one, if any, takes its place. Main thread only.
void confirmScenarioMessage();

const Widget* baseWidget(const Gui* gui);
const Widget* focusedWidget(const Gui* gui);
const Widget* widgetUnderMouse(const Gui* gui);
// The modal widget with the highest priority (an open dialog, a dropped-down list), or null.
const Widget* topModal(const Gui* gui);

const Widget* parent(const Widget* widget);
std::span<const Widget* const> children(const Widget* widget);
// Children a widget manages itself rather than through add(), e.g. a Frame's content layout.
std::span<const Widget* const> privateChildren(const Widget* widget);
// Widget::text, or for anything derived from agui::Label its caption.
std::string_view text(const Widget* widget);
bool visible(const Widget* widget);
bool enabled(const Widget* widget);

// Fully qualified C++ class from RTTI, e.g. "agui::TextButton" or "MainMenuGui".
const std::string& className(const Widget* widget);

// What a widget is, from the agui classes in its RTTI hierarchy; game subclasses map to their
// agui base.
enum class Kind {
   Container, // anything without a kind of its own: flows, frames, game panels
   Ignored,   // chrome with nothing to say: scroll bars, fillers, tooltips
   Window,
   ScrollPane,
   Table,
   HorizontalFlow,
   TabbedPane,
   Label,
   Button,
   CheckBox,
   RadioButton,
   DropDown,
   Slider,
   Switch,
   TextBox,
   Tab,
   ListBox,
};

Kind kind(const Widget* widget);

// The caption of a Frame or Window, or null for anything else.
const Widget* frameTitle(const Widget* widget);

enum class CheckState { Unchecked, Checked, Intermediate };
CheckState checkState(const Widget* toggleButton);
bool buttonToggled(const Widget* button);

struct SliderValue {
   double value;
   double min;
   double max;
   double step;
};
SliderValue sliderValue(const Widget* slider);

enum class SwitchState { Left, Right, None };
SwitchState switchState(const Widget* widget);

bool readOnly(const Widget* textBox);
std::string_view textBoxText(const Widget* textBox);

bool tabSelected(const Widget* tab);

// Cells per row of an agui::Table; its children fill the rows in order.
unsigned tableColumns(const Widget* table);

// The title and text of a widget's plain tooltip, already translated; empty when it has none or
// its tooltip is built some other way.
struct ToolTip {
   std::string_view title;
   std::string_view text;
};
ToolTip toolTip(const Widget* widget);

// Whether the widget's class is, or derives from, the named class, e.g. "SettingsGui".
bool derivesFrom(const Widget* widget, std::string_view className);

// A widget the game keeps as a member at `offset` inside `owner`, by value or (for `pointer`)
// through a pointer; offsets come from game::layout.
const Widget* member(const Widget* owner, uint32_t offset);
const Widget* pointerMember(const Widget* owner, uint32_t offset);

// The footer of a Dialog window (back, confirm and the like), or null for other windows.
const Widget* dialogButtons(const Widget* window);

// The parts of a MenuGui window (the main menu, single player, the game menu, ...), or all null
// for any other window: the highlighted buttons on top (Continue), the main buttons, the bottom
// row (Exit, Back).
struct MenuParts {
   const Widget* top = nullptr;
   const Widget* main = nullptr;
   const Widget* bottom = nullptr;
};
MenuParts menuParts(const Widget* window);

// The item buttons of an agui::ListBox, in order.
std::vector<const Widget*> listBoxItems(const Widget* listBox);

// What an IconButton depicts: the translated name of the prototype its sprite belongs to (a planet,
// an item), as the game itself would write it; empty for anything else or an ownerless sprite.
// Translates through the game's locale, so main thread only.
std::string_view iconName(const Widget* widget);

// The selected row of an agui::TableWithSelection, the header row counting as 0.
unsigned selectedRow(const Widget* table);

// Calls into the game. Mutating, so main thread inside logic() only.
bool isFocusable(const Widget* widget);
// Gives the widget the game's keyboard focus, as Tab would.
void focus(const Widget* widget);
// Scrolls the nearest scroll pane holding the widget so that it is in view.
void scrollIntoView(const Widget* widget);

// Presses and releases a mouse button on the widget exactly as the Gui does for a real click that
// hit it, at its centre: mouse enter, mouse down, click, mouse up, mouse leave, each through the
// widget's own dispatcher, so its handlers, listeners, click sound and pressed look all run as in
// vanilla. Tabs select and dropdowns open on the press, toggles flip on the release. The OS mouse
// is never moved. (A Button's own Enter key confirms the window instead, hence clicks.)
enum class MouseButton { Left, Right };
void press(const Widget* widget, MouseButton button, bool shift, bool control);
// The same, but at the centre of `over`, a widget inside `widget`: for widgets that act on where
// they were clicked, such as a table selecting the row under the mouse.
void pressOver(const Widget* widget, const Widget* over, MouseButton button, bool shift, bool control);

// The DropDown whose list is open as the top modal, or null.
const Widget* openDropDown(const Gui* gui);
// A DropDown's own list and the index of its current value.
const Widget* dropDownList(const Widget* dropDown);
int dropDownSelected(const Widget* dropDown);

// Keys we hand to widgets, as agui::KeyEnum / agui::ExtendedKeyEnum values.
enum class Key { Enter, Space, Up, Down, Left, Right };
// Delivers the key to the widget's own keyDown, and keyUp too when `release`; the widget reacts
// exactly as if it had keyboard focus. Returns whether keyDown handled it.
bool sendKey(const Widget* widget, Key key, bool release);

} // namespace fa::agui
