#include "agui.h"

#include "game.h"

#include <windows.h>

#include <dbghelp.h>

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
   if (const std::byte* inventory = at<const std::byte*>(self, layout.slotInventory)) {
      uint16_t index = at<uint16_t>(self, layout.slotIndex);
      if (index >= at<uint16_t>(inventory, layout.inventorySize)) return item;
      stack = at<const std::byte*>(inventory, layout.inventoryData) + size_t{index} * layout.itemStackSize;
   }
   if (!stack) return item;
   item.count = at<uint32_t>(stack, layout.itemStackCount);
   if (item.count == 0) return item;
   if (const std::byte* prototype = prototypeAt(layout.itemPrototypes, at<uint16_t>(stack, layout.itemStackItem)))
      item.name = localisedName(prototype);
   if (const std::byte* quality = prototypeAt(layout.qualityPrototypes, at<uint8_t>(stack, layout.itemStackQuality));
       quality && readString(quality, layout.prototypeName) != "normal")
      item.quality = localisedName(quality);
   return item;
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
      uint32_t progressBar, bonusBar, recipe, outputs, changeRecipe;
   };
   for (const Machine& machine :
        {Machine{".?AVAssemblingMachineGui@@", layout.assemblerProgressBar, layout.assemblerBonusBar,
                 layout.assemblerRecipe, layout.assemblerOutputs, layout.assemblerChangeRecipe},
         Machine{".?AVFurnaceGui@@", layout.furnaceProgressBar, layout.furnaceBonusBar, layout.furnaceRecipe,
                 layout.furnaceOutputs, 0},
         Machine{".?AVMiningDrillGui@@", layout.drillProgressBar, layout.drillBonusBar, 0, 0, 0}}) {
      const std::byte* base = asBase(window, machine.type);
      if (!base) continue;
      auto widgetAt = [base](uint32_t offset) {
         return offset ? reinterpret_cast<const Widget*>(base + offset) : nullptr;
      };
      parts.progressBar = widgetAt(machine.progressBar);
      parts.bonusBar = widgetAt(machine.bonusBar);
      parts.recipe = widgetAt(machine.recipe);
      parts.outputs = widgetAt(machine.outputs);
      parts.changeRecipe = widgetAt(machine.changeRecipe);
   }
   return parts;
}

BurnerParts burnerParts(const Widget* burnerInfo) {
   return {member(burnerInfo, layout.burnerSlots), member(burnerInfo, layout.burnerProgressBar)};
}

unsigned selectedRow(const Widget* table) {
   return at<uint32_t>(asBaseChecked(table, ".?AVTableWithSelection@agui@@"), layout.tableSelectedIndex);
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

} // namespace

void pressOver(const Widget* widget, const Widget* over, MouseButton button, bool shift, bool control) {
   using Dispatch = void (*)(const Widget* widget, const void* event);
   // agui::MouseButton and agui::MouseEvent::Type
   constexpr uint16_t kLeft = 2, kRight = 4, kMiddle = 8;
   constexpr uint32_t kDown = 1, kUp = 2, kClick = 4, kEnter = 10, kLeave = 11;
   alignas(16) std::byte event[128] = {};
   if (layout.mouseEventSize > sizeof(event)) return;
   auto put = [&event](uint32_t offset, auto value) { std::memcpy(event + offset, &value, sizeof(value)); };
   struct Pair {
      int32_t x, y;
   };
   // The centre of `over`, in `widget`'s coordinates: locations are relative to the parent.
   Pair size = at<Pair>(over, layout.widgetSize);
   Pair point = {size.x / 2, size.y / 2};
   for (const Widget* step = over; step && step != widget; step = parent(step)) {
      Pair location = at<Pair>(step, layout.widgetLocation);
      point.x += location.x;
      point.y += location.y;
   }
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
   // release, as a real click would.
   struct HeldButton {
      uint32_t* buttons;
      uint32_t saved;
      void release() {
         if (buttons) *buttons = saved;
         buttons = nullptr;
      }
      ~HeldButton() { release(); }
   } held{nullptr, 0};

   // A widget the real mouse rests on is hovered already, and must stay so.
   bool hover = gui && widgetUnderMouse(gui) != widget;
   if (hover && !send(layout.dispatchMouseEnter, kEnter)) return;
   // Entered with no button down, so a slot starts no drag.
   auto* context = *reinterpret_cast<std::byte* const*>(layout.globalContext);
   if (auto* state = context ? at<std::byte*>(context, layout.globalInputState) : nullptr) {
      // SDL_BUTTON_LMASK, SDL_BUTTON_MMASK, SDL_BUTTON_RMASK
      uint32_t mask = button == MouseButton::Left ? 1 : button == MouseButton::Middle ? 2 : 4;
      held.buttons = reinterpret_cast<uint32_t*>(state + layout.inputStateMouseButtons);
      held.saved = *held.buttons;
      *held.buttons |= mask;
   }
   if (!send(layout.dispatchMouseDown, kDown)) return;
   // A click-on-press widget clicked inside dispatchMouseDown; the Gui sends no second click.
   if (!clickOnPress && !send(layout.dispatchClick, kClick)) return;
   held.release();
   if (!send(layout.dispatchMouseUp, kUp)) return;
   if (hover) send(layout.dispatchMouseLeave, kLeave);
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
