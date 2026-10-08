#include "agui.h"

#include "game.h"
#include "world.h"

#include <windows.h>

#include <dbghelp.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

// The input is a bare type encoding, as in RTTI type descriptors. Older SDK headers lack it.
#ifndef UNDNAME_TYPE_ONLY
#define UNDNAME_TYPE_ONLY 0x2000
#endif

namespace fa::agui {

namespace {

using game::layout;

template <class T>
const T& at(const void* object, uint32_t offset) {
   return *reinterpret_cast<const T*>(static_cast<const std::byte*>(object) + offset);
}

// MSVC std::string: a 16-byte small buffer or a heap pointer, then size and capacity.
struct MsvcString {
   union {
      char buffer[16];
      const char* pointer;
   };
   size_t size;
   size_t capacity;
};

template <class T>
struct MsvcVector {
   T* first;
   T* last;
   T* end;
};

const Widget* fromTargetable(const std::byte* targetable) {
   if (!targetable) return nullptr;
   return reinterpret_cast<const Widget*>(targetable - layout.widgetTargetable);
}

const Widget* fromTargeter(const void* owner, uint32_t targeterOffset) {
   return fromTargetable(at<const std::byte*>(owner, targeterOffset + layout.targeterTarget));
}

// x64 MSVC RTTI: vtable[-1] is the complete object locator, whose type descriptor holds the
// decorated name, e.g. ".?AVTextButton@agui@@".
struct CompleteObjectLocator {
   DWORD signature;
   DWORD offset; // where the subobject owning this vtable sits in the complete object
   DWORD cdOffset;
   DWORD typeDescriptor; // image-relative
   DWORD classDescriptor;
   DWORD self;           // image-relative address of this locator
};

struct TypeDescriptor {
   const void* vtable;
   void* spare;
   char name[1];
};

struct ClassHierarchyDescriptor {
   DWORD signature;
   DWORD attributes;
   DWORD baseCount;
   DWORD baseArray; // image-relative array of image-relative BaseClassDescriptor addresses
};

// Every class in the hierarchy, the class itself first.
struct BaseClassDescriptor {
   DWORD typeDescriptor;
   DWORD containedBases;
   int32_t memberDisplacement; // offset of this base inside the complete object
   int32_t vbtableDisplacement; // -1 unless the base is virtual
   int32_t vbtableIndex;
   DWORD attributes;
   DWORD classDescriptor;
};

std::string undecorate(const char* decorated) {
   // ".?AVTextButton@agui@@" minus the leading dot is a type encoding that UnDecorateSymbolName
   // turns into "class agui::TextButton".
   char buffer[1024];
   if (!UnDecorateSymbolName(decorated + 1, buffer, sizeof(buffer), UNDNAME_32_BIT_DECODE | UNDNAME_TYPE_ONLY))
      return decorated;
   std::string_view name(buffer);
   for (std::string_view prefix : {"class ", "struct "}) {
      if (name.starts_with(prefix)) {
         name.remove_prefix(prefix.size());
         break;
      }
   }
   return std::string(name);
}

// The kinds in order of precedence: a CheckBox is also a ToggleButton, a TextButton also a Button.
constexpr std::pair<std::string_view, Kind> kKindBases[] = {
   {".?AVToolTip@agui@@", Kind::Ignored},
   {".?AVFiller@agui@@", Kind::Ignored},
   {".?AVEmptyWidget@agui@@", Kind::Ignored},
   {".?AVCheckBox@agui@@", Kind::CheckBox},
   {".?AVRadioButton@agui@@", Kind::RadioButton},
   {".?AVToggleButton@agui@@", Kind::CheckBox},
   {".?AVDropDown@agui@@", Kind::DropDown},
   {".?AVSlider@agui@@", Kind::Slider},
   {".?AVTextBox@agui@@", Kind::TextBox},
   {".?AVSwitch@agui@@", Kind::Switch},
   {".?AVTab@agui@@", Kind::Tab},
   {".?AVTabbedPane@agui@@", Kind::TabbedPane},
   {".?AVListBox@agui@@", Kind::ListBox},
   {".?AVButton@agui@@", Kind::Button},
   // A LayoutButton laid out as a recipe slot and name, clicked as one button.
   {".?AVRecipeInfoWidget@@", Kind::Button},
   {".?AVLabel@agui@@", Kind::Label},
   {".?AVScrollPane@agui@@", Kind::ScrollPane},
   {".?AVTable@agui@@", Kind::Table},
   {".?AVHorizontalFlow@agui@@", Kind::HorizontalFlow},
   // A description's "Stack size:" beside "50", laid out side by side.
   {".?AVTwoLabelsFlow@agui@@", Kind::HorizontalFlow},
   {".?AVWindow@agui@@", Kind::Window},
   {".?AVProgressBar@agui@@", Kind::ProgressBar},
};

// Every scroll bar is an instance of the template agui::ScrollBar<Policy>.
constexpr std::string_view kScrollBarPrefix = ".?AV?$ScrollBar@";

struct ClassInfo {
   std::string name;
   Kind kind = Kind::Container;
   // Each non-virtual base class, by decorated name, at its offset from the agui::Widget subobject
   // that the widget pointers we hold point at.
   std::unordered_map<std::string_view, int32_t> bases;
};

const ClassInfo& classInfo(const Widget* widget) {
   static std::unordered_map<const void*, ClassInfo> cache;
   auto* vtable = *reinterpret_cast<const void* const* const*>(widget);
   auto [it, inserted] = cache.try_emplace(vtable);
   ClassInfo& info = it->second;
   if (!inserted) return info;

   auto* locator = static_cast<const CompleteObjectLocator*>(vtable[-1]);
   auto imageBase = reinterpret_cast<uintptr_t>(locator) - locator->self;
   auto typeAt = [imageBase](DWORD rva) { return reinterpret_cast<const TypeDescriptor*>(imageBase + rva); };
   info.name = undecorate(typeAt(locator->typeDescriptor)->name);

   auto* hierarchy = reinterpret_cast<const ClassHierarchyDescriptor*>(imageBase + locator->classDescriptor);
   auto* bases = reinterpret_cast<const DWORD*>(imageBase + hierarchy->baseArray);
   bool scrollBar = false;
   for (DWORD i = 0; i < hierarchy->baseCount; ++i) {
      auto* base = reinterpret_cast<const BaseClassDescriptor*>(imageBase + bases[i]);
      if (base->vbtableDisplacement != -1) continue;
      std::string_view name = typeAt(base->typeDescriptor)->name;
      info.bases.try_emplace(name, base->memberDisplacement - static_cast<int32_t>(locator->offset));
      scrollBar |= name.starts_with(kScrollBarPrefix);
   }
   if (scrollBar) {
      info.kind = Kind::Ignored;
   } else {
      for (const auto& [base, kind] : kKindBases) {
         if (info.bases.contains(base)) {
            info.kind = kind;
            break;
         }
      }
   }
   return info;
}

// The given agui base class of a widget, or null when the widget does not derive from it.
const std::byte* asBase(const Widget* widget, std::string_view decoratedBase) {
   const auto& bases = classInfo(widget).bases;
   auto it = bases.find(decoratedBase);
   if (it == bases.end()) return nullptr;
   return reinterpret_cast<const std::byte*>(widget) + it->second;
}

const std::byte* asBaseChecked(const Widget* widget, std::string_view decoratedBase) {
   const std::byte* base = asBase(widget, decoratedBase);
   // Callers check the kind first; a mismatch here is a bug in the caller.
   if (!base) RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
   return base;
}

std::string_view readString(const void* object, uint32_t offset) {
   const auto& string = at<MsvcString>(object, offset);
   return {string.capacity >= sizeof(string.buffer) ? string.pointer : string.buffer, string.size};
}

using VirtualTable = void* const*;

template <class Result, class... Args>
Result callVirtual(const Widget* widget, uint32_t slot, Args... args) {
   auto* self = const_cast<Widget*>(widget);
   auto vtable = *reinterpret_cast<VirtualTable*>(self);
   return reinterpret_cast<Result (*)(Widget*, Args...)>(vtable[slot])(self, args...);
}

// agui::KeyEnum / agui::ExtendedKeyEnum values and the SDL3 keycodes behind them.
struct KeyCodes {
   uint32_t key;     // agui::KeyEnum
   uint32_t extKey;  // agui::ExtendedKeyEnum
   uint32_t keyCode; // SDL_Keycode
   uint32_t unichar;
};

KeyCodes codesOf(Key key) {
   switch (key) {
   case Key::Enter: return {13, 0, 0x0d, 13};
   case Key::Space: return {32, 0, 0x20, 32};
   case Key::Up: return {0, 31, 0x40000052, 0};
   case Key::Down: return {0, 32, 0x40000051, 0};
   case Key::Left: return {0, 33, 0x40000050, 0};
   case Key::Right: return {0, 34, 0x4000004f, 0};
   }
   return {};
}

} // namespace

const Gui* instance() { return *reinterpret_cast<const Gui* const*>(layout.guiInstance); }

const Gui* applicationGui() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   if (!context) return nullptr;
   return at<const Gui*>(context, layout.globalGui);
}

bool inGame() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   return context && at<const void*>(context, layout.globalGame) != nullptr;
}

const Widget* menuStateWindow() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   if (!context) return nullptr;
   auto* manager = at<const std::byte*>(context, layout.globalAppManager);
   if (!manager) return nullptr;
   const auto& states = at<MsvcVector<const std::byte* const>>(manager, layout.appManagerStates);
   if (states.first == states.last) return nullptr;
   const std::byte* state = states.last[-1];
   // Not a widget, but polymorphic all the same, which is all classInfo reads. InGame, the state
   // of plain play, owns no window.
   for (const auto& [base, displacement] : classInfo(reinterpret_cast<const Widget*>(state)).bases)
      if (base.starts_with(".?AV?$AppManagerStateWithGuiManualConstruction@"))
         return at<const Widget*>(state + displacement, layout.appStateGui);
   return nullptr;
}

const Widget* versionLabel() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   if (!context) return nullptr;
   auto* manager = at<const std::byte*>(context, layout.globalAppManager);
   return manager ? at<const Widget*>(manager, layout.appVersionLabel) : nullptr;
}

std::vector<const Widget*> mainMenuPanels(const Widget* mainMenu) {
   const std::byte* menu = asBase(mainMenu, ".?AVMainMenuGui@@");
   if (!menu) return {};
   std::vector<const Widget*> panels;
   for (uint32_t offset : {layout.mainMenuLanguage, layout.mainMenuSimulation, layout.mainMenuAdvert})
      if (const Widget* panel = at<const Widget*>(menu, offset)) panels.push_back(panel);
   return panels;
}

const Widget* scenarioMessage() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   if (!context) return nullptr;
   auto* game = at<const std::byte*>(context, layout.globalGame);
   if (!game) return nullptr;
   auto* view = at<const std::byte*>(game, layout.gameView);
   if (!view) return nullptr;
   return at<const Widget*>(view, layout.gameViewMessage);
}

const Widget* scenarioMessageLabel(const Widget* message) { return member(message, layout.speechBubbleLabel); }

void confirmScenarioMessage() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   auto* source = at<void*>(context, layout.globalPlayerInputSource);
   reinterpret_cast<bool (*)(void*)>(layout.processNextDialog)(source);
}

const Widget* baseWidget(const Gui* gui) { return at<const Widget*>(gui, layout.guiBaseWidget); }

const Widget* focusedWidget(const Gui* gui) { return fromTargeter(gui, layout.guiFocusedWidget); }

const Widget* widgetUnderMouse(const Gui* gui) { return fromTargeter(gui, layout.guiWidgetUnderMouse); }

const Widget* topModal(const Gui* gui) {
   // The focus manager keeps modals sorted by priority, the topmost last.
   const auto& modals = at<MsvcVector<const std::byte>>(gui, layout.guiModals);
   for (const std::byte* entry = modals.last; entry > modals.first;) {
      entry -= layout.modalSize;
      if (const Widget* widget = fromTargeter(entry, layout.modalWidget)) return widget;
   }
   return nullptr;
}

const Widget* parent(const Widget* widget) { return at<const Widget*>(widget, layout.widgetParent); }

std::span<const Widget* const> children(const Widget* widget) {
   const auto& vector = at<MsvcVector<const Widget* const>>(widget, layout.widgetChildren);
   return {vector.first, vector.last};
}

std::span<const Widget* const> privateChildren(const Widget* widget) {
   const auto& vector = at<MsvcVector<const Widget* const>>(widget, layout.widgetPrivateChildren);
   return {vector.first, vector.last};
}

std::string_view text(const Widget* widget) {
   if (const std::byte* label = asBase(widget, ".?AVLabel@agui@@")) return readString(label, layout.labelText);
   return readString(widget, layout.widgetText);
}

bool visible(const Widget* widget) {
   return (at<uint32_t>(widget, layout.widgetUsageBits) & game::kUsageHiddenMask) == game::kUsageVisible;
}

bool enabled(const Widget* widget) { return (at<uint32_t>(widget, layout.widgetUsageBits) & game::kUsageEnabled) != 0; }

const std::string& className(const Widget* widget) { return classInfo(widget).name; }

Kind kind(const Widget* widget) { return classInfo(widget).kind; }

const Widget* frameTitle(const Widget* widget) {
   const std::byte* frame = asBase(widget, ".?AVFrame@agui@@");
   if (!frame) return nullptr;
   return reinterpret_cast<const Widget*>(frame + layout.frameTitle);
}

CheckState checkState(const Widget* toggleButton) {
   return static_cast<CheckState>(
      at<uint32_t>(asBaseChecked(toggleButton, ".?AVToggleButton@agui@@"), layout.toggleChecked));
}

bool buttonToggled(const Widget* button) { return at<bool>(asBaseChecked(button, ".?AVButton@agui@@"), layout.buttonToggled); }

bool buttonIsToggle(const Widget* button) { return at<bool>(asBaseChecked(button, ".?AVButton@agui@@"), layout.buttonIsToggle); }

SliderValue sliderValue(const Widget* slider) {
   const std::byte* base = asBaseChecked(slider, ".?AVSlider@agui@@");
   return {at<double>(base, layout.sliderValue), at<double>(base, layout.sliderMin), at<double>(base, layout.sliderMax),
           at<double>(base, layout.sliderStep)};
}

SwitchState switchState(const Widget* widget) {
   return static_cast<SwitchState>(at<uint8_t>(asBaseChecked(widget, ".?AVSwitch@agui@@"), layout.switchState));
}

bool readOnly(const Widget* textBox) { return at<bool>(asBaseChecked(textBox, ".?AVTextBox@agui@@"), layout.textBoxReadOnly); }

std::string_view textBoxText(const Widget* textBox) {
   return readString(asBaseChecked(textBox, ".?AVTextBox@agui@@"), layout.textBoxText);
}

LabelTone labelTone(const Widget* label) {
   const std::byte* base = asBase(label, ".?AVLabel@agui@@");
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   const std::byte* styles = context ? at<const std::byte*>(context, layout.globalStyle) : nullptr;
   if (!base || !styles) return LabelTone::Plain;
   const std::byte* parent = at<const std::byte*>(base, layout.labelStyleParent);
   auto is = [&](uint32_t field) {
      const std::byte* style = at<const std::byte*>(styles, field);
      return style && parent == style + layout.integratedLabelStyleAgui;
   };
   if (is(layout.guiStyleBoldRedLabel)) return LabelTone::Red;
   if (is(layout.guiStyleBoldOrangeLabel)) return LabelTone::Orange;
   return LabelTone::Plain;
}

bool tabSelected(const Widget* tab) {
   const auto* pane = at<const std::byte*>(asBaseChecked(tab, ".?AVTab@agui@@"), layout.tabPane);
   if (!pane) return false;
   const auto& tabs = at<MsvcVector<const std::byte>>(pane, layout.tabbedPaneTabs);
   const auto* selected = at<const std::byte*>(pane, layout.tabbedPaneSelected);
   if (selected < tabs.first || selected >= tabs.last) return false;
   return fromTargeter(selected, layout.tabEntryTab) == tab;
}

unsigned tableColumns(const Widget* table) { return at<uint32_t>(asBaseChecked(table, ".?AVTable@agui@@"), layout.tableColumns); }

ToolTip toolTip(const Widget* widget) {
   const auto* creator = at<const Widget*>(widget, layout.widgetToolTipCreator);
   // Not a widget, but polymorphic all the same, which is all classInfo reads.
   if (!creator || !asBase(creator, ".?AVPlainToolTipCreator@agui@@")) return {};
   return {readString(creator, layout.plainToolTipTitle), readString(creator, layout.plainToolTipText)};
}

const Widget* showTooltip(const Widget* widget, bool& created) {
   created = !fromTargeter(widget, layout.widgetToolTip);
   reinterpret_cast<void (*)(const Widget*)>(layout.checkCreateTooltip)(widget);
   const Widget* tooltip = fromTargeter(widget, layout.widgetToolTip);
   created &= tooltip != nullptr;
   // A new tooltip is empty until the end of the Gui's next logic fills it.
   if (tooltip) callVirtual<void>(tooltip, layout.slotToolTipUpdateContent);
   return tooltip;
}

void removeTooltip(const Widget* widget) {
   reinterpret_cast<bool (*)(const Widget*, bool)>(layout.removeToolTipWidget)(widget, true);
}

bool derivesFrom(const Widget* widget, std::string_view className) {
   // "agui::Table" is decorated ".?AVTable@agui@@": the scopes innermost first.
   std::string decorated = ".?AV";
   std::string_view rest = className;
   std::vector<std::string_view> scopes;
   for (size_t split; (split = rest.find("::")) != std::string_view::npos; rest.remove_prefix(split + 2))
      scopes.push_back(rest.substr(0, split));
   decorated.append(rest).append("@");
   for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) decorated.append(*it).append("@");
   decorated.append("@");
   return asBase(widget, decorated) != nullptr;
}

bool derivesFromTemplate(const Widget* widget, std::string_view templateName) {
   // "FilterSelectGui<...>" is decorated ".?AV?$FilterSelectGui@...".
   std::string prefix = std::string(".?AV?$").append(templateName).append("@");
   for (const auto& [base, displacement] : classInfo(widget).bases)
      if (base.starts_with(prefix)) return true;
   return false;
}

const Widget* member(const Widget* owner, uint32_t offset) {
   return reinterpret_cast<const Widget*>(reinterpret_cast<const std::byte*>(owner) + offset);
}

const Widget* pointerMember(const Widget* owner, uint32_t offset) { return at<const Widget*>(owner, offset); }

const Widget* dialogButtons(const Widget* window) {
   // Dialog is a template, so its instantiations share only the prefix of their decorated name.
   for (const auto& [base, displacement] : classInfo(window).bases)
      if (base.starts_with(".?AV?$Dialog@")) return member(window, displacement + layout.dialogButtons);
   return nullptr;
}

MenuParts menuParts(const Widget* window) {
   // MenuGui is a template too.
   for (const auto& [base, displacement] : classInfo(window).bases)
      if (base.starts_with(".?AV?$MenuGui@"))
         return {member(window, displacement + layout.menuTop), member(window, displacement + layout.menuMain),
                 member(window, displacement + layout.menuBottom)};
   return {};
}

std::vector<const Widget*> listBoxItems(const Widget* listBox) {
   const std::byte* base = asBaseChecked(listBox, ".?AVListBox@agui@@");
   const auto& items = at<MsvcVector<const std::byte>>(base, layout.listBoxItems);
   std::vector<const Widget*> buttons;
   for (const std::byte* item = items.first; item < items.last; item += layout.listBoxItemSize)
      if (const Widget* button = at<const Widget*>(item, layout.listBoxItemButton)) buttons.push_back(button);
   return buttons;
}

namespace {

// A prototype's localised name in the game's current locale.
std::string_view localisedName(const std::byte* prototype) {
   using Str = const MsvcString* (*)(const void* localisedString, const void* localeProvider);
   const MsvcString* name =
       reinterpret_cast<Str>(layout.localisedStringStr)(prototype + layout.prototypeLocalisedName, nullptr);
   return {name->capacity >= sizeof(name->buffer) ? name->pointer : name->buffer, name->size};
}

} // namespace

std::string_view iconName(const Widget* widget) {
   const std::byte* button = asBase(widget, ".?AVIconButton@@");
   if (!button) return {};
   const std::byte* sprite = at<const std::byte*>(button, layout.iconButtonSprite);
   if (!sprite) return {};
   const std::byte* owner = at<const std::byte*>(sprite, layout.spriteOwner);
   if (!owner) return {};
   return localisedName(owner);
}

namespace {

// The prototype an ID indexes in a PrototypeList<T>::indexToPrototype vector, or null.
const std::byte* prototypeAt(uintptr_t list, size_t index) {
   const auto& prototypes = *reinterpret_cast<const MsvcVector<const std::byte* const>*>(list);
   return index < static_cast<size_t>(prototypes.last - prototypes.first) ? prototypes.first[index] : nullptr;
}

// An MSVC std::map node: the tree links and flags, then the stored pair.
struct MapNode {
   const MapNode* left;
   const MapNode* parent;
   const MapNode* right;
   char color;
   char isNil;
};
constexpr size_t kMapValue = 0x20;

// The key of the std::map<ID<T, unsigned short>, std::unique_ptr<agui::Button>> entry holding `button`.
std::optional<uint16_t> mapKeyOf(const std::byte* map, const Widget* button) {
   const MapNode* head = at<const MapNode*>(map, 0);
   std::vector<const MapNode*> pending{head->parent};
   while (!pending.empty()) {
      const MapNode* node = pending.back();
      pending.pop_back();
      if (node->isNil) continue;
      auto* value = reinterpret_cast<const std::byte*>(node) + kMapValue;
      if (at<const Widget*>(value, 8) == button) return at<uint16_t>(value, 0);
      pending.push_back(node->left);
      pending.push_back(node->right);
   }
   return std::nullopt;
}

} // namespace

SlotItem slotItem(const Widget* slot) {
   const std::byte* self = asBaseChecked(slot, ".?AVInventoryGuiSlot@@");
   SlotItem item;
   // As InventoryGuiSlot::getStack reads it.
   const std::byte* stack = at<const std::byte*>(self, layout.slotItemStack);
   uint16_t index = at<uint16_t>(self, layout.slotIndex);
   if (const std::byte* inventory = at<const std::byte*>(self, layout.slotInventory)) {
      if (index >= at<uint16_t>(inventory, layout.inventorySize)) return item;
      stack = at<const std::byte*>(inventory, layout.inventoryData) + size_t{index} * layout.itemStackSize;
      // As InventoryGuiSlot::paintComponent draws the hand.
      item.inHand = at<uint16_t>(inventory, layout.inventoryHand) == index;
   }
   if (!stack) return item;
   item.count = at<uint32_t>(stack, layout.itemStackCount);
   if (item.count == 0) return item;
   item.data = at<const void*>(stack, layout.itemStackData);
   // As BlueprintBookSlot::paintComponent highlights it.
   if (const std::byte* bookSlot = asBase(slot, ".?AVBlueprintBookSlot@@"))
      if (const std::byte* book = at<const std::byte*>(bookSlot, layout.bookSlotBook))
         item.active = at<uint16_t>(book, layout.bookActiveIndex) == index;
   if (const std::byte* prototype = prototypeAt(layout.itemPrototypes, at<uint16_t>(stack, layout.itemStackItem)))
      item.name = localisedName(prototype);
   if (const std::byte* quality = prototypeAt(layout.qualityPrototypes, at<uint8_t>(stack, layout.itemStackQuality));
       quality && readString(quality, layout.prototypeName) != "normal")
      item.quality = localisedName(quality);
   return item;
}

bool blueprintsListView(const Widget* list) {
   return at<uint32_t>(asBaseChecked(list, ".?AVBlueprintsList@@"), layout.listViewMode) == layout.listViewList;
}

InventoryBar inventoryBar(const Widget* widget) {
   for (const Widget* ancestor = widget; ancestor; ancestor = parent(ancestor)) {
      const std::byte* self = asBase(ancestor, ".?AVInventoryWithBarGui@@");
      if (!self) continue;
      InventoryBar bar;
      bar.button = reinterpret_cast<const Widget*>(self + layout.barGuiButton);
      bar.choosing = at<uint32_t>(self, layout.barGuiMode) == 1;
      if (const std::byte* inventory =
             at<const std::byte*>(asBaseChecked(ancestor, ".?AVInventoryGui@@"), layout.inventoryGuiInventory)) {
         bar.size = at<uint16_t>(inventory, layout.inventorySize);
         bar.unlocked = std::min<unsigned>(at<uint16_t>(inventory, layout.inventoryBar), bar.size);
      }
      return bar;
   }
   return {};
}

bool slotLocked(const Widget* slot) {
   const std::byte* self = asBase(slot, ".?AVInventoryGuiSlot@@");
   if (!self || !at<const std::byte*>(self, layout.slotInventory)) return false;
   InventoryBar bar = inventoryBar(slot);
   return bar.button && at<uint16_t>(self, layout.slotIndex) >= bar.unlocked;
}

RecipeItem recipeItem(const Widget* craftingList, const Widget* slot) {
   RecipeItem item;
   item.craftable = at<uint32_t>(asBaseChecked(slot, ".?AVRecipeSlot@@"), layout.recipeSlotCount);
   const std::byte* list = asBaseChecked(craftingList, ".?AV?$SelectListGui@V?$ID@VRecipePrototype@@G@@@@");
   if (auto id = mapKeyOf(list + layout.recipeListSlots, slot))
      if (const std::byte* recipe = prototypeAt(layout.recipePrototypes, *id)) item.name = localisedName(recipe);
   return item;
}

namespace {

// A const getter that `subobject`'s own vtable introduces: the game's slot buttons inherit theirs
// from secondary bases, so the call goes through that base with `this` adjusted to it.
template <class Result>
Result callVirtualAt(const std::byte* subobject, uint32_t slot) {
   auto* self = const_cast<std::byte*>(subobject);
   auto vtable = *reinterpret_cast<VirtualTable*>(self);
   return reinterpret_cast<Result (*)(void*)>(vtable[slot])(self);
}

} // namespace

bool isSlotButton(const Widget* widget) {
   return asBase(widget, ".?AVSlotButtonBase@@") && asBase(widget, ".?AVPrototypeProvider@@") &&
          asBase(widget, ".?AVButtonNumber@@");
}

SlotButton slotButton(const Widget* slot) {
   const std::byte* provider = asBaseChecked(slot, ".?AVPrototypeProvider@@");
   SlotButton button;
   // An item slot with neither an inventory nor a loose stack aborts the game in its getters.
   if (const std::byte* item = asBase(slot, ".?AVInventoryGuiSlot@@");
       item && !at<const std::byte*>(item, layout.slotInventory) && !at<const std::byte*>(item, layout.slotItemStack))
      return button;
   if (auto* prototype = callVirtualAt<const std::byte*>(provider, layout.providerBasePrototype))
      button.name = localisedName(prototype);
   if (auto* quality = callVirtualAt<const std::byte*>(provider, layout.providerQualityPrototype);
       quality && readString(quality, layout.prototypeName) != "normal")
      button.quality = localisedName(quality);
   button.count = callVirtualAt<double>(asBaseChecked(slot, ".?AVButtonNumber@@"), layout.buttonNumberCount);
   return button;
}

double progress(const Widget* bar) { return at<double>(asBaseChecked(bar, ".?AVProgressBar@agui@@"), layout.progressBarValue); }

bool isRecordSlot(const Widget* widget) { return asBase(widget, ".?AVBlueprintRecordSlotButton@@"); }

uint16_t bookRecordActiveIndex(const void* book, const void* player) {
   using ActiveIndexFunction = uint16_t (*)(const void* book, const void* player, const void* latencyState);
   const void* latency = player ? at<const void*>(player, layout.playerLatencyState) : nullptr;
   return reinterpret_cast<ActiveIndexFunction>(layout.bookRecordActiveIndex)(book, player, latency);
}

RecordSlot recordSlot(const Widget* slot) {
   using GetRecordFunction = const void* (*)(const void* button);
   using CursorRecordFunction = void* (*)(const void* adapter, std::byte* out);
   const std::byte* button = asBaseChecked(slot, ".?AVBlueprintRecordSlotButton@@");
   RecordSlot result;
   result.record = reinterpret_cast<GetRecordFunction>(layout.recordSlotRecord)(button);
   if (!result.record) return result;
   const std::byte* record = asBase(static_cast<const Widget*>(result.record), ".?AVBlueprintRecord@@");
   result.preview = callVirtualAt<bool>(record, layout.recordIsPreview);
   result.progress = at<float>(button, layout.recordSlotProgress);
   // As BlueprintRecordSlotButton::paintComponent highlights the book's active slot.
   const std::byte* player = at<const std::byte*>(button, layout.recordSlotPlayer);
   result.player = player;
   if (const std::byte* book = at<const std::byte*>(button, layout.recordSlotBook))
      result.active = bookRecordActiveIndex(book, player) == at<uint16_t>(button, layout.recordSlotIndex);
   // And draws the hand over the record in the player's hand, as the state the GUI shows has it.
   std::array<std::byte, 64> held{};
   if (player && at<uint32_t>(button, layout.recordSlotGrabbed) == layout.recordSlotShowsGrabbed &&
       layout.recordIdSize <= held.size()) {
      const std::byte* adapter = at<const std::byte*>(player, layout.playerLatencyAdapter);
      if (!adapter) adapter = player + layout.playerGameStateAdapter;
      auto vtable = *reinterpret_cast<VirtualTable const*>(adapter);
      reinterpret_cast<CursorRecordFunction>(vtable[layout.adapterCursorRecord])(adapter, held.data());
      const std::byte* id = record + layout.recordId;
      result.inHand = at<uint16_t>(held.data(), layout.recordIdPlayer) == at<uint16_t>(id, layout.recordIdPlayer) &&
                      at<uint32_t>(held.data(), layout.recordIdIndex) == at<uint32_t>(id, layout.recordIdIndex);
   }
   return result;
}

EntityWindowParts entityWindowParts(const Widget* window) {
   const std::byte* gui = asBase(window, ".?AVGameGuiWithControllerInventory@@");
   if (!gui) return {};
   EntityWindowParts parts;
   parts.entity = reinterpret_cast<const Widget*>(gui + layout.entityMainWindow);
   parts.header = member(parts.entity, layout.frameHeader);
   // The holder is no widget, but polymorphic all the same: its RTTI tells the player's own
   // inventory from the remote view's item list.
   auto* holder = at<const Widget*>(gui, layout.entityInventoryHolder);
   if (holder && derivesFrom(holder, "GameControllerInventoryHolder")) {
      parts.inventory = member(holder, layout.holderInventory);
      parts.inventoryTitle = member(holder, layout.holderTitle);
      parts.inventoryPanel = parent(parts.inventory);
   }
   struct Machine {
      std::string_view type;
      uint32_t progressBar, bonusBar, recipe, inputs, outputs, modules, changeRecipe;
   };
   for (const Machine& machine :
        {Machine{".?AVAssemblingMachineGui@@", layout.assemblerProgressBar, layout.assemblerBonusBar,
                 layout.assemblerRecipe, layout.assemblerInputs, layout.assemblerOutputs, layout.assemblerModules,
                 layout.assemblerChangeRecipe},
         Machine{".?AVFurnaceGui@@", layout.furnaceProgressBar, layout.furnaceBonusBar, layout.furnaceRecipe,
                 layout.furnaceInputs, layout.furnaceOutputs, layout.furnaceModules, 0},
         Machine{".?AVMiningDrillGui@@", layout.drillProgressBar, layout.drillBonusBar, 0, 0, 0, layout.drillModules,
                 0}}) {
      const std::byte* base = asBase(window, machine.type);
      if (!base) continue;
      auto widgetAt = [base](uint32_t offset) {
         return offset ? reinterpret_cast<const Widget*>(base + offset) : nullptr;
      };
      parts.progressBar = widgetAt(machine.progressBar);
      parts.bonusBar = widgetAt(machine.bonusBar);
      parts.recipe = widgetAt(machine.recipe);
      parts.inputs = widgetAt(machine.inputs);
      parts.outputs = widgetAt(machine.outputs);
      parts.modules = machine.modules ? at<const Widget*>(base, machine.modules) : nullptr;
      parts.changeRecipe = widgetAt(machine.changeRecipe);
   }
   return parts;
}

BurnerParts burnerParts(const Widget* burnerInfo) {
   return {member(burnerInfo, layout.burnerSlots), member(burnerInfo, layout.burntResultSlots),
           member(burnerInfo, layout.burnerProgressBar)};
}

EntityPanelParts entityPanelParts(const Widget* window) {
   const std::byte* sideButtons = asBase(window, ".?AVGuiWithSideButtons@@");
   if (!sideButtons) return {};
   EntityPanelParts parts;
   if (const std::byte* gui = asBase(window, ".?AVGenericOnOffEntityGui@@")) {
      parts.titled = reinterpret_cast<const Widget*>(gui + layout.onOffEntityWindow);
   } else if (asBase(window, ".?AVSplitterGui@@")) {
      parts.titled = window;
   } else if (asBase(window, ".?AVEntityWithEnergySourceGui@@")) {
      parts.titled = entityWindowParts(window).entity;
   } else {
      return {};
   }
   parts.sidePanel = reinterpret_cast<const Widget*>(sideButtons + layout.sidePanelContainer);
   return parts;
}

FluidBoxParts fluidBoxParts(const Widget* window) {
   const std::byte* gui = asBase(window, ".?AVSingleFluidBoxEntityGui@@");
   if (!gui) return {};
   const std::byte* box = gui + layout.singleFluidBoxGui;
   return {reinterpret_cast<const Widget*>(box + layout.fluidBoxIcon),
           reinterpret_cast<const Widget*>(box + layout.fluidBoxBar)};
}

ElectricNetworkParts electricNetworkParts(const Widget* window) {
   // Both are the same template over a pointer, so their members lie at the same offsets.
   const std::byte* pole = asBase(window, ".?AV?$ElectricNetworkGuiWindow@VElectricPole@@@@");
   const std::byte* gui = pole ? pole : asBase(window, ".?AV?$ElectricNetworkGuiWindow@VSurface@@@@");
   if (!gui) return {};
   ElectricNetworkParts parts;
   parts.bars = reinterpret_cast<const Widget*>(gui + layout.electricNetworkBars);
   parts.flows = reinterpret_cast<const Widget*>(gui + layout.electricNetworkFlows);
   parts.consumption = reinterpret_cast<const Widget*>(gui + layout.electricNetworkConsumption);
   parts.production = reinterpret_cast<const Widget*>(gui + layout.electricNetworkProduction);
   parts.storage = reinterpret_cast<const Widget*>(gui + layout.electricNetworkStorage);
   for (const Widget* frame : {parts.consumption, parts.production, parts.storage})
      parts.graphs.push_back(member(frame, layout.flowFrameGraph));
   return parts;
}

const Widget* wrappedWindow(const Widget* widget) {
   const std::byte* wrapper = asBase(widget, ".?AVCustomGuiGameGuiWrapper@@");
   if (!wrapper) return nullptr;
   // The table's other cells are the side flows and empty fillers; only the window is a Window.
   for (const Widget* cell : children(reinterpret_cast<const Widget*>(wrapper + layout.relativeWrapperTable)))
      if (asBase(cell, ".?AVWindow@agui@@")) return cell;
   return nullptr;
}

std::vector<const Widget*> relativeFlows(const Widget* wrapper) {
   const std::byte* base = asBaseChecked(wrapper, ".?AVCustomGuiGameGuiWrapper@@");
   std::vector<const Widget*> flows;
   for (uint32_t offset : {layout.relativeWrapperTop, layout.relativeWrapperLeft, layout.relativeWrapperRight,
                           layout.relativeWrapperBottom})
      flows.push_back(reinterpret_cast<const Widget*>(base + offset));
   return flows;
}

unsigned selectedRow(const Widget* table) {
   return at<uint32_t>(asBaseChecked(table, ".?AVTableWithSelection@agui@@"), layout.tableSelectedIndex);
}

const Widget* quickBar() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   if (!context) return nullptr;
   auto* game = at<const std::byte*>(context, layout.globalGame);
   if (!game) return nullptr;
   auto* view = at<const std::byte*>(game, layout.gameView);
   if (!view) return nullptr;
   auto* controllerView = at<std::byte*>(view, layout.gameViewControllerView);
   if (!controllerView) return nullptr;
   auto vtable = *reinterpret_cast<VirtualTable*>(controllerView);
   return reinterpret_cast<const Widget* (*)(void*)>(vtable[layout.controllerViewQuickBar])(controllerView);
}

namespace {

std::vector<QuickBarRow> rowsAt(const Widget* quickBar, uint32_t offset) {
   std::vector<QuickBarRow> rows;
   const auto& all = at<MsvcVector<const std::byte* const>>(quickBar, offset);
   for (const std::byte* const* it = all.first; it != all.last; ++it) {
      const std::byte* widgets = *it;
      QuickBarRow& row = rows.emplace_back();
      row.page = at<uint8_t>(widgets, layout.rowPage);
      row.button = at<const Widget*>(widgets, layout.rowButton);
      const auto& slots = at<MsvcVector<const Widget* const>>(widgets, layout.rowSlots);
      row.slots.assign(slots.first, slots.last);
   }
   return rows;
}

} // namespace

std::vector<QuickBarRow> quickBarRows(const Widget* quickBar) { return rowsAt(quickBar, layout.quickBarMainRows); }

int quickBarPickingFor(const Widget* quickBar) {
   // MSVC std::optional<unsigned char>: the value, then whether there is one.
   if (!at<bool>(quickBar, layout.quickBarPickingFor + 1)) return -1;
   return at<uint8_t>(quickBar, layout.quickBarPickingFor);
}

std::vector<QuickBarRow> quickBarPickerRows(const Widget* quickBar) {
   return rowsAt(quickBar, layout.quickBarPickerRows);
}

const Widget* shortcutBar() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   if (!context) return nullptr;
   auto* game = at<const std::byte*>(context, layout.globalGame);
   if (!game) return nullptr;
   auto* view = at<const std::byte*>(game, layout.gameView);
   if (!view) return nullptr;
   auto* controllerView = at<std::byte*>(view, layout.gameViewControllerView);
   if (!controllerView) return nullptr;
   auto vtable = *reinterpret_cast<VirtualTable*>(controllerView);
   return reinterpret_cast<const Widget* (*)(void*)>(vtable[layout.controllerViewShortcutBar])(controllerView);
}

std::vector<std::vector<Shortcut>> shortcutBarRows(const Widget* shortcutBar) {
   // Columns with no shortcut at all are hidden; the rest show every place, empty or not.
   std::vector<const Widget*> columns;
   const auto& all = at<MsvcVector<const Widget* const>>(shortcutBar, layout.shortcutBarColumns);
   for (const Widget* const* it = all.first; it != all.last; ++it)
      if (visible(*it)) columns.push_back(*it);

   std::vector<std::vector<Shortcut>> rows;
   for (const Widget* column : columns) {
      std::span<const Widget* const> buttons = children(column);
      if (rows.size() < buttons.size()) rows.resize(buttons.size());
      for (size_t row = 0; row < buttons.size(); ++row) {
         const Widget* button = buttons[row];
         const std::byte* behavior = at<const std::byte*>(button, layout.shortcutButtonBehavior);
         if (!behavior) continue;
         rows[row].push_back({button, localisedName(at<const std::byte*>(behavior, layout.shortcutBehaviorPrototype)),
                              buttonIsToggle(button)});
      }
   }
   std::erase_if(rows, [](const std::vector<Shortcut>& row) { return row.empty(); });
   return rows;
}

const Widget* shortcutBarListButton(const Widget* shortcutBar) {
   return member(shortcutBar, layout.shortcutBarListButton);
}

namespace {

// The loaded game's GameView, or null.
const std::byte* gameView() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   if (!context) return nullptr;
   auto* game = at<const std::byte*>(context, layout.globalGame);
   return game ? at<const std::byte*>(game, layout.gameView) : nullptr;
}

// The widget a GameView member points at, or null; shown only while it is visible.
const Widget* shownMember(const std::byte* view, uint32_t offset) {
   const Widget* widget = view ? at<const Widget*>(view, offset) : nullptr;
   return widget && visible(widget) ? widget : nullptr;
}

const Widget* shownBar(const std::byte* bottom, uint32_t targeter) {
   const Widget* bar = fromTargeter(bottom, targeter);
   return bar && visible(bar) ? bar : nullptr;
}

} // namespace

const Widget* sideMenu() {
   const std::byte* view = gameView();
   return view ? at<const Widget*>(view, layout.gameViewSideMenu) : nullptr;
}

ResearchBox researchBox() {
   const Widget* button = shownMember(gameView(), layout.gameViewResearch);
   if (!button) return {};
   const Widget* progress = visible(member(button, layout.researchProgressFlow))
                                ? member(button, layout.researchProgressLabel)
                                : nullptr;
   return {button, member(button, layout.researchTitle), progress};
}

std::vector<AlertButton> alertButtons() {
   std::vector<AlertButton> buttons;
   const std::byte* view = gameView();
   if (!view) return buttons;
   const auto& guis = at<MsvcVector<const Widget* const>>(view, layout.gameViewAlerts);
   for (const Widget* const* it = guis.first; it != guis.last; ++it)
      if (visible(*it))
         buttons.push_back({member(*it, layout.alertGuiButton), at<AlertCategory>(*it, layout.alertGuiCategory)});
   return buttons;
}

double iconButtonCount(const Widget* button) {
   return at<double>(asBaseChecked(button, ".?AVIconButtonWithNumber@@"), layout.iconButtonCount);
}

AlertsWindow alertsWindow() {
   const Widget* window = shownMember(gameView(), layout.gameViewAlertsOverview);
   if (!window) return {};
   AlertsWindow alerts{window, at<AlertCategory>(window, layout.alertsOverviewCategory), {}};
   std::vector<const Widget*> items = listBoxItems(member(window, layout.alertsOverviewList));
   std::span<const Widget* const> pins = children(member(window, layout.alertsOverviewPins));
   // The list and the pin column are filled together, a pin per row.
   for (size_t i = 0; i < items.size(); ++i) {
      const Widget* pin = i < pins.size() ? pins[i] : nullptr;
      alerts.rows.push_back({items[i], pin && derivesFrom(pin, "agui::IconButton") ? pin : nullptr});
   }
   return alerts;
}

ChartSearchResults chartSearchResults() {
   const Widget* window = shownMember(gameView(), layout.gameViewChartSearch);
   if (!window) return {};
   ChartSearchResults results{window, {}};
   std::vector<const Widget*> items = listBoxItems(member(window, layout.chartSearchList));
   std::span<const Widget* const> pins = children(member(window, layout.chartSearchPins));
   // The list and the pin column are filled together, a pin per row.
   for (size_t i = 0; i < items.size(); ++i)
      results.rows.push_back({items[i], i < pins.size() ? pins[i] : nullptr});
   return results;
}

const Widget* mapViewOptions() { return shownMember(gameView(), layout.gameViewMapViewOptions); }

const Widget* goalLabel() {
   const Widget* goal = shownMember(gameView(), layout.gameViewGoal);
   return goal ? member(goal, layout.goalLabel) : nullptr;
}

HudBars hudBars() {
   const std::byte* view = gameView();
   const auto* bottom = view ? at<const std::byte*>(view, layout.gameViewBottom) : nullptr;
   if (!bottom) return {};
   return {shownBar(bottom, layout.bottomHealthBar), shownBar(bottom, layout.bottomShieldBar),
           shownBar(bottom, layout.bottomVehicleHealthBar), shownBar(bottom, layout.bottomVehicleShieldBar),
           shownBar(bottom, layout.bottomMiningBar)};
}

bool gameWindowOpen() {
   const std::byte* view = gameView();
   // A GameGui*, which is not where its window's Widget starts.
   return view && at<const void*>(view, layout.gameViewActiveWindow);
}

Factoriopedia factoriopedia() {
   const Widget* window = shownMember(gameView(), layout.gameViewFactoriopedia);
   if (!window) return {};
   return {window,
           member(window, layout.frameHeader),
           member(window, layout.factoriopediaList),
           member(window, layout.factoriopediaSubheader),
           member(window, layout.factoriopediaPage),
           member(window, layout.factoriopediaUnresearched),
           at<bool>(window, layout.factoriopediaPinned)};
}

namespace {

// TagType values: the icons RichTextHoverManager::handleHover gives a tooltip and a click, from
// SpecialItem to SpacePlatform. Gps is left out: it needs the console line it was posted in.
constexpr uint32_t kTagSpecialItem = 0x9;
constexpr uint32_t kTagGps = 0xb;
constexpr uint32_t kTagSpacePlatform = 0x27;

// The hover manager of a label whose rich text is hoverable, or null for any other label.
std::byte* hoverManager(const Widget* label) {
   const std::byte* hoverable = asBase(label, ".?AVLabelWithHoverableRichText@@");
   return hoverable ? const_cast<std::byte*>(hoverable + layout.hoverableLabelManager) : nullptr;
}

// The label's text sections, laid out as it draws them; empty without rich text.
std::span<const std::byte> richTextSections(const Widget* label) {
   const auto* text = at<const std::byte*>(asBaseChecked(label, ".?AVLabel@agui@@"), layout.labelRichText);
   if (!text) return {};
   const auto* begin = at<const std::byte*>(text, layout.richTextSectionsBegin);
   const auto* end = at<const std::byte*>(text, layout.richTextSectionsEnd);
   return {begin, end};
}

const std::byte* richTextSection(const Widget* label, size_t section) {
   std::span<const std::byte> sections = richTextSections(label);
   size_t offset = section * layout.richTextSectionSize;
   return offset + layout.richTextSectionSize <= sections.size() ? sections.data() + offset : nullptr;
}

void handleHover(std::byte* manager, const std::byte* section, bool clicked) {
   using ClearTooltip = void (*)(std::byte*);
   using HandleHover = void (*)(std::byte*, const std::byte*, const void* consoleItem, bool);
   reinterpret_cast<ClearTooltip>(layout.richTextClearTooltip)(manager);
   reinterpret_cast<HandleHover>(layout.richTextHandleHover)(manager, section, nullptr, clicked);
}

} // namespace

std::vector<RichTextLink> richTextLinks(const Widget* label) {
   std::vector<RichTextLink> links;
   if (!hoverManager(label)) return links;
   std::span<const std::byte> sections = richTextSections(label);
   for (size_t offset = 0, index = 0; offset + layout.richTextSectionSize <= sections.size();
        offset += layout.richTextSectionSize, ++index) {
      const std::byte* section = sections.data() + offset;
      uint32_t type = at<uint32_t>(section, layout.richTextSectionType);
      std::string_view tag = at<std::string_view>(section, layout.richTextSectionTag);
      if (type >= kTagSpecialItem && type <= kTagSpacePlatform && type != kTagGps && !tag.empty())
         links.push_back({index, tag});
   }
   return links;
}

void clickRichTextLink(const Widget* label, size_t section) {
   std::byte* manager = hoverManager(label);
   const std::byte* drawn = manager ? richTextSection(label, section) : nullptr;
   if (drawn) handleHover(manager, drawn, true);
}

const Widget* hoverRichTextLink(const Widget* label, size_t section) {
   std::byte* manager = hoverManager(label);
   const std::byte* drawn = manager ? richTextSection(label, section) : nullptr;
   if (!drawn) return nullptr;
   handleHover(manager, drawn, false);
   const Widget* tooltip = fromTargeter(manager, layout.hoverManagerTooltip);
   if (tooltip) callVirtual<void>(tooltip, layout.slotToolTipUpdateContent);
   return tooltip;
}

void clearRichTextHover(const Widget* label) {
   if (std::byte* manager = hoverManager(label))
      reinterpret_cast<void (*)(std::byte*)>(layout.richTextClearTooltip)(manager);
}

TechnologyWindow technologyWindow() {
   const Widget* window = shownMember(gameView(), layout.gameViewTechnology);
   if (!window) return {};
   const Widget* list = member(window, layout.technologyList);
   return {window,
           member(window, layout.technologyQueue),
           member(window, layout.technologyTitle),
           member(window, layout.technologyStatus),
           member(window, layout.technologyFeatured),
           list,
           member(list, layout.technologyListTable),
           member(window, layout.technologyGraphTitle),
           member(window, layout.technologyGraphHolder),
           member(window, layout.technologyGraph)};
}

namespace {

// Technology::ResearchState, from the PDB: an enum's values are not resolvable by name.
enum class ResearchState : uint8_t { Disabled, Researched, Available, ConditionallyAvailable, NotAvailable };

// TechnologyGraphGui::Vertex::Type, from the PDB: a technology's button, a routing point of an edge
// that spans layers, or the button standing for the technologies the view leaves out.
constexpr uint32_t kVertexFull = 0;
constexpr uint32_t kVertexDummy = 1;

// std::deque's elements per block for a 2-byte element, a constant of MSVC's library.
constexpr size_t kDequeBlock = 8;

uint16_t technologyId(const std::byte* reference) { return at<uint16_t>(reference, layout.techReferenceId); }

const std::byte* technologyOf(const std::byte* reference) {
   return reinterpret_cast<const std::byte* (*)(const std::byte*)>(layout.getTechnology)(reference);
}

// The text of a LocalisedString in the game's current locale; destroys the string.
std::string takeTranslation(void* localised) {
   struct Destroy {
      void* object;
      ~Destroy() { reinterpret_cast<void (*)(void*)>(layout.localisedStringDestroy)(object); }
   } destroy{localised};
   using Str = const MsvcString* (*)(const void* localisedString, const void* localeProvider);
   const MsvcString* text = reinterpret_cast<Str>(layout.localisedStringStr)(localised, nullptr);
   return {text->capacity >= sizeof(text->buffer) ? text->pointer : text->buffer, text->size};
}

// A key of the game's locale in its current language, e.g. "gui-technology-preview.status-queued".
std::string translateKey(const char* key) {
   alignas(8) std::byte localised[256];
   if (layout.localisedStringSize > sizeof(localised)) return {};
   reinterpret_cast<void* (*)(void*, const char*)>(layout.localisedStringFromKey)(localised, key);
   return takeTranslation(localised);
}

std::string nameWithLevel(const std::byte* prototype, unsigned level) {
   alignas(8) std::byte localised[256];
   if (layout.localisedStringSize > sizeof(localised)) return {};
   reinterpret_cast<void* (*)(const std::byte*, void*, unsigned)>(layout.technologyNameWithLevel)(prototype, localised,
                                                                                                  level);
   return takeTranslation(localised);
}

// The 1-based place of the technology in a research queue, or 0.
unsigned queuePosition(const std::byte* queue, uint16_t id) {
   auto* const* map = at<const uint16_t* const*>(queue, layout.researchQueueMap);
   size_t mapSize = at<size_t>(queue, layout.researchQueueMapSize);
   size_t first = at<size_t>(queue, layout.researchQueueOffset);
   size_t size = at<size_t>(queue, layout.researchQueueSize);
   for (size_t i = 0; i < size; ++i) {
      size_t place = first + i;
      if (map[(place / kDequeBlock) & (mapSize - 1)][place % kDequeBlock] == id) return static_cast<unsigned>(i + 1);
   }
   return 0;
}

// The complete object a polymorphic subobject belongs to, by its vtable's RTTI locator.
const Widget* completeObject(const std::byte* subobject) {
   auto* vtable = *reinterpret_cast<const void* const* const*>(subobject);
   auto* locator = static_cast<const CompleteObjectLocator*>(vtable[-1]);
   return reinterpret_cast<const Widget*>(subobject - locator->offset);
}

} // namespace

std::vector<QueueEntry> researchQueueEntries(const Widget* queue) {
   std::vector<QueueEntry> entries;
   for (const Widget* element : children(member(queue, layout.queueTable))) {
      if (!derivesFrom(element, "TechnologyQueueElement")) continue;
      const Widget* slot = member(element, layout.queueElementSlot);
      if (technologyId(reinterpret_cast<const std::byte*>(slot) + layout.techSlotTechnology) != 0)
         entries.push_back({slot, member(element, layout.queueElementCancel)});
   }
   return entries;
}

bool isTechnologySlot(const Widget* widget) { return asBase(widget, ".?AVTechnologySlot@@") != nullptr; }

TechnologyInfo technologyInfo(const Widget* slot) {
   const std::byte* self = asBaseChecked(slot, ".?AVTechnologySlot@@");
   const std::byte* reference = self + layout.techSlotTechnology;
   TechnologyInfo info;
   info.id = technologyId(reference);
   if (info.id == 0) return info;
   const std::byte* technology = technologyOf(reference);
   auto level = reinterpret_cast<unsigned (*)(const std::byte*)>(layout.techSlotLevel)(self);
   info.name = nameWithLevel(at<const std::byte*>(technology, layout.technologyPrototype), level);

   const std::byte* queue = at<const std::byte*>(self, layout.techSlotResearchQueue);
   info.queuePosition = queue ? queuePosition(queue, info.id) : 0;
   auto state = reinterpret_cast<ResearchState (*)(const std::byte*, const std::byte*)>(layout.technologyState)(
       technology, queue);
   // The words of the selected technology's status (TechnologyGui::updateTitle), the research going
   // on as the research box words it.
   const char* key = "gui-technology-preview.status-not-available";
   if (info.queuePosition == 1)
      key = "gui-technology-preview.status-researching";
   else if (info.queuePosition > 1)
      key = "gui-technology-preview.status-queued";
   else if (state == ResearchState::Disabled)
      key = "gui-technology-preview.status-disabled";
   else if (state == ResearchState::Researched)
      key = "gui-technology-preview.status-researched";
   else if (state == ResearchState::Available || state == ResearchState::ConditionallyAvailable)
      key = "gui-technology-preview.status-available";
   info.status = translateKey(key);

   const std::byte* manager = at<const std::byte*>(self, layout.techSlotResearchManager);
   if (manager && at<uint32_t>(self, layout.techSlotIndicateProgress) != 0 && state != ResearchState::Researched)
      info.progress = reinterpret_cast<double (*)(const std::byte*, const std::byte*)>(layout.researchProgress)(
          manager, technology);
   return info;
}

TechnologyGraph technologyGraph(const Widget* graph) {
   TechnologyGraph result;
   const auto& vertices = at<MsvcVector<const std::byte* const>>(graph, layout.graphVertices);
   auto type = [](const std::byte* vertex) { return at<uint32_t>(vertex, layout.vertexType); };
   auto edges = [](const std::byte* vertex, uint32_t offset) {
      const auto& list = at<MsvcVector<const std::byte* const>>(vertex, offset);
      return std::span<const std::byte* const>(list.first, list.last);
   };
   // Through the routing vertices of an edge that spans layers, one in each, to its far end.
   auto end = [&](const std::byte* vertex, uint32_t offset) {
      while (vertex && type(vertex) == kVertexDummy) {
         std::span<const std::byte* const> next = edges(vertex, offset);
         vertex = next.empty() ? nullptr : next.front();
      }
      return vertex;
   };

   std::unordered_map<const std::byte*, size_t> index;
   for (const std::byte* const* it = vertices.first; it != vertices.last; ++it) {
      const std::byte* vertex = *it;
      if (type(vertex) == kVertexDummy) continue;
      TechnologyVertex node;
      node.button = completeObject(at<const std::byte*>(vertex, layout.vertexSlot));
      node.layer = at<uint32_t>(vertex, layout.vertexLayer);
      node.x = at<int32_t>(vertex, layout.vertexX);
      if (type(vertex) == kVertexFull) {
         node.technology = technologyId(vertex + layout.vertexTechnology);
      } else {
         node.omitted = at<uint32_t>(vertex, layout.vertexNumOmitted);
         std::span<const std::byte* const> from = edges(vertex, layout.vertexPredecessors);
         if (const std::byte* owner = from.empty() ? nullptr : end(from.front(), layout.vertexPredecessors))
            node.technology = technologyId(owner + layout.vertexTechnology);
      }
      index.emplace(vertex, result.vertices.size());
      result.vertices.push_back(std::move(node));
   }
   for (const auto& [vertex, self] : index) {
      for (const std::byte* up : edges(vertex, layout.vertexPredecessors))
         if (auto found = index.find(end(up, layout.vertexPredecessors)); found != index.end())
            result.vertices[self].prerequisites.push_back(found->second);
      for (const std::byte* down : edges(vertex, layout.vertexSuccessors))
         if (auto found = index.find(end(down, layout.vertexSuccessors)); found != index.end())
            result.vertices[self].unlocks.push_back(found->second);
   }
   if (auto found = index.find(at<const std::byte*>(graph, layout.graphCentral)); found != index.end())
      result.central = found->second;
   return result;
}

const Widget* sideMenuMuteButton(const Widget* sideMenu) { return pointerMember(sideMenu, layout.sideMenuMuteButton); }

const Widget* craftingQueue() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   if (!context) return nullptr;
   auto* game = at<const std::byte*>(context, layout.globalGame);
   if (!game) return nullptr;
   auto* view = at<const std::byte*>(game, layout.gameView);
   if (!view) return nullptr;
   auto* controllerView = at<std::byte*>(view, layout.gameViewControllerView);
   if (!controllerView) return nullptr;
   auto vtable = *reinterpret_cast<VirtualTable*>(controllerView);
   return reinterpret_cast<const Widget* (*)(void*)>(vtable[layout.controllerViewCraftingQueue])(controllerView);
}

std::vector<const Widget*> craftingQueueSlots(const Widget* craftingQueue) {
   const auto& slots = at<MsvcVector<const Widget* const>>(craftingQueue, layout.craftingQueueSlots);
   return {slots.first, slots.last};
}

CharacterQueue characterQueue(const Widget* characterInfo) {
   return {member(characterInfo, layout.characterInfoQueueLabel), member(characterInfo, layout.characterInfoQueue)};
}

std::vector<const Widget*> shortcutBarListCheckBoxes(const Widget* shortcutBar) {
   std::vector<const Widget*> boxes;
   if (!at<bool>(shortcutBar, layout.shortcutBarListOpen)) return boxes;
   const auto& rows = at<MsvcVector<const Widget* const>>(shortcutBar, layout.shortcutBarListRows);
   for (const Widget* const* it = rows.first; it != rows.last; ++it)
      // The list's search hides the rows it filters out.
      if (visible(*it)) boxes.push_back(member(*it, layout.shortcutRowCheckBox));
   return boxes;
}


bool isFocusable(const Widget* widget) { return callVirtual<bool>(widget, layout.slotIsFocusable); }

void focus(const Widget* widget) {
   // NamedBool<TabbedInTag> is a one-byte class passed by value, so it travels as a bool.
   callVirtual<void>(widget, layout.slotFocus, true);
}

namespace {

// Our string in the game's std::string layout, for calls that take std::string const& and copy
// it; the game never frees or grows it. Copying reads the terminator too, so `text` must stay alive
// and unchanged for the call.
MsvcString borrow(const std::string& text) {
   MsvcString view{};
   view.size = text.size();
   if (text.size() < sizeof(view.buffer)) {
      std::memcpy(view.buffer, text.c_str(), text.size() + 1);
      view.capacity = sizeof(view.buffer) - 1;
   } else {
      view.pointer = text.c_str();
      view.capacity = text.size();
   }
   return view;
}

} // namespace

void setEnabled(const Widget* widget, bool enabled) {
   callVirtual<Widget*>(widget, layout.slotSetEnabled, enabled);
}

void setToolTip(const Widget* widget, const std::string& text) {
   MsvcString view = borrow(text);
   reinterpret_cast<Widget* (*)(const Widget*, const MsvcString*)>(layout.widgetSetToolTip)(widget, &view);
}

void setLabelText(const Widget* label, const std::string& text) {
   MsvcString view = borrow(text);
   reinterpret_cast<void (*)(const Widget*, const MsvcString*)>(layout.labelSetText)(label, &view);
}

void scrollIntoView(const Widget* widget) {
   using ScrollToVisible = void (*)(std::byte* pane, const Widget* widget, uint8_t mode);
   constexpr uint8_t kInView = 1; // ScrollMode::InView
   for (const Widget* ancestor = parent(widget); ancestor; ancestor = parent(ancestor)) {
      if (const std::byte* pane = asBase(ancestor, ".?AVScrollPane@agui@@")) {
         reinterpret_cast<ScrollToVisible>(layout.scrollToVisible)(const_cast<std::byte*>(pane), widget, kInView);
         return;
      }
   }
}

void press(const Widget* widget, MouseButton button, bool shift, bool control) {
   pressOver(widget, widget, button, shift, control);
}

namespace {

// Whether `widget` hangs in the tree under `root`, found by walking down from the root: the widget
// itself is never read, so a freed one is safe to ask about.
bool inTree(const Widget* root, const Widget* widget, int depth = 0) {
   if (root == widget) return true;
   if (depth > 64) return false;
   for (const Widget* child : children(root))
      if (inTree(child, widget, depth + 1)) return true;
   for (const Widget* child : privateChildren(root))
      if (inTree(child, widget, depth + 1)) return true;
   return false;
}

// The widget a press is replaying a click on, while it runs. Presses run on the main thread, inside
// a Gui's logic.
thread_local const Widget* t_pressed = nullptr;

struct UnderMouse {
   bool overGui;
   const Widget* widget;
};
using DetermineWidgetUnderMouse = UnderMouse* (*)(UnderMouse* out);
DetermineWidgetUnderMouse g_underMouseOriginal = nullptr;

UnderMouse* detourUnderMouse(UnderMouse* out) {
   if (t_pressed) {
      *out = {true, t_pressed};
      return out;
   }
   // The game ignores the world wherever the real mouse rests on a GUI: no hover or map selection,
   // no drag building, selection tools or world controls. While the mod drives the cursor, the
   // mouse is nowhere.
   if (world::drivesCursor()) {
      *out = {false, nullptr};
      return out;
   }
   return g_underMouseOriginal(out);
}

} // namespace

void* underMouseDetour() { return reinterpret_cast<void*>(&detourUnderMouse); }

void** underMouseOriginal() { return reinterpret_cast<void**>(&g_underMouseOriginal); }

namespace {

struct Pair {
   int32_t x, y;
};

// The press at `point`, in `widget`'s coordinates, the mouse over `over` (the widget or one inside).
void pressInside(const Widget* widget, const Widget* over, Pair point, MouseButton button, bool shift, bool control) {
   using Dispatch = void (*)(const Widget* widget, const void* event);
   // agui::MouseButton and agui::MouseEvent::Type
   constexpr uint16_t kLeft = 2, kRight = 4, kMiddle = 8;
   constexpr uint32_t kDown = 1, kUp = 2, kClick = 4, kEnter = 10, kLeave = 11;
   alignas(16) std::byte event[128] = {};
   if (layout.mouseEventSize > sizeof(event)) return;
   auto put = [&event](uint32_t offset, auto value) { std::memcpy(event + offset, &value, sizeof(value)); };
   put(layout.mouseEventPosition, point.x);
   put(layout.mouseEventPosition + 4, point.y);
   put(layout.mouseEventButton, button == MouseButton::Left ? kLeft : button == MouseButton::Right ? kRight : kMiddle);
   put(layout.mouseEventShift, shift);
   put(layout.mouseEventControl, control);
   put(layout.mouseEventSource, reinterpret_cast<uintptr_t>(widget));
   // A handler may close the window and free the widget (Mod settings in the map generator does).
   // The Gui holds the widget under the mouse through a targeter that clears itself then, so it
   // sends nothing more; we look the widget up in the tree again, never touching it, and stop the
   // same way once it is gone.
   const Gui* gui = applicationGui();
   const Widget* root = gui ? baseWidget(gui) : nullptr;
   bool tracked = root && inTree(root, widget);
   bool clickOnPress = at<uint32_t>(widget, layout.widgetUsageBits) & game::kUsageClickOnMouseDown;
   auto send = [&](uintptr_t dispatcher, uint32_t type) {
      if (tracked && !inTree(root, widget)) return false;
      put(layout.mouseEventType, type);
      reinterpret_cast<Dispatch>(dispatcher)(widget, event);
      return true;
   };

   // Handlers that ask whether a control is held (crafting checks craft, craft5 and craftAll) read
   // the mouse buttons from the InputState, not the event: hold the button down there until the
   // release, as a real click would. A handler that acts on the press also marks the button used
   // by its control (an inventory slot by cursor-transfer), which blocks every other control on
   // the button until InputState::setMouseUp clears it on the real release; the release puts the
   // block back as it was.
   struct HeldButton {
      uint32_t* buttons = nullptr;
      uint32_t saved = 0;
      std::byte* block = nullptr;
      std::byte savedBlock[32] = {};
      void release() {
         if (buttons) *buttons = saved;
         if (block) std::memcpy(block, savedBlock, layout.mouseBlockSize);
         buttons = nullptr;
         block = nullptr;
      }
      ~HeldButton() { release(); }
   } held;

   // Those handlers also ask whether the mouse is over a GUI, which a real click on the widget is.
   struct Pressing {
      const Widget* saved;
      ~Pressing() { t_pressed = saved; }
   } pressing{std::exchange(t_pressed, over)};

   // A widget the real mouse rests on is hovered already, and must stay so.
   bool hover = gui && widgetUnderMouse(gui) != widget;
   if (hover && !send(layout.dispatchMouseEnter, kEnter)) return;
   // Entered with no button down, so a slot starts no drag.
   auto* context = *reinterpret_cast<std::byte* const*>(layout.globalContext);
   if (auto* state = context ? at<std::byte*>(context, layout.globalInputState) : nullptr) {
      // The game's own numbering, not SDL's (Event::convertSDLMouseButton): left 1, right 2, middle
      // 3, each held as bit n-1. "mouse-button-2" in a binding is the right button.
      uint32_t index = button == MouseButton::Left ? 0 : button == MouseButton::Right ? 1 : 2;
      held.buttons = reinterpret_cast<uint32_t*>(state + layout.inputStateMouseButtons);
      held.saved = *held.buttons;
      *held.buttons |= 1u << index;
      if (layout.mouseBlockSize <= sizeof(held.savedBlock)) {
         held.block = state + layout.inputStateMouseBlocks + index * layout.mouseBlockSize;
         std::memcpy(held.savedBlock, held.block, layout.mouseBlockSize);
      }
   }
   if (!send(layout.dispatchMouseDown, kDown)) return;
   // A click-on-press widget clicked inside dispatchMouseDown; the Gui sends no second click.
   if (!clickOnPress && !send(layout.dispatchClick, kClick)) return;
   held.release();
   if (!send(layout.dispatchMouseUp, kUp)) return;
   if (hover) send(layout.dispatchMouseLeave, kLeave);
}

} // namespace

void pressOver(const Widget* widget, const Widget* over, MouseButton button, bool shift, bool control) {
   // The centre of `over`, in `widget`'s coordinates: locations are relative to the parent.
   Pair size = at<Pair>(over, layout.widgetSize);
   Pair point = {size.x / 2, size.y / 2};
   for (const Widget* step = over; step && step != widget; step = parent(step)) {
      Pair location = at<Pair>(step, layout.widgetLocation);
      point.x += location.x;
      point.y += location.y;
   }
   pressInside(widget, over, point, button, shift, control);
}

void pressAt(const Widget* widget, int x, int y, MouseButton button, bool shift, bool control) {
   pressInside(widget, widget, {x, y}, button, shift, control);
}

const std::byte* objectAsBase(const void* object, std::string_view decoratedBase) {
   return asBase(static_cast<const Widget*>(object), decoratedBase);
}

const Widget* openDropDown(const Gui* gui) {
   // The top modal only: a dropdown list under a dialog is not the one being used.
   const auto& modals = at<MsvcVector<const std::byte>>(gui, layout.guiModals);
   for (const std::byte* entry = modals.last; entry > modals.first;) {
      entry -= layout.modalSize;
      const Widget* widget = fromTargeter(entry, layout.modalWidget);
      if (!widget) continue;
      if (!at<bool>(entry, layout.modalIsDropDown)) return nullptr;
      // The list is a member of its DropDown, so the DropDown sits just before it.
      const auto* dropDown = reinterpret_cast<const Widget*>(
         reinterpret_cast<const std::byte*>(asBaseChecked(widget, ".?AVListBox@agui@@")) - layout.dropDownList);
      return asBase(dropDown, ".?AVDropDown@agui@@") ? dropDown : nullptr;
   }
   return nullptr;
}

const Widget* dropDownList(const Widget* dropDown) {
   return reinterpret_cast<const Widget*>(asBaseChecked(dropDown, ".?AVDropDown@agui@@") + layout.dropDownList);
}

int dropDownSelected(const Widget* dropDown) {
   return at<int32_t>(asBaseChecked(dropDown, ".?AVDropDown@agui@@"), layout.dropDownSelected);
}

bool sendKey(const Widget* widget, Key key, bool release) {
   alignas(16) std::byte event[128] = {};
   if (layout.keyEventSize > sizeof(event)) return false;
   KeyCodes codes = codesOf(key);
   auto put = [&event](uint32_t offset, auto value) { std::memcpy(event + offset, &value, sizeof(value)); };
   put(layout.keyEventUnichar, codes.unichar);
   put(layout.keyEventKeyCode, codes.keyCode);
   put(layout.keyEventExtKey, codes.extKey);
   put(layout.keyEventKey, codes.key);
   put(layout.keyEventSource, reinterpret_cast<uintptr_t>(widget));
   bool handled = callVirtual<bool>(widget, layout.slotKeyDown, static_cast<const void*>(event));
   if (release) callVirtual<bool>(widget, layout.slotKeyUp, static_cast<const void*>(event));
   return handled;
}

} // namespace fa::agui
