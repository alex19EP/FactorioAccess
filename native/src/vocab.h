#pragma once

#include <string>
#include <string_view>

// Every word the native layer says on its own (graph-a11y-spec A8). Text read from the game is
// already in the player's language; these are not yet, and live here so that translating them is
// one file. Kept short and lowercase, after the mod's own speech style.
namespace fa::vocab {

inline constexpr std::string_view kButton = "button";
inline constexpr std::string_view kCheckBox = "checkbox";
inline constexpr std::string_view kRadioButton = "radio button";
inline constexpr std::string_view kDropDown = "dropdown";
inline constexpr std::string_view kSlider = "slider";
inline constexpr std::string_view kSwitch = "switch";
inline constexpr std::string_view kEdit = "edit";
inline constexpr std::string_view kTab = "tab";
inline constexpr std::string_view kList = "list";
inline constexpr std::string_view kProgressBar = "progress bar";

inline constexpr std::string_view kChecked = "checked";
inline constexpr std::string_view kUnchecked = "not checked";
inline constexpr std::string_view kPartlyChecked = "partly checked";
inline constexpr std::string_view kSelected = "selected";
inline constexpr std::string_view kPressed = "pressed";
inline constexpr std::string_view kLeft = "left";
inline constexpr std::string_view kRight = "right";
inline constexpr std::string_view kDisabled = "disabled";
inline constexpr std::string_view kReadOnly = "read only";
inline constexpr std::string_view kBlank = "blank";

inline constexpr std::string_view kAllMods = "all mods";
inline constexpr std::string_view kAlternative = "alternative";
inline constexpr std::string_view kSortBy = "sort by";
inline constexpr std::string_view kContinue = "continue";
inline constexpr std::string_view kClose = "close";
inline constexpr std::string_view kAdjusting = "adjusting";

inline constexpr std::string_view kEmpty = "empty";
inline constexpr std::string_view kProductivity = "productivity";

// An entity window's slots and bars, named as the mod's own entity menus name them.
inline constexpr std::string_view kInputs = "input materials";
inline constexpr std::string_view kOutputs = "output products";
inline constexpr std::string_view kFuel = "fuel";
inline constexpr std::string_view kBurntResults = "burnt results";
inline constexpr std::string_view kModules = "modules";
inline constexpr std::string_view kProgress = "progress";
inline constexpr std::string_view kMining = "mining";
inline constexpr std::string_view kBurning = "burning";

// A chest's slot limit: the red X button and the slots it locks.
inline constexpr std::string_view kLimitSlots = "limit slots";
inline constexpr std::string_view kChooseFirstLocked = "choose first locked slot";
inline constexpr std::string_view kAllUnlocked = "all unlocked";
inline constexpr std::string_view kAllLocked = "all locked";
inline constexpr std::string_view kLocked = "locked";
inline constexpr std::string_view kLockFromHere = "lock from here";

// The quickbar and the page each of its bars shows ("page 3").
inline constexpr std::string_view kQuickBar = "quickbar";
inline constexpr std::string_view kPage = "page";

inline constexpr std::string_view kNoTooltip = "no tooltip";
inline constexpr std::string_view kNoAction = "no action";

std::string position(int index, int count);
std::string expandedState(bool expanded);
std::string flyoutHint(int count);
std::string unlocked(unsigned count);

} // namespace fa::vocab
