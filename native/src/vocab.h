#pragma once

#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <string>

// Every word the native layer says on its own (graph-a11y-spec A8). Text read from the game is
// already in the player's language; these are keys of the mod's locale (locale/<language>/native.cfg),
// which the game translates into the player's language when they are said. Kept short and
// lowercase, after the mod's own speech style.
namespace fa::vocab {

// The most parameters a key takes: the game has a LocalisedString constructor for each count.
inline constexpr size_t kMaxParameters = 3;

// The text of a key of the game's or the mod's locale, its parameters (__1__, __2__, __3__) filled
// in, in the game's current language. Translating touches the game's locale, so this runs where
// the game itself translates (GUI and game hooks), never on a thread of our own.
std::string translate(const char* key, std::initializer_list<std::string> parameters);

// A key of the mod's locale. It turns into text where it is said, so the text follows the game's
// language; hold the Word, not its text.
struct Word {
   const char* key;

   std::string str() const { return translate(key, {}); }
   operator std::string() const { return str(); }

   // The phrase with its parameters filled in, each written as std::format writes it.
   template <class... Parameters>
   std::string operator()(const Parameters&... parameters) const {
      static_assert(sizeof...(Parameters) <= kMaxParameters);
      return translate(key, {std::format("{}", parameters)...});
   }
};

inline constexpr Word kButton{"fa.native-button"};
inline constexpr Word kCheckBox{"fa.native-checkbox"};
inline constexpr Word kRadioButton{"fa.native-radio-button"};
inline constexpr Word kDropDown{"fa.native-dropdown"};
inline constexpr Word kSlider{"fa.native-slider"};
inline constexpr Word kSwitch{"fa.native-switch"};
inline constexpr Word kEdit{"fa.native-edit"};
inline constexpr Word kTab{"fa.native-tab"};
inline constexpr Word kProgressBar{"fa.native-progress-bar"};

inline constexpr Word kChecked{"fa.native-checked"};
inline constexpr Word kUnchecked{"fa.native-unchecked"};
inline constexpr Word kPartlyChecked{"fa.native-partly-checked"};
inline constexpr Word kSelected{"fa.native-selected"};
inline constexpr Word kPressed{"fa.native-pressed"};
inline constexpr Word kNotPressed{"fa.native-not-pressed"};
inline constexpr Word kLeft{"fa.native-left"};
inline constexpr Word kRight{"fa.native-right"};
inline constexpr Word kDisabled{"fa.native-disabled"};
inline constexpr Word kReadOnly{"fa.native-read-only"};
inline constexpr Word kBlank{"fa.native-blank"};

// Where an item is in its list ("3 of 10"), a node's state, and the hint on an item that opens a
// submenu ("submenu, 4 items").
inline constexpr Word kPosition{"fa.native-position"};
inline constexpr Word kExpanded{"fa.native-expanded"};
inline constexpr Word kCollapsed{"fa.native-collapsed"};
inline constexpr Word kSubmenu{"fa.native-submenu"};

inline constexpr Word kAllMods{"fa.native-all-mods"};
inline constexpr Word kAlternative{"fa.native-alternative"};
// A column heading's button, "sort by name".
inline constexpr Word kSortBy{"fa.native-sort-by"};
inline constexpr Word kContinue{"fa.native-continue"};
inline constexpr Word kClose{"fa.native-close"};
inline constexpr Word kAdjusting{"fa.native-adjusting"};

inline constexpr Word kEmpty{"fa.native-empty"};
inline constexpr Word kProductivity{"fa.native-productivity"};

// An entity window's slots and bars, named as the mod's own entity menus name them.
inline constexpr Word kInputs{"fa.native-inputs"};
inline constexpr Word kOutputs{"fa.native-outputs"};
inline constexpr Word kFuel{"fa.native-fuel"};
inline constexpr Word kBurntResults{"fa.native-burnt-results"};
inline constexpr Word kModules{"fa.native-modules"};
inline constexpr Word kProgress{"fa.native-progress"};
inline constexpr Word kMining{"fa.native-mining"};
inline constexpr Word kBurning{"fa.native-burning"};

// A chest's slot limit: the red X button, the slots it locks, and how many are left unlocked.
inline constexpr Word kLimitSlots{"fa.native-limit-slots"};
inline constexpr Word kChooseFirstLocked{"fa.native-choose-first-locked"};
inline constexpr Word kAllUnlocked{"fa.native-all-unlocked"};
inline constexpr Word kAllLocked{"fa.native-all-locked"};
inline constexpr Word kUnlocked{"fa.native-unlocked"};
inline constexpr Word kLocked{"fa.native-locked"};
inline constexpr Word kLockFromHere{"fa.native-lock-from-here"};

// A blueprint's component the player took out of it: the game shows it red with a count of 0.
inline constexpr Word kRemoved{"fa.native-removed"};
// A slot whose item the player holds: the game draws a hand on it.
inline constexpr Word kInHand{"fa.native-in-hand"};
// A book's slot it builds from while held, highlighted in the book's window.
inline constexpr Word kActive{"fa.native-active"};
// A library record whose content has not arrived yet (drawn grey), or is arriving ("transferring 40
// percent").
inline constexpr Word kNotAvailable{"fa.native-not-available"};
inline constexpr Word kTransferring{"fa.native-transferring"};
// A deconstruction planner for trees and rocks only, which shows a tree, crossed out when it
// removes everything else.
inline constexpr Word kTreesAndRocks{"fa.native-trees-and-rocks"};
inline constexpr Word kNotTreesAndRocks{"fa.native-not-trees-and-rocks"};
// The button inside a text box that opens the chooser of an icon to put in its text; the game
// shows only an icon on it.
inline constexpr Word kInsertIcon{"fa.native-insert-icon"};

// The quickbar, its bars and the page each shows ("bar 2, page 3"), and the page picker's button
// that shows a page on a bar ("show on bar 2").
inline constexpr Word kQuickBar{"fa.native-quickbar"};
inline constexpr Word kBar{"fa.native-bar"};
inline constexpr Word kPage{"fa.native-page"};
inline constexpr Word kShowOnBar{"fa.native-show-on-bar"};
// The shortcut bar, and the button that opens the list of every shortcut when it has no name of
// its own on screen.
inline constexpr Word kShortcutBar{"fa.native-shortcut-bar"};
inline constexpr Word kAllShortcuts{"fa.native-all-shortcuts"};
inline constexpr Word kSideMenu{"fa.native-side-menu"};
inline constexpr Word kCraftingQueue{"fa.native-crafting-queue"};
// Remote view's panel of map overlay toggles and the add tag and add ping buttons.
inline constexpr Word kMapViewOptions{"fa.native-map-view-options"};
// The HUD's status: research, the alert categories, the scenario's goal and the bars.
inline constexpr Word kStatus{"fa.native-status"};
inline constexpr Word kAlerts{"fa.native-alerts"};
inline constexpr Word kGoal{"fa.native-goal"};
// AlertCategory in the game's order, from attack to pipelines.
inline constexpr Word kAlertCategories[] = {
    {"fa.native-alert-attack"},    {"fa.native-alert-construction"}, {"fa.native-alert-platform-construction"},
    {"fa.native-alert-custom"},    {"fa.native-alert-logistics"},    {"fa.native-alert-trains"},
    {"fa.native-alert-pipelines"},
};
inline constexpr Word kHealth{"fa.native-health"};
inline constexpr Word kShield{"fa.native-shield"};
inline constexpr Word kVehicleHealth{"fa.native-vehicle-health"};
inline constexpr Word kVehicleShield{"fa.native-vehicle-shield"};
// The alerts window's button beside a group of alerts, which pins the group to the pins panel.
inline constexpr Word kPin{"fa.native-pin"};
// Where leaving a part of the HUD with no window open takes the player.
inline constexpr Word kMap{"fa.native-map"};
// An icon in a text that the game lets the mouse click (a Factoriopedia description's).
inline constexpr Word kLink{"fa.native-link"};
// The technology window: a queued research's X, which takes it out of the queue, and the graph's
// button standing for the technologies the view leaves out ("12 omitted").
inline constexpr Word kCancel{"fa.native-cancel"};
inline constexpr Word kOmitted{"fa.native-omitted"};
// Its parts that have no heading on screen: the selected technology's details, and the buttons on
// the technology tree's title bar.
inline constexpr Word kSelectedTechnology{"fa.native-selected-technology"};
inline constexpr Word kTreeControls{"fa.native-tree-controls"};

// A recipe tooltip's ingredient count in red (not enough, and none to make) or in orange (not
// enough, made from intermediates when crafted).
inline constexpr Word kMissing{"fa.native-missing"};
inline constexpr Word kFromIntermediates{"fa.native-from-intermediates"};

// What the build preview's tint means where the item in hand would go, besides the game's own
// reason it cannot be built: buildable but out of reach, and an identical entity already there.
inline constexpr Word kOutOfReach{"fa.native-out-of-reach"};
inline constexpr Word kAlreadyBuilt{"fa.native-already-built"};

// What the preview highlights, each followed by the entity and where it is from the preview: the
// poles that would power it (or none), the poles it would wire to (or none), what its supply or
// logistic area would cover, the inserters, drills and machines that would put into or take from
// it, its underground partner, what it would replace, turn or otherwise change, an entity whose
// deconstruction it would cancel, the roboports it would link to, and any other highlight.
inline constexpr Word kPowerFrom{"fa.native-power-from"};
inline constexpr Word kNoPower{"fa.native-no-power"};
inline constexpr Word kWireTo{"fa.native-wire-to"};
inline constexpr Word kNoWires{"fa.native-no-wires"};
inline constexpr Word kCovers{"fa.native-covers"};
inline constexpr Word kWorksWith{"fa.native-works-with"};
// The logistic network a logistic container would join, by name or number, or none.
inline constexpr Word kInNetwork{"fa.native-in-network"};
inline constexpr Word kNoNetwork{"fa.native-no-network"};
inline constexpr Word kPairsWith{"fa.native-pairs-with"};
inline constexpr Word kReplaces{"fa.native-replaces"};
inline constexpr Word kTurns{"fa.native-turns"};
inline constexpr Word kChanges{"fa.native-changes"};
inline constexpr Word kKeeps{"fa.native-keeps"};
inline constexpr Word kLinksTo{"fa.native-links-to"};
inline constexpr Word kHighlights{"fa.native-highlights"};
// "and 3 more" after the first few of a kind, and how far and which way an entity is ("3 tiles
// North").
inline constexpr Word kAndMore{"fa.native-and-more"};
inline constexpr Word kTilesToward{"fa.native-tiles-toward"};
// The mod's own key for the sixteen directions, north 0 clockwise.
inline constexpr Word kDirection{"fa.direction"};

// A blueprint's picture, tile by tile: what the game draws on an entity. Items to be delivered to
// it ("with 2 speed module"), its filters, which way an underground belt or loader faces, a
// splitter's priorities, a combinator's operation and output, a recipe not unlocked yet.
inline constexpr Word kWith{"fa.native-with"};
inline constexpr Word kFilter{"fa.native-filter"};
inline constexpr Word kBlacklist{"fa.native-blacklist"};
inline constexpr Word kBlacklistOf{"fa.native-blacklist-of"};
inline constexpr Word kInput{"fa.native-input"};
inline constexpr Word kOutput{"fa.native-output"};
inline constexpr Word kOutputSignal{"fa.native-output-signal"};
inline constexpr Word kLeftLane{"fa.native-left-lane"};
inline constexpr Word kRightLane{"fa.native-right-lane"};
inline constexpr Word kInputPriority{"fa.native-input-priority"};
inline constexpr Word kOutputPriority{"fa.native-output-priority"};
inline constexpr Word kFilterToward{"fa.native-filter-toward"};
inline constexpr Word kLockedRecipe{"fa.native-locked-recipe"};
// ArithmeticCombinatorParameters::Operation, Comparison and SelectorCombinatorParameters::Operation
// in the game's order. A selector's Select is said with its maximum or minimum.
inline constexpr Word kArithmetic[11] = {
    {"fa.native-multiply"}, {"fa.native-divide"},     {"fa.native-plus"},        {"fa.native-minus"},
    {"fa.native-modulo"},   {"fa.native-power"},      {"fa.native-left-shift"},  {"fa.native-right-shift"},
    {"fa.native-and"},      {"fa.native-or"},         {"fa.native-xor"},
};
inline constexpr Word kComparisons[6] = {
    {"fa.native-greater-than"},     {"fa.native-less-than"},     {"fa.native-equals"},
    {"fa.native-greater-or-equal"}, {"fa.native-less-or-equal"}, {"fa.native-not-equal"},
};
inline constexpr Word kSelector[9] = {
    {"fa.native-select"},           {"fa.native-count"},          {"fa.native-random"},
    {"fa.native-quality-transfer"}, {"fa.native-stack-size"},     {"fa.native-rocket-capacity"},
    {"fa.native-quality-filter"},   {"fa.native-time"},           {"fa.native-quality-select"},
};
inline constexpr Word kSelectMaximum{"fa.native-select-maximum"};
inline constexpr Word kSelectMinimum{"fa.native-select-minimum"};

// The Y key in the world when the cursor points at no entity and no tile.
inline constexpr Word kNothingHere{"fa.native-nothing-here"};

// Settings > Other: why the crash log upload checkbox is off and greyed out.
inline constexpr Word kCrashLogUploadOff{"fa.native-crash-log-upload-off"};

// A map tag the full map selects.
inline constexpr Word kMapTag{"fa.native-map-tag"};

// Escape on a selection tool's open selection.
inline constexpr Word kSelectionCancelled{"fa.native-selection-cancelled"};
// A selection's box, "5 by 3" tiles, and an upgrade it would make, "3 transport belt to fast
// transport belt".
inline constexpr Word kBoxSize{"fa.native-box-size"};
inline constexpr Word kUpgrade{"fa.native-upgrade"};

inline constexpr Word kNoTooltip{"fa.native-no-tooltip"};
inline constexpr Word kNoAction{"fa.native-no-action"};

std::string position(int index, int count);
std::string expandedState(bool expanded);
std::string flyoutHint(int count);
// One of the eight directions, north 0 clockwise.
std::string direction(int eighth);
// An alert category by its AlertCategory value, from attack to pipelines; empty for any other.
std::string alertCategory(uint8_t category);

} // namespace fa::vocab

template <>
struct std::formatter<fa::vocab::Word> : std::formatter<std::string> {
   auto format(const fa::vocab::Word& word, std::format_context& context) const {
      return std::formatter<std::string>::format(word.str(), context);
   }
};
