#pragma once

#include <cstdint>

namespace fa::pdb {
class SymbolTable;
}

namespace fa::game {

// Addresses and class layouts read from factorio.pdb, so nothing here is tied to one build.
struct Layout {
   // Functions and globals.
   uintptr_t guiLogic = 0;    // virtual void agui::Gui::logic(bool)
   uintptr_t guiInstance = 0; // static agui::Gui* agui::Gui::instance

   // agui::Gui
   uint32_t guiBaseWidget = 0;      // agui::TopContainer* baseWidget
   uint32_t guiFocusedWidget = 0;   // targeter in focusManager; holds a GenericTargetable*
   uint32_t guiWidgetUnderMouse = 0; // targeter; holds a GenericTargetable*

   // agui::GenericTargeterBase
   uint32_t targeterTarget = 0;

   // agui::Widget
   uint32_t widgetTargetable = 0; // GenericTargetable base, what targeters point at
   uint32_t widgetParent = 0;
   uint32_t widgetChildren = 0;        // std::vector<agui::Widget*>
   uint32_t widgetPrivateChildren = 0; // std::vector<agui::Widget*>; holds e.g. a Frame's content
   uint32_t widgetText = 0;            // std::string, already translated
   uint32_t widgetUsageBits = 0;

   // agui::Label keeps its caption here instead of in Widget::text.
   uint32_t labelText = 0; // std::string
};

// Bits of agui::Widget::usageBitMask, read from Widget::setVisible and Widget::isEnabled in
// 2.1.20. These are code constants, not PDB data, so they are the one part of the layout that a
// Factorio update can change silently.
inline constexpr uint32_t kUsageVisible = 0x4;
inline constexpr uint32_t kUsageHiddenMask = 0x20004; // visible only when (bits & mask) == visible
inline constexpr uint32_t kUsageEnabled = 0x8;

extern Layout layout;

// Fills `layout`; logs every missing name and returns false if anything could not be resolved.
bool resolve(pdb::SymbolTable& symbols);

} // namespace fa::game
