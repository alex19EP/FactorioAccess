#include "game.h"

#include "log.h"
#include "symbols.h"

namespace fa::game {

Layout layout;

bool resolve(pdb::SymbolTable& symbols) {
   bool ok = true;
   auto address = [&](uintptr_t& out, const char* name) {
      if (auto value = symbols.address(name))
         out = *value;
      else
         ok = false;
   };
   auto offset = [&](uint32_t& out, const char* type, const char* path) {
      if (auto value = symbols.offset(type, path))
         out = *value;
      else
         ok = false;
   };

   address(layout.guiLogic, "?logic@Gui@agui@@UEAAX_N@Z");
   address(layout.guiInstance, "?instance@Gui@agui@@2PEAV12@EA");

   offset(layout.guiBaseWidget, "agui::Gui", "baseWidget");
   offset(layout.guiFocusedWidget, "agui::Gui", "focusManager.focusedWidget");
   offset(layout.guiWidgetUnderMouse, "agui::Gui", "widgetUnderMouse");
   offset(layout.targeterTarget, "agui::GenericTargeterBase", "target");

   offset(layout.widgetTargetable, "agui::Widget", "agui::GenericTargetable");
   offset(layout.widgetParent, "agui::Widget", "parentWidget");
   offset(layout.widgetChildren, "agui::Widget", "children");
   offset(layout.widgetPrivateChildren, "agui::Widget", "privateChildren");
   offset(layout.widgetText, "agui::Widget", "text");
   offset(layout.widgetUsageBits, "agui::Widget", "usageBitMask");
   offset(layout.labelText, "agui::Label", "resizableText.data");

   if (ok) {
      log::info("Layout: Gui baseWidget {:#x} focused {:#x} underMouse {:#x}; Widget parent {:#x} children {:#x} "
                "privateChildren {:#x} text {:#x} usage {:#x}; Label text {:#x}",
                layout.guiBaseWidget, layout.guiFocusedWidget, layout.guiWidgetUnderMouse, layout.widgetParent,
                layout.widgetChildren, layout.widgetPrivateChildren, layout.widgetText, layout.widgetUsageBits,
                layout.labelText);
   }
   return ok;
}

} // namespace fa::game
