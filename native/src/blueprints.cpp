#include "blueprints.h"

#include "agui.h"
#include "game.h"
#include "text.h"
#include "vocab.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <tuple>

namespace fa::blueprints {

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

std::string_view view(const MsvcString& string) {
   return {string.capacity >= sizeof(string.buffer) ? string.pointer : string.buffer, string.size};
}

template <class T>
struct MsvcVector {
   T* first;
   T* last;
   T* end;
};

using LocalisedStr = const MsvcString* (*)(const void* localisedString, const void* localeProvider);
using SignalPrototypeFunction = const std::byte* (*)(const void* signal);

// What a slot shows at most over the item's icon (PreviewIcons::draw, the planners' getIcons).
constexpr size_t kIconsShown = 4;
// How deep a book without icons is followed into the active book inside it, as BlueprintBook::draw
// recurses.
constexpr int kNestedBooks = 8;

// The prototype an ID indexes in a PrototypeList<T>::indexToPrototype vector, or null.
const std::byte* prototypeAt(uintptr_t list, size_t index) {
   const auto& prototypes = *reinterpret_cast<const MsvcVector<const std::byte* const>*>(list);
   return index < static_cast<size_t>(prototypes.last - prototypes.first) ? prototypes.first[index] : nullptr;
}

std::string localisedName(const std::byte* prototype) {
   const MsvcString* name =
      reinterpret_cast<LocalisedStr>(layout.localisedStringStr)(prototype + layout.prototypeLocalisedName, nullptr);
   return text::speakable(view(*name));
}

// A quality's name and a space before the name it qualifies, or nothing for normal quality or none.
std::string qualityPrefix(uint8_t quality) {
   const std::byte* prototype = quality ? prototypeAt(layout.qualityPrototypes, quality) : nullptr;
   if (!prototype || view(at<MsvcString>(prototype, layout.prototypeName)) == "normal") return {};
   return localisedName(prototype) + " ";
}

std::string named(uintptr_t list, uint16_t id, uint8_t quality) {
   const std::byte* prototype = id ? prototypeAt(list, id) : nullptr;
   return prototype ? qualityPrefix(quality) + localisedName(prototype) : std::string();
}

// A SignalID (an item, fluid, virtual signal, entity, recipe and so on), empty when unset. The
// game takes a signal as set when the index in its low 16 bits is.
std::string signalName(const std::byte* signal) {
   if ((at<uint32_t>(signal, 0) & 0xffff) == 0) return {};
   const std::byte* prototype = reinterpret_cast<SignalPrototypeFunction>(layout.signalPrototype)(signal);
   if (!prototype) return {};
   return qualityPrefix(at<uint8_t>(signal, layout.signalQuality)) + localisedName(prototype);
}

// The icons the owner chose, a PreviewIcons' std::vector<SignalID>.
std::vector<std::string> chosenIcons(const std::byte* icons) {
   std::vector<std::string> names;
   const auto& signals = at<MsvcVector<const std::byte>>(icons, 0);
   for (const std::byte* signal = signals.first; signal < signals.last && names.size() < kIconsShown;
        signal += layout.signalSize)
      if (std::string name = signalName(signal); !name.empty()) names.push_back(std::move(name));
   return names;
}

std::string stringAt(const std::byte* object, uint32_t offset) {
   return text::speakable(view(at<MsvcString>(object, offset)));
}

std::optional<Shown> shownAt(const void* item, int depth);
std::optional<Shown> shownRecordAt(const void* record, const void* player, int depth);

// As BlueprintBook::draw: the book's own icons, else what its active item shows inside it.
std::vector<std::string> bookIcons(const std::byte* book, int depth) {
   const auto& own = at<MsvcVector<const std::byte>>(book, layout.bookIcons);
   if (own.first != own.last || depth >= kNestedBooks) return chosenIcons(book + layout.bookIcons);
   const std::byte* inventory = book + layout.bookInventory;
   uint16_t active = at<uint16_t>(book, layout.bookActiveIndex);
   if (active >= at<uint16_t>(inventory, layout.inventorySize)) return {};
   const std::byte* stack =
      at<const std::byte*>(inventory, layout.inventoryData) + size_t{active} * layout.itemStackSize;
   const void* inner = at<const void*>(stack, layout.itemStackData);
   std::optional<Shown> drawn = inner ? shownAt(inner, depth + 1) : std::nullopt;
   return drawn ? std::move(drawn->icons) : std::vector<std::string>();
}

// As BlueprintBookRecord::draw: the book's own icons, else what the record the player builds from
// shows.
std::vector<std::string> bookRecordIcons(const std::byte* book, const void* player, int depth) {
   const auto& own = at<MsvcVector<const std::byte>>(book, layout.bookRecordIcons);
   if (own.first != own.last || depth >= kNestedBooks) return chosenIcons(book + layout.bookRecordIcons);
   const auto& records = at<MsvcVector<const void* const>>(book, layout.bookRecordRecords);
   uint16_t active = agui::bookRecordActiveIndex(book, player);
   if (active >= static_cast<size_t>(records.last - records.first) || !records.first[active]) return {};
   std::optional<Shown> drawn = shownRecordAt(records.first[active], player, depth + 1);
   return drawn ? std::move(drawn->icons) : std::vector<std::string>();
}

// As DeconstructionData::getIcons: a tree for trees and rocks only (crossed out as a blacklist),
// else the first entity filters, then tile filters, as far as the tile mode lets each in.
std::vector<std::string> deconIcons(const std::byte* planner) {
   if (at<bool>(planner, layout.deconDataTreesAndRocks)) {
      uint8_t mode = at<uint8_t>(planner, layout.deconDataEntityMode);
      if (mode == layout.entityFilterWhitelist) return {std::string(vocab::kTreesAndRocks)};
      if (mode == layout.entityFilterBlacklist) return {std::string(vocab::kNotTreesAndRocks)};
      return {};
   }
   std::vector<std::string> names;
   uint8_t tileMode = at<uint8_t>(planner, layout.deconDataTileMode);
   if (tileMode != layout.tileSelectionOnly) {
      const auto& filters = at<MsvcVector<const std::byte>>(planner, layout.deconDataEntities);
      for (const std::byte* filter = filters.first; filter < filters.last && names.size() < kIconsShown;
           filter += layout.entityFilterSize) {
         // The quality is drawn only when the filter asks for exactly one.
         bool exact = at<uint8_t>(filter, layout.entityFilterComparison) == layout.comparisonEquals;
         std::string name = named(layout.entityPrototypes, at<uint16_t>(filter, layout.entityFilterId),
                                  exact ? at<uint8_t>(filter, layout.entityFilterQuality) : 0);
         if (!name.empty()) names.push_back(std::move(name));
      }
   }
   if (tileMode != layout.tileSelectionNever) {
      const auto& tiles = at<MsvcVector<const uint16_t>>(planner, layout.deconDataTiles);
      for (const uint16_t* tile = tiles.first; tile < tiles.last && names.size() < kIconsShown; ++tile)
         if (std::string name = named(layout.tilePrototypes, *tile, 0); !name.empty()) names.push_back(std::move(name));
   }
   return names;
}

// As UpgradeData::getIcons: the distinct destinations of the rules with a source, in order.
std::vector<std::string> upgradeIcons(const std::byte* planner) {
   std::vector<std::string> names;
   std::vector<std::tuple<uint8_t, uint16_t, uint8_t>> seen;
   const auto& mappings = at<MsvcVector<const std::byte>>(planner, layout.upgradeDataMappings);
   for (const std::byte* mapping = mappings.first; mapping < mappings.last && names.size() < kIconsShown;
        mapping += layout.mappingSize) {
      if (at<uint16_t>(mapping, layout.mappingSourceId) == 0 &&
          at<uint8_t>(mapping, layout.mappingSourceQuality) == 0 &&
          at<uint16_t>(mapping, layout.mappingSourceEntity) == 0 &&
          at<uint8_t>(mapping, layout.mappingSourceEntityQuality) == 0)
         continue;
      std::tuple destination{at<uint8_t>(mapping, layout.mappingDestinationType),
                             at<uint16_t>(mapping, layout.mappingDestinationId),
                             at<uint8_t>(mapping, layout.mappingDestinationQuality)};
      auto [type, id, quality] = destination;
      if (id == 0 || std::ranges::find(seen, destination) != seen.end()) continue;
      seen.push_back(destination);
      uintptr_t list = type == layout.upgradeTypeEntity ? layout.entityPrototypes : layout.itemPrototypes;
      if (std::string name = named(list, id, quality); !name.empty()) names.push_back(std::move(name));
   }
   return names;
}

// A Blueprint, a DeconstructionData or an UpgradeData, held by an item or a library record.
Shown blueprintShown(const std::byte* blueprint) {
   return {
      {}, chosenIcons(blueprint + layout.blueprintDataIcons), stringAt(blueprint, layout.blueprintDataDescription)};
}

Shown deconShown(const std::byte* planner) {
   Shown result{{}, chosenIcons(planner + layout.deconDataIcons), stringAt(planner, layout.deconDataDescription)};
   if (result.icons.empty()) result.icons = deconIcons(planner);
   return result;
}

Shown upgradeShown(const std::byte* planner) {
   Shown result{{}, chosenIcons(planner + layout.upgradeDataIcons), stringAt(planner, layout.upgradeDataDescription)};
   if (result.icons.empty()) result.icons = upgradeIcons(planner);
   return result;
}

std::optional<Shown> shownAt(const void* item, int depth) {
   Shown result;
   const std::byte* object = nullptr;
   if ((object = agui::objectAsBase(item, ".?AVBlueprintItem@@"))) {
      result = blueprintShown(object + layout.blueprintItemBlueprint);
   } else if ((object = agui::objectAsBase(item, ".?AVBlueprintBook@@"))) {
      result.icons = bookIcons(object, depth);
      result.description = stringAt(object, layout.bookDescription);
   } else if ((object = agui::objectAsBase(item, ".?AVDeconstructionItem@@"))) {
      result = deconShown(object + layout.deconItemData);
   } else if ((object = agui::objectAsBase(item, ".?AVUpgradeItem@@"))) {
      result = upgradeShown(object + layout.upgradeItemData);
   } else {
      return std::nullopt;
   }
   result.label = stringAt(agui::objectAsBase(item, ".?AVItemWithLabel@@"), layout.itemLabel);
   return result;
}

std::optional<Shown> shownRecordAt(const void* record, const void* player, int depth) {
   Shown result;
   const std::byte* object = nullptr;
   if ((object = agui::objectAsBase(record, ".?AVSingleBlueprintRecord@@"))) {
      result = blueprintShown(object + layout.singleRecordBlueprint);
   } else if ((object = agui::objectAsBase(record, ".?AVBlueprintBookRecord@@"))) {
      result.icons = bookRecordIcons(object, player, depth);
      result.description = stringAt(object, layout.bookRecordDescription);
   } else if ((object = agui::objectAsBase(record, ".?AVDeconstructionRecord@@"))) {
      result = deconShown(object + layout.deconRecordData);
   } else if ((object = agui::objectAsBase(record, ".?AVUpgradeRecord@@"))) {
      result = upgradeShown(object + layout.upgradeRecordData);
   } else {
      return std::nullopt;
   }
   result.label = stringAt(agui::objectAsBase(record, ".?AVBlueprintRecord@@"), layout.recordLabel);
   return result;
}

} // namespace

std::optional<Shown> shown(const void* item) { return shownAt(item, 0); }

std::optional<Shown> shownRecord(const void* record, const void* player) { return shownRecordAt(record, player, 0); }

std::string recordItem(const void* record) {
   return named(layout.itemPrototypes,
                at<uint16_t>(agui::objectAsBase(record, ".?AVBlueprintRecord@@"), layout.recordItem), 0);
}

} // namespace fa::blueprints
