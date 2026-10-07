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
   ProgressBar,
};

Kind kind(const Widget* widget);

// The caption of a Frame or Window, or null for anything else.
const Widget* frameTitle(const Widget* widget);

enum class CheckState { Unchecked, Checked, Intermediate };
CheckState checkState(const Widget* toggleButton);
bool buttonToggled(const Widget* button);
// Whether a click flips the button on and off (agui::Button::isButtonToggleButton).
bool buttonIsToggle(const Widget* button);

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

// The tooltip the game shows while the mouse rests on the widget, made now as hovering makes it
// (Widget::checkCreateTooltip), so it is on screen too; null when the widget has none. `created`
// tells whether this call made it, for removeTooltip. Mutating, so main thread inside logic() only.
const Widget* showTooltip(const Widget* widget, bool& created);
// Takes down and destroys the widget's tooltip.
void removeTooltip(const Widget* widget);

// Whether the widget's class is, or derives from, the named class, e.g. "SettingsGui".
bool derivesFrom(const Widget* widget, std::string_view className);
// The same for any instantiation of a class template, e.g. "FilterSelectGui" for every
// FilterSelectGui<T>; the name is unscoped.
bool derivesFromTemplate(const Widget* widget, std::string_view templateName);

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

// What an item slot of the game's inventory windows (InventoryGuiSlot) holds: the item's name and
// count, and its quality unless that is normal; an empty slot has a count of 0 and no name. Names
// are translated, so main thread only.
struct SlotItem {
   std::string_view name;
   std::string_view quality;
   uint32_t count = 0;
};
SlotItem slotItem(const Widget* slot);

// A chest's slot limit, for any widget in the window part that sets it (an InventoryWithBarGui):
// its red X `button`, whether the player is `choosing` the first slot to lock (after a click on
// the button), and how many of its `size` slots are `unlocked`. All null and 0 outside one.
struct InventoryBar {
   const Widget* button = nullptr;
   bool choosing = false;
   unsigned unlocked = 0;
   unsigned size = 0;
};
InventoryBar inventoryBar(const Widget* widget);

// Whether an item slot (InventoryGuiSlot) of such a part is at or past the limit.
bool slotLocked(const Widget* slot);

// A recipe button (RecipeSlot) of a crafting list (CraftingGui): the recipe's translated name and
// how many the player can craft from what they carry.
struct RecipeItem {
   std::string_view name;
   uint32_t craftable = 0;
};
RecipeItem recipeItem(const Widget* craftingList, const Widget* slot);

// Any slot button (SlotButtonBase): the item slots of inventories, a machine's fluid boxes, recipe
// and filter choices.
bool isSlotButton(const Widget* widget);

// What a slot button depicts, as the game itself reads it for drawing: the prototype's translated
// name (an empty slot's expected or filtered item, or nothing), its quality unless that is normal,
// and the number drawn on it (a stack's count, a fluid's amount). Main thread only.
struct SlotButton {
   std::string_view name;
   std::string_view quality;
   double count = 0;
};
SlotButton slotButton(const Widget* slot);

// How full an agui::ProgressBar is, 0 to 1.
double progress(const Widget* bar);

// The parts of an entity's window (GameGuiWithControllerInventory: a chest, a furnace, a drill, an
// assembler, ...), or all null for any other window. `entity` is the window titled with the entity's
// name; it holds the player's inventory beside the entity's own part. `inventory` is the player's
// InventoryGui, `inventoryPanel` the panel holding it under `inventoryTitle` ("Character"), and
// `header` the title bar's search and close buttons. A crafting machine or drill has a
// `progressBar` and the productivity `bonusBar` under it, and may take `modules`; a crafting
// machine shows its `recipe` (a RecipeInfoWidget) and the tables of its `inputs` and `outputs`,
// and an assembler the `changeRecipe` button, which is in the window only when the recipe can
// change.
struct EntityWindowParts {
   const Widget* entity = nullptr;
   const Widget* header = nullptr;
   const Widget* inventoryPanel = nullptr;
   const Widget* inventoryTitle = nullptr;
   const Widget* inventory = nullptr;
   const Widget* progressBar = nullptr;
   const Widget* bonusBar = nullptr;
   const Widget* recipe = nullptr;
   const Widget* inputs = nullptr;
   const Widget* outputs = nullptr;
   const Widget* modules = nullptr;
   const Widget* changeRecipe = nullptr;
};
EntityWindowParts entityWindowParts(const Widget* window);

// The fuel part of a burner-powered entity's window (a BurnerInfo): the tables of fuel slots and
// of what the fuel leaves when burnt, and the bar of what is left of the fuel burning.
struct BurnerParts {
   const Widget* slots = nullptr;
   const Widget* burntResults = nullptr;
   const Widget* bar = nullptr;
};
BurnerParts burnerParts(const Widget* burnerInfo);

// The parts of a window with circuit and logistic network buttons in its title bar
// (GuiWithSideButtons) that is a transport belt's, a lamp's or another GenericOnOffEntityGui, or a
// splitter's (SplitterGui); all null and 0 for any other window. `titled` is the window titled with
// the entity's name, `sidePanel` the container the buttons open their panels in, and `unitNumber`
// the entity's (LuaEntity::unit_number).
struct EntityPanelParts {
   const Widget* titled = nullptr;
   const Widget* sidePanel = nullptr;
   uint64_t unitNumber = 0;
};
EntityPanelParts entityPanelParts(const Widget* window);

// The quickbar along the bottom of the screen (QuickBarGui), or null outside a game or while the
// view has none.
const Widget* quickBar();

// One bar of the quickbar: the page it shows (0 for page 1), the button showing that page's number,
// and the page's slots in order (QuickBarItemSlot buttons).
struct QuickBarRow {
   uint8_t page = 0;
   const Widget* button = nullptr;
   std::vector<const Widget*> slots;
};
// The bars on screen, the one the quickbar keys (1 to 0) use first.
std::vector<QuickBarRow> quickBarRows(const Widget* quickBar);
// While the page picker is open, the bar it chooses a page for (an index into quickBarRows);
// otherwise -1.
int quickBarPickingFor(const Widget* quickBar);
// The page picker's rows, one per page: clicking a row's button shows that page on the bar.
std::vector<QuickBarRow> quickBarPickerRows(const Widget* quickBar);

// The shortcut bar beside the quickbar (ShortcutBarGui), or null outside a game or while the view
// has none.
const Widget* shortcutBar();

// A shortcut on the bar: its button (a ShortcutButton), the shortcut's translated name, and whether
// it is one that stays on or off (the personal roboport, alt mode) rather than acting once.
struct Shortcut {
   const Widget* button = nullptr;
   std::string_view name;
   bool toggle = false;
};
// The shortcuts on the bar, a row per row of buttons as the bar lays them out, left to right. The
// empty places are left out. Names are translated, so main thread only.
std::vector<std::vector<Shortcut>> shortcutBarRows(const Widget* shortcutBar);
// The button that opens and closes the list of every shortcut, where the player chooses those on
// the bar.
const Widget* shortcutBarListButton(const Widget* shortcutBar);
// While that list is open, its checkboxes in its order, one per shortcut, named with it and checked
// while the shortcut is on the bar; otherwise empty.
std::vector<const Widget*> shortcutBarListCheckBoxes(const Widget* shortcutBar);

// The side menu at the top right (SideMenu), or null outside a game or while the view has none (a
// gamepad, or a mod hiding it).
const Widget* sideMenu();
// Its master mute button, the one button of it that opens no window.
const Widget* sideMenuMuteButton(const Widget* sideMenu);

// The crafting queue at the bottom left (CraftingQueueGui), or null outside a game or while the view
// has none (remote view, or the queue hidden by game_view_settings).
const Widget* craftingQueue();
// Its slots, the order being crafted first, one CraftingQueueSlot per order: a click cancels one,
// five or all of it, as the game's cancel craft controls bind the button and modifiers held. Past
// two rows a last button shows every order or two rows again. Empty while nothing is queued.
std::vector<const Widget*> craftingQueueSlots(const Widget* craftingQueue);

// The crafting queue in the character window's left pane (CharacterInfoGui): its "Crafting queue"
// label and the CraftingQueueTableGui holding the same slots as the bottom left's, a row per table
// row.
struct CharacterQueue {
   const Widget* label = nullptr;
   const Widget* table = nullptr;
};
CharacterQueue characterQueue(const Widget* characterInfo);

// The HUD's status, each part null while the game does not show it. Main thread only.
//
// The research box at the top right (CurrentResearchInfo, a button that opens the technology
// tree): its title (the technology with its level, or "not researching") and the label of its
// progress ("45%"), which is null while nothing is researched.
struct ResearchBox {
   const Widget* button = nullptr;
   const Widget* title = nullptr;
   const Widget* progress = nullptr;
};
ResearchBox researchBox();

// The alert buttons over the shortcut bar, one per category that has alerts, in the game's order.
// The button (an IconButtonWithNumber) opens the list of the category's alerts.
enum class AlertCategory : uint8_t { Attack, Construction, PlatformConstruction, Custom, Logistics, Trains, Pipelines };
struct AlertButton {
   const Widget* button = nullptr;
   AlertCategory category = AlertCategory::Attack;
};
std::vector<AlertButton> alertButtons();
// The number on an IconButtonWithNumber: an alert button's count of its most important alert. It
// reads 0 while the button blinks.
double iconButtonCount(const Widget* button);

// The scenario's goal at the top left (GoalDescription's label), while it shows one.
const Widget* goalLabel();

// The bars over the quickbar (ControllerProgressBar), those the game shows.
struct HudBars {
   const Widget* health = nullptr;
   const Widget* shield = nullptr;
   const Widget* vehicleHealth = nullptr;
   const Widget* vehicleShield = nullptr;
   const Widget* mining = nullptr;
};
HudBars hudBars();

// Factoriopedia (GameView::factoriopedia) while it shows, else a null window. Main thread only.
struct Factoriopedia {
   const Widget* window = nullptr;
   const Widget* header = nullptr;           // the title bar's buttons
   const Widget* list = nullptr;             // the entries: group tabs over a grid of entry buttons
   const Widget* subheader = nullptr;        // the frame over the page, holding the entry's title label
   const Widget* page = nullptr;             // the scroll pane of the entry's description and sections
   const Widget* showUnresearched = nullptr; // the title bar's toggle
   bool pinned = false;                      // kept open while playing, without the modal focus
};
Factoriopedia factoriopedia();

// The icons a LabelWithHoverableRichText (a description's lines) lets the mouse hover and click,
// by the index of their section in the label's text, with the tag each shows ("item=iron-plate").
// Empty for any other label.
struct RichTextLink {
   size_t section = 0;
   std::string_view tag;
};
std::vector<RichTextLink> richTextLinks(const Widget* label);
// Mutating, so main thread inside logic() only. A click on the icon, as the label runs it for the
// section under the mouse: opens its Factoriopedia entry, or its technology.
void clickRichTextLink(const Widget* label, size_t section);
// The tooltip hovering the icon shows, or null. It stays up until clearRichTextHover.
const Widget* hoverRichTextLink(const Widget* label, size_t section);
void clearRichTextHover(const Widget* label);

// The selected row of an agui::TableWithSelection, the header row counting as 0.
unsigned selectedRow(const Widget* table);

// Calls into the game. Mutating, so main thread inside logic() only.
bool isFocusable(const Widget* widget);
// Gives the widget the game's keyboard focus, as Tab would.
void focus(const Widget* widget);
// Greys the widget out or back in (Widget::setEnabled).
void setEnabled(const Widget* widget, bool enabled);
// Replaces the widget's tooltip with plain text, as the game's own Widget::setToolTip does.
void setToolTip(const Widget* widget, const std::string& text);
// Sets the caption of an agui::Label.
void setLabelText(const Widget* label, const std::string& text);
// Scrolls the nearest scroll pane holding the widget so that it is in view.
void scrollIntoView(const Widget* widget);

// Presses and releases a mouse button on the widget exactly as the Gui does for a real click that
// hit it, at its centre: mouse enter, mouse down, click, mouse up, mouse leave, each through the
// widget's own dispatcher, so its handlers, listeners, click sound and pressed look all run as in
// vanilla. Tabs select and dropdowns open on the press, toggles flip on the release. The OS mouse
// is never moved. (A Button's own Enter key confirms the window instead, hence clicks.)
enum class MouseButton { Left, Right, Middle };
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
