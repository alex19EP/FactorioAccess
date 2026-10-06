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

inline constexpr std::string_view kNoTooltip = "no tooltip";
inline constexpr std::string_view kNoAction = "no action";

std::string position(int index, int count);
std::string expandedState(bool expanded);
std::string flyoutHint(int count);

} // namespace fa::vocab
