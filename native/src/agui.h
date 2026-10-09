#pragma once

#include <cstddef>
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

// The game's bold red and bold orange label styles, which say something the text does not: in a
// recipe tooltip, an ingredient count there is short, red when it cannot be made and orange when
// crafting makes it from intermediates.
enum class LabelTone { Plain, Red, Orange };
LabelTone labelTone(const Widget* label);

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

// The base class of any polymorphic game object (an entity as well as a widget) by its decorated
// RTTI name, e.g. ".?AVInserter@@", or null when the object's class does not derive from it.
const std::byte* objectAsBase(const void* object, std::string_view decoratedBase);

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
// are translated, so main thread only. `data` is the stack's own Item (a blueprint, a book), null
// for plain items. A slot whose item is in the player's hand shows a hand (`inHand`); a book's slot
// is highlighted while it is the book's active one (`active`).
struct SlotItem {
   std::string_view name;
   std::string_view quality;
   uint32_t count = 0;
   const void* data = nullptr;
   bool inHand = false;
   bool active = false;
};
SlotItem slotItem(const Widget* slot);

// What robots are to do with an entity's slot (an InventoryGuiSlot), as the slot draws it: the
// item requested there (`name`, its `quality` unless normal, and `count`; an empty name for none),
// which a ghost in hand puts there in remote view, and whether its stack is to be taken out
// (`removal`). Main thread only.
struct SlotRequest {
   std::string_view name;
   std::string_view quality;
   uint32_t count = 0;
   bool removal = false;
};
SlotRequest slotRequest(const Widget* slot);

// A blueprint library slot (BlueprintRecordSlotButton), as it draws itself: its `record` (a
// BlueprintRecord, null for an empty slot), whether only the record's `preview` has arrived (drawn
// grey), the player holds it (a hand over it), it is the `active` one of its book, and how far its
// transfer is (`progress`, drawn while in (0, 1)).
struct RecordSlot {
   const void* record = nullptr;
   const void* player = nullptr; // the Player the slot shows the library to
   bool preview = false;
   bool inHand = false;
   bool active = false;
   float progress = 0;
};
bool isRecordSlot(const Widget* widget);
RecordSlot recordSlot(const Widget* slot);
// The record of a BlueprintBookRecord that `player` builds from: the book's own for the owner, the
// player's choice for a book on the game's shelf or another player's.
uint16_t bookRecordActiveIndex(const void* book, const void* player);

// An achievement's card (AchievementCard) in the achievements window: the AchievementPrototype it
// shows, its state as its frame draws it, the flow of its texts (name, description, progress), a
// normal one's track button and a failed one's warning icon, whose tooltip says why it failed
// (null on the others).
enum class AchievementState { Normal, Earned, Failed };
struct AchievementCard {
   const void* prototype = nullptr;
   AchievementState state = AchievementState::Normal;
   const Widget* description = nullptr;
   const Widget* track = nullptr;
   const Widget* warning = nullptr;
};
AchievementCard achievementCard(const Widget* card);

// Whether a list of blueprints (BlueprintsList, in a book's window or the library) is in List
// view, a row per item with its name and description beside the slot, rather than Grid or Slots.
bool blueprintsListView(const Widget* list);

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
// `header` the title bar's search and close buttons. In remote view the window may hold
// `ghostChoices` (a SelectListGui, "Ghost cursor selection") there instead of the inventory; then
// `inventoryPanel` is that list. A crafting machine or drill has a
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
   const Widget* ghostChoices = nullptr;
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
// (GuiWithSideButtons) that is a transport belt's, a pipe's, a lamp's or another
// GenericOnOffEntityGui, a splitter's (SplitterGui), or a pump's, a boiler's or another
// EntityWithEnergySourceGui; all null for any other window. `titled` is the window titled with the
// entity's name, `sidePanel` the container the buttons open their panels in.
struct EntityPanelParts {
   const Widget* titled = nullptr;
   const Widget* sidePanel = nullptr;
};
EntityPanelParts entityPanelParts(const Widget* window);

// The parts of a pipe's or a storage tank's window (SingleFluidBoxEntityGui) that repeat its fluid
// line ("Water 100%"): the fluid's icon and the bar of how full the entity is. Null for any other
// window.
struct FluidBoxParts {
   const Widget* icon = nullptr;
   const Widget* bar = nullptr;
};
FluidBoxParts fluidBoxParts(const Widget* window);

// The parts of an electric network's window (ElectricNetworkGuiWindow): a pole's network, or every
// network of the surface. `bars` is the row of bars of how well the network is supplied, `flows`
// the columns below them: `consumption`, `production` and `storage` (accumulators), each with its
// graph. All null for any other window.
struct ElectricNetworkParts {
   const Widget* bars = nullptr;
   const Widget* flows = nullptr;
   const Widget* consumption = nullptr;
   const Widget* production = nullptr;
   const Widget* storage = nullptr;
   std::vector<const Widget*> graphs;
};
ElectricNetworkParts electricNetworkParts(const Widget* window);

// A game window a mod attached relative GUI elements to sits in an invisible window the game puts
// on the root in its place (CustomGuiGameGuiWrapper). The window it wraps, or null when `widget` is
// no such wrapper.
const Widget* wrappedWindow(const Widget* widget);

// The flows of such a wrapper that hold the mod's elements, in reading order: above the window,
// left of it, right of it, below it.
std::vector<const Widget*> relativeFlows(const Widget* wrapper);

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

// The alerts window an alert button opens (GameView::alertsOverview) while it shows, else a null
// window. `rows` pairs each row of its list with what stands beside it: a group of alerts with its
// pin button, or a surface's name heading the groups on it (pin null). Main thread only.
struct AlertsRow {
   const Widget* item = nullptr; // the list's TextButton: "[icon] Turret is under attack (3)", or the surface
   const Widget* pin = nullptr;  // the group's pin button, which pins it to the pins panel
};
struct AlertsWindow {
   const Widget* window = nullptr;
   AlertCategory category = AlertCategory::Attack;
   std::vector<AlertsRow> rows;
};
AlertsWindow alertsWindow();

// The map search's results in remote view (GameView::chartSearchResultGui) while they show, else a
// null window: a row per result of what the search box in remote view's title bar found, as the
// game words it ("[item=iron-ore] Iron ore 402k"), with its pin button. Clicking a row moves the
// camera to the result; the pin button pins it to the pins panel. Main thread only.
struct ChartSearchRow {
   const Widget* item = nullptr; // the list's TextButton
   const Widget* pin = nullptr;  // its pin button
};
struct ChartSearchResults {
   const Widget* window = nullptr;
   std::vector<ChartSearchRow> rows;
};
ChartSearchResults chartSearchResults();

// The map view options at the right in remote view (MapViewOptionsGui) while they show: the add
// tag and add ping buttons, and on the map the overlay toggles. Null in the character view.
const Widget* mapViewOptions();

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

// Whether a game window is open in place of the map (GameView::activeWindow): the inventory, an
// entity's window, production statistics and the like.
bool gameWindowOpen();

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

// The technology window (GameView::technologyGui, opened by T or the research box) while it shows,
// else a null window. Main thread only.
struct TechnologyWindow {
   const Widget* window = nullptr;
   const Widget* queue = nullptr;       // the research queue (ResearchQueueGui), hidden while the queue is off
   const Widget* title = nullptr;       // the selected technology's name with its level
   const Widget* status = nullptr;      // beside it, its status: "(Available)"
   const Widget* featured = nullptr;    // its cost, effects, description and Start research
   const Widget* list = nullptr;        // every technology, the grid the search filters (TechnologyListGui)
   const Widget* listTable = nullptr;   // that grid's table of TechnologySlot buttons
   const Widget* graphTitle = nullptr;  // the graph's title bar, with Back, Forward and close
   const Widget* graphHolder = nullptr; // "Show only essential technologies" over the graph
   const Widget* graph = nullptr;       // the graph (TechnologyGraphGui)
};
TechnologyWindow technologyWindow();

// The research queue's entries in its order, the research going on first: the technology's button,
// which selects it, and the button that takes it out of the queue. The empty places are left out.
struct QueueEntry {
   const Widget* slot = nullptr;
   const Widget* cancel = nullptr;
};
std::vector<QueueEntry> researchQueueEntries(const Widget* queue);

// A technology's button (TechnologySlot), in the list, the queue, the details or the graph.
bool isTechnologySlot(const Widget* widget);

// What a technology's button shows, in words: the technology's name with the level its band shows,
// and its status as the game words the selected one's ("Researched", "Available", "Queued",
// "Researching", "Unavailable", "Undiscovered"). A queued technology has its place in the queue,
// 1 for the research going on. `progress` is how much of it is researched, 0 to 1, when the button
// draws it. Translates through the game's locale, so main thread only.
struct TechnologyInfo {
   uint16_t id = 0;
   std::string name;
   std::string status;
   unsigned queuePosition = 0;
   double progress = 0;
};
TechnologyInfo technologyInfo(const Widget* slot);

// The graph of the selected technology, as the game lays it out: layers top to bottom, each
// technology's prerequisites in layers above it and what it unlocks below, and within a layer left
// to right by x. The routing vertices an edge passes through are left out; edges link the
// technologies at their ends. An omitted vertex is the button standing for `omitted` technologies
// the view leaves out; its `technology` is the one it hangs from. `central` indexes the selected
// technology, or is SIZE_MAX while the graph is empty.
struct TechnologyVertex {
   const Widget* button = nullptr; // a TechnologySlot, or for an omitted vertex a TechnologyOmittedButton
   uint16_t technology = 0;
   unsigned omitted = 0;
   unsigned layer = 0;
   int x = 0;
   std::vector<size_t> prerequisites;
   std::vector<size_t> unlocks;
};
struct TechnologyGraph {
   std::vector<TechnologyVertex> vertices;
   size_t central = SIZE_MAX;
};
TechnologyGraph technologyGraph(const Widget* graph);

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
// The same at a point of the widget, in its own coordinates: for widgets that act on the place
// clicked inside them, such as a blueprint's picture.
void pressAt(const Widget* widget, int x, int y, MouseButton button, bool shift, bool control);
// Detour for determineWidgetUnderMouse: during a press the mouse is over the pressed widget, so the
// controls its handlers ask about (craft, craft-5, craft-all) hold wherever the real mouse is. While
// the mod drives the cursor the mouse is over no GUI otherwise, so the world answers the FA cursor
// wherever the real mouse rests.
void* underMouseDetour();
void** underMouseOriginal();

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
