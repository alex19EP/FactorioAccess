#include "agui.h"

#include "game.h"

#include <windows.h>

#include <dbghelp.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>

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

struct MsvcVector {
   const Widget* const* first;
   const Widget* const* last;
   const Widget* const* end;
};

const Widget* fromTargeter(const void* owner, uint32_t targeterOffset) {
   auto* targetable = at<const std::byte*>(owner, targeterOffset + layout.targeterTarget);
   if (!targetable) return nullptr;
   return reinterpret_cast<const Widget*>(targetable - layout.widgetTargetable);
}

// x64 MSVC RTTI: vtable[-1] is the complete object locator, whose type descriptor holds the
// decorated name, e.g. ".?AVTextButton@agui@@".
struct CompleteObjectLocator {
   DWORD signature;
   DWORD offset;
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

constexpr std::string_view kLabelTypeName = ".?AVLabel@agui@@";

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

struct ClassInfo {
   std::string name;
   // Where the agui::Label part sits, for any class derived from it.
   std::optional<int32_t> labelOffset;
};

const ClassInfo& classInfo(const Widget* widget) {
   static std::unordered_map<const void*, ClassInfo> cache;
   auto* vtable = *reinterpret_cast<const void* const* const*>(widget);
   auto [it, inserted] = cache.try_emplace(vtable);
   if (!inserted) return it->second;

   auto* locator = static_cast<const CompleteObjectLocator*>(vtable[-1]);
   auto imageBase = reinterpret_cast<uintptr_t>(locator) - locator->self;
   auto typeAt = [imageBase](DWORD rva) { return reinterpret_cast<const TypeDescriptor*>(imageBase + rva); };
   it->second.name = undecorate(typeAt(locator->typeDescriptor)->name);

   auto* hierarchy = reinterpret_cast<const ClassHierarchyDescriptor*>(imageBase + locator->classDescriptor);
   auto* bases = reinterpret_cast<const DWORD*>(imageBase + hierarchy->baseArray);
   for (DWORD i = 0; i < hierarchy->baseCount; ++i) {
      auto* base = reinterpret_cast<const BaseClassDescriptor*>(imageBase + bases[i]);
      if (base->vbtableDisplacement == -1 && typeAt(base->typeDescriptor)->name == kLabelTypeName) {
         it->second.labelOffset = base->memberDisplacement;
         break;
      }
   }
   return it->second;
}

std::string_view readString(const void* object, uint32_t offset) {
   const auto& string = at<MsvcString>(object, offset);
   return {string.capacity >= sizeof(string.buffer) ? string.pointer : string.buffer, string.size};
}

} // namespace

const Gui* instance() { return *reinterpret_cast<const Gui* const*>(layout.guiInstance); }

const Widget* baseWidget(const Gui* gui) { return at<const Widget*>(gui, layout.guiBaseWidget); }

const Widget* focusedWidget(const Gui* gui) { return fromTargeter(gui, layout.guiFocusedWidget); }

const Widget* widgetUnderMouse(const Gui* gui) { return fromTargeter(gui, layout.guiWidgetUnderMouse); }

const Widget* parent(const Widget* widget) { return at<const Widget*>(widget, layout.widgetParent); }

std::span<const Widget* const> children(const Widget* widget) {
   const auto& vector = at<MsvcVector>(widget, layout.widgetChildren);
   return {vector.first, vector.last};
}

std::span<const Widget* const> privateChildren(const Widget* widget) {
   const auto& vector = at<MsvcVector>(widget, layout.widgetPrivateChildren);
   return {vector.first, vector.last};
}

std::string_view text(const Widget* widget) {
   if (auto labelOffset = classInfo(widget).labelOffset) {
      return readString(reinterpret_cast<const std::byte*>(widget) + *labelOffset, layout.labelText);
   }
   return readString(widget, layout.widgetText);
}

bool visible(const Widget* widget) {
   return (at<uint32_t>(widget, layout.widgetUsageBits) & game::kUsageHiddenMask) == game::kUsageVisible;
}

bool enabled(const Widget* widget) { return (at<uint32_t>(widget, layout.widgetUsageBits) & game::kUsageEnabled) != 0; }

const std::string& className(const Widget* widget) { return classInfo(widget).name; }

} // namespace fa::agui
