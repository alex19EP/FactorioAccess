#include "symbols.h"

#include "log.h"

#include <PDB.h>
#include "PDB_CoalescedMSFStream.h"
#include "PDB_DBIStream.h"
#include "PDB_ImageSectionStream.h"
#include "PDB_InfoStream.h"
#include "PDB_IPIStream.h"
#include "PDB_ModuleInfoStream.h"
#include "PDB_ModuleSymbolStream.h"
#include "PDB_PublicSymbolStream.h"
#include "PDB_RawFile.h"
#include "PDB_TPIStream.h"

#include <chrono>
#include <cstring>
#include <format>
#include <fstream>
#include <set>
#include <unordered_map>
#include <vector>

namespace fa::pdb {

namespace DBI = PDB::CodeView::DBI;
namespace IPI = PDB::CodeView::IPI;
namespace TPI = PDB::CodeView::TPI;
using DBI::SymbolRecordKind;
using TPI::TypeRecordKind;

namespace {

struct CodeViewPdb70 {
   DWORD signature;
   GUID guid;
   DWORD age;
   char path[1];
};

constexpr DWORD kRsds = 'SDSR';

bool failed(PDB::ErrorCode code) { return code != PDB::ErrorCode::Success; }

class MappedFile {
public:
   explicit MappedFile(const std::filesystem::path& path) {
      file_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr);
      if (file_ == INVALID_HANDLE_VALUE) return;
      LARGE_INTEGER size{};
      if (!GetFileSizeEx(file_, &size)) return;
      mapping_ = CreateFileMappingW(file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
      if (!mapping_) return;
      data_ = MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0);
      if (data_) size_ = static_cast<size_t>(size.QuadPart);
   }
   ~MappedFile() {
      if (data_) UnmapViewOfFile(data_);
      if (mapping_) CloseHandle(mapping_);
      if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_);
   }
   MappedFile(const MappedFile&) = delete;
   MappedFile& operator=(const MappedFile&) = delete;

   const void* data() const { return data_; }
   size_t size() const { return size_; }

private:
   HANDLE file_ = INVALID_HANDLE_VALUE;
   HANDLE mapping_ = nullptr;
   void* data_ = nullptr;
   size_t size_ = 0;
};

// CodeView numeric leaf: a value below LF_NUMERIC is stored inline, otherwise the kind is followed
// by the value. Returns the encoded size, or 0 for an encoding we do not read.
size_t readNumeric(const char* data, uint64_t& value) {
   auto kind = *reinterpret_cast<const TypeRecordKind*>(data);
   const char* payload = data + sizeof(TypeRecordKind);
   if (kind < TypeRecordKind::LF_NUMERIC) {
      value = static_cast<uint16_t>(kind);
      return sizeof(TypeRecordKind);
   }
   switch (kind) {
   case TypeRecordKind::LF_CHAR:
      value = static_cast<uint64_t>(*reinterpret_cast<const int8_t*>(payload));
      return sizeof(TypeRecordKind) + 1;
   case TypeRecordKind::LF_SHORT:
      value = static_cast<uint64_t>(*reinterpret_cast<const int16_t*>(payload));
      return sizeof(TypeRecordKind) + 2;
   case TypeRecordKind::LF_USHORT:
      value = *reinterpret_cast<const uint16_t*>(payload);
      return sizeof(TypeRecordKind) + 2;
   case TypeRecordKind::LF_LONG:
      value = static_cast<uint64_t>(*reinterpret_cast<const int32_t*>(payload));
      return sizeof(TypeRecordKind) + 4;
   case TypeRecordKind::LF_ULONG:
      value = *reinterpret_cast<const uint32_t*>(payload);
      return sizeof(TypeRecordKind) + 4;
   case TypeRecordKind::LF_QUADWORD:
   case TypeRecordKind::LF_UQUADWORD:
      value = *reinterpret_cast<const uint64_t*>(payload);
      return sizeof(TypeRecordKind) + 8;
   default: return 0;
   }
}

bool isClassKind(TypeRecordKind kind) {
   return kind == TypeRecordKind::LF_CLASS || kind == TypeRecordKind::LF_STRUCTURE ||
          kind == TypeRecordKind::LF_CLASS2 || kind == TypeRecordKind::LF_STRUCTURE2;
}

// Bit 7 of the class property field: the record is a forward declaration without members.
constexpr uint32_t kForwardRef = 1u << 7;

struct ClassInfo {
   std::string_view name;
   uint32_t fieldList;
   uint64_t size;
   bool forwardRef;
};

bool introducesVirtual(TPI::MemberAttributes attributes) {
   auto property = static_cast<TPI::MethodProperty>(attributes.mprop);
   return property == TPI::MethodProperty::Intro || property == TPI::MethodProperty::PureIntro;
}

std::optional<ClassInfo> classInfo(const TPI::Record* record) {
   if (!record || !isClassKind(record->header.kind)) return std::nullopt;
   uint32_t property;
   uint32_t fieldList;
   const char* data;
   if (record->header.kind == TypeRecordKind::LF_CLASS || record->header.kind == TypeRecordKind::LF_STRUCTURE) {
      property = *reinterpret_cast<const uint16_t*>(&record->data.LF_CLASS.property);
      fieldList = record->data.LF_CLASS.field;
      data = record->data.LF_CLASS.data;
   } else {
      property = record->data.LF_CLASS2.property;
      fieldList = record->data.LF_CLASS2.field;
      data = record->data.LF_CLASS2.data;
   }
   uint64_t size = 0;
   size_t sizeBytes = readNumeric(data, size);
   if (sizeBytes == 0) return std::nullopt;
   return ClassInfo{data + sizeBytes, fieldList, size, (property & kForwardRef) != 0};
}

bool isProcedure(SymbolRecordKind kind) {
   return kind == SymbolRecordKind::S_GPROC32 || kind == SymbolRecordKind::S_LPROC32 ||
          kind == SymbolRecordKind::S_GPROC32_ID || kind == SymbolRecordKind::S_LPROC32_ID;
}

// S_INLINESITE2 adds an invocation count after the fields S_INLINESITE has, so both read alike.
bool isInlineSite(SymbolRecordKind kind) {
   return kind == SymbolRecordKind::S_INLINESITE || kind == SymbolRecordKind::S_INLINESITE2;
}

std::string_view unqualified(std::string_view name) {
   auto colons = name.rfind("::");
   return colons == std::string_view::npos ? name : name.substr(colons + 2);
}

} // namespace

class PdbFile {
public:
   static std::unique_ptr<PdbFile> open(const std::filesystem::path& path, const Identity& expected) {
      auto file = std::make_unique<MappedFile>(path);
      if (!file->data()) {
         log::error("Cannot open {}", path.string());
         return nullptr;
      }
      if (failed(PDB::ValidateFile(file->data(), file->size()))) {
         log::error("{} is not a valid PDB", path.string());
         return nullptr;
      }
      auto pdb = std::unique_ptr<PdbFile>(new PdbFile(std::move(file)));
      if (failed(PDB::HasValidDBIStream(pdb->raw_)) || failed(PDB::HasValidTPIStream(pdb->raw_))) {
         log::error("{} has no usable DBI or TPI stream", path.string());
         return nullptr;
      }
      PDB::InfoStream info(pdb->raw_);
      const auto* header = info.GetHeader();
      if (std::memcmp(&header->guid, &expected.guid, sizeof(GUID)) != 0) {
         log::error("{} does not match the running executable", path.string());
         return nullptr;
      }
      pdb->dbi_ = PDB::CreateDBIStream(pdb->raw_);
      if (failed(pdb->dbi_.HasValidSymbolRecordStream(pdb->raw_)) ||
          failed(pdb->dbi_.HasValidPublicSymbolStream(pdb->raw_)) ||
          failed(pdb->dbi_.HasValidImageSectionStream(pdb->raw_))) {
         log::error("{} has no usable public symbol stream", path.string());
         return nullptr;
      }
      return pdb;
   }

   std::optional<uint32_t> publicRva(std::string_view decoratedName) {
      if (publics_.empty()) indexPublics();
      auto it = publics_.find(decoratedName);
      if (it == publics_.end()) return std::nullopt;
      return it->second;
   }

   std::optional<uint32_t> memberOffset(std::string_view type, std::string_view path) {
      if (classes_.empty()) indexTypes();
      auto current = definitionOf(type);
      if (!current) {
         log::error("Type not found: {}", type);
         return std::nullopt;
      }
      uint32_t total = 0;
      size_t start = 0;
      while (start <= path.size()) {
         size_t end = path.find('.', start);
         if (end == std::string_view::npos) end = path.size();
         auto component = path.substr(start, end - start);
         auto member = findMember(*current, component);
         if (!member) {
            log::error("Member not found: {} {} (at {})", type, path, component);
            return std::nullopt;
         }
         total += member->offset;
         start = end + 1;
         if (start <= path.size()) {
            current = classOf(member->type);
            if (!current) {
               log::error("Member {} of {} is not a class", component, type);
               return std::nullopt;
            }
         }
      }
      return total;
   }

   std::optional<int64_t> enumeratorValue(std::string_view type, std::string_view enumerator) {
      if (classes_.empty()) indexTypes();
      auto it = enums_.find(type);
      if (it == enums_.end()) {
         log::error("Enum not found: {}", type);
         return std::nullopt;
      }
      std::optional<int64_t> found;
      forEachField(it->second, [&](const TPI::FieldList& field, const char* name, uint64_t value) {
         if (field.kind != TypeRecordKind::LF_ENUMERATE || !name || name != enumerator) return true;
         found = static_cast<int64_t>(value);
         return false;
      });
      if (!found) log::error("Enumerator not found: {}::{}", type, enumerator);
      return found;
   }

   std::optional<uint32_t> classSize(std::string_view type) {
      if (classes_.empty()) indexTypes();
      auto index = definitionOf(type);
      if (!index) {
         log::error("Type not found: {}", type);
         return std::nullopt;
      }
      return static_cast<uint32_t>(classInfo(this->type(*index))->size);
   }

   // Slot of a virtual method in the class's primary vtable, found where the method is introduced:
   // in the class itself or along its chain of primary (offset 0) bases.
   std::optional<uint32_t> virtualSlot(std::string_view type, std::string_view method) {
      if (classes_.empty()) indexTypes();
      auto index = definitionOf(type);
      if (!index) {
         log::error("Type not found: {}", type);
         return std::nullopt;
      }
      for (int depth = 0; index && depth < 32; ++depth) {
         std::vector<uint32_t> slots;
         std::optional<uint32_t> primaryBase;
         forEachField(classInfo(this->type(*index))->fieldList, [&](const TPI::FieldList& field, const char* name,
                                                                    uint64_t value) {
            if (field.kind == TypeRecordKind::LF_BCLASS && value == 0 && !primaryBase)
               primaryBase = classOf(field.data.LF_BCLASS.index);
            if (!name || name != method) return true;
            if (field.kind == TypeRecordKind::LF_ONEMETHOD && introducesVirtual(field.data.LF_ONEMETHOD.attributes))
               slots.push_back(static_cast<uint32_t>(value));
            if (field.kind == TypeRecordKind::LF_METHOD) collectIntroSlots(field.data.LF_METHOD.mList, slots);
            return true;
         });
         if (slots.size() == 1) return slots[0] / static_cast<uint32_t>(sizeof(void*));
         if (slots.size() > 1) {
            log::error("Virtual method {}::{} is overloaded; cannot pick a slot", type, method);
            return std::nullopt;
         }
         index = primaryBase;
      }
      log::error("Virtual method not found: {}::{}", type, method);
      return std::nullopt;
   }

   // One pass finds the procedure at each RVA and its function ID, a second finds the inline sites
   // naming those IDs.
   std::vector<InlinedCopies> inlinedCopies(std::span<const uint32_t> rvas) {
      auto started = std::chrono::steady_clock::now();
      if (publics_.empty()) indexPublics();
      std::vector<InlinedCopies> results(rvas.size());
      std::vector<std::set<std::string>> into(rvas.size());
      std::unordered_map<uint32_t, size_t> slotOfRva;
      for (size_t i = 0; i < rvas.size(); ++i) {
         results[i].address = rvas[i];
         slotOfRva.emplace(rvas[i], i);
      }

      // S_*PROC32_ID records carry the function's ID; plain S_*PROC32 ones carry its type, which
      // is matched to an ID through the IPI stream by type and name.
      std::unordered_map<uint32_t, size_t> slotOfId;
      std::vector<std::pair<size_t, uint32_t>> byType;
      const auto modules = dbi_.CreateModuleInfoStream(raw_);
      for (const auto& module : modules.GetModules()) {
         if (!module.HasSymbolStream()) continue;
         const auto stream = module.CreateSymbolStream(raw_);
         stream.ForEachSymbol([&](const DBI::Record* record) {
            if (!isProcedure(record->header.kind)) return;
            const auto& proc = record->data.S_GPROC32;
            auto it = slotOfRva.find(sections_.ConvertSectionOffsetToRVA(proc.section, proc.offset));
            if (it == slotOfRva.end()) return;
            results[it->second].name = proc.name;
            if (record->header.kind == SymbolRecordKind::S_GPROC32_ID ||
                record->header.kind == SymbolRecordKind::S_LPROC32_ID)
               slotOfId.emplace(proc.typeIndex, it->second);
            else
               byType.emplace_back(it->second, proc.typeIndex);
         });
      }
      if (!byType.empty() && !failed(PDB::HasValidIPIStream(raw_))) {
         const auto ipi = PDB::CreateIPIStream(raw_);
         const auto records = ipi.GetTypeRecords();
         for (size_t i = 0; i < records.GetLength(); ++i) {
            const auto* record = records[i];
            uint32_t type;
            const char* name;
            if (record->header.kind == IPI::TypeRecordKind::LF_FUNC_ID) {
               type = record->data.LF_FUNC_ID.typeIndex;
               name = record->data.LF_FUNC_ID.name;
            } else if (record->header.kind == IPI::TypeRecordKind::LF_MFUNC_ID) {
               type = record->data.LF_MFUNC_ID.typeIndex;
               name = record->data.LF_MFUNC_ID.name;
            } else {
               continue;
            }
            for (const auto& [slot, procType] : byType) {
               if (type == procType && unqualified(results[slot].name) == name)
                  slotOfId.emplace(ipi.GetFirstTypeIndex() + static_cast<uint32_t>(i), slot);
            }
         }
      }

      for (const auto& module : modules.GetModules()) {
         if (!module.HasSymbolStream()) continue;
         const auto stream = module.CreateSymbolStream(raw_);
         stream.ForEachSymbol([&](const DBI::Record* record) {
            if (!isInlineSite(record->header.kind)) return;
            auto it = slotOfId.find(record->data.S_INLINESITE.inlinee);
            if (it == slotOfId.end()) return;
            results[it->second].sites++;
            // Inline sites nest in blocks and in other inline sites, whose parents always lie earlier
            // in the stream; the outermost scope is the procedure.
            const DBI::Record* scope = record;
            while (isInlineSite(scope->header.kind) || scope->header.kind == SymbolRecordKind::S_BLOCK32) {
               scope = isInlineSite(scope->header.kind) ? stream.GetParentRecord(scope->data.S_INLINESITE)
                                                        : stream.GetParentRecord(scope->data.S_BLOCK32);
            }
            into[it->second].insert(
               isProcedure(scope->header.kind)
                  ? std::string(scope->data.S_GPROC32.name)
                  : std::format("<scope record {:#x}>", static_cast<uint16_t>(scope->header.kind)));
         });
      }
      for (size_t i = 0; i < results.size(); ++i) results[i].into.assign(into[i].begin(), into[i].end());
      log::info("Scanned {} modules for inline copies in {} ms", modules.GetModules().GetLength(), elapsedMs(started));
      return results;
   }

private:
   struct Member {
      uint32_t offset;
      uint32_t type;
   };

   explicit PdbFile(std::unique_ptr<MappedFile> file)
      : file_(std::move(file)), raw_(PDB::CreateRawFile(file_->data())) {}

   void indexPublics() {
      auto started = std::chrono::steady_clock::now();
      sections_ = dbi_.CreateImageSectionStream(raw_);
      symbolRecords_ = dbi_.CreateSymbolRecordStream(raw_);
      publicSymbols_ = dbi_.CreatePublicSymbolStream(raw_);
      for (const PDB::HashRecord& hash : publicSymbols_.GetRecords()) {
         const auto* record = publicSymbols_.GetRecord(symbolRecords_, hash);
         if (record->header.kind != PDB::CodeView::DBI::SymbolRecordKind::S_PUB32) continue;
         uint32_t rva = sections_.ConvertSectionOffsetToRVA(record->data.S_PUB32.section, record->data.S_PUB32.offset);
         if (rva != 0) publics_.emplace(record->data.S_PUB32.name, rva);
      }
      log::info("Indexed {} public symbols in {} ms", publics_.size(), elapsedMs(started));
   }

   void indexTypes() {
      auto started = std::chrono::steady_clock::now();
      tpi_ = PDB::CreateTPIStream(raw_);
      // Type records are variable-length, so one pass records where each index starts.
      const auto& direct = tpi_.GetDirectMSFStream();
      typeStream_ = PDB::CoalescedMSFStream(direct, direct.GetSize(), 0);
      firstType_ = tpi_.GetFirstTypeIndex();
      types_.reserve(tpi_.GetTypeRecordCount());
      tpi_.ForEachTypeRecordHeaderAndOffset([this](const TPI::RecordHeader&, size_t offset) {
         types_.push_back(typeStream_.GetDataAtOffset<const TPI::Record>(offset));
      });
      for (uint32_t i = 0; i < types_.size(); ++i) {
         auto info = classInfo(types_[i]);
         if (info && !info->forwardRef) classes_.try_emplace(info->name, firstType_ + i);
         const auto* record = types_[i];
         if (record->header.kind == TypeRecordKind::LF_ENUM &&
             !(*reinterpret_cast<const uint16_t*>(&record->data.LF_ENUM.property) & kForwardRef))
            enums_.try_emplace(record->data.LF_ENUM.name, record->data.LF_ENUM.field);
      }
      log::info("Indexed {} type records, {} class definitions in {} ms", types_.size(), classes_.size(),
                elapsedMs(started));
   }

   static long long elapsedMs(std::chrono::steady_clock::time_point started) {
      return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
   }

   const TPI::Record* type(uint32_t index) const {
      if (index < firstType_ || index - firstType_ >= types_.size()) return nullptr;
      return types_[index - firstType_];
   }

   std::optional<uint32_t> definitionOf(std::string_view name) const {
      auto it = classes_.find(name);
      if (it == classes_.end()) return std::nullopt;
      return it->second;
   }

   // The class a member's type refers to, through cv-modifiers and from a forward declaration to
   // its definition; never through a pointer, since a member path describes storage in one object.
   std::optional<uint32_t> classOf(uint32_t index) const {
      for (int depth = 0; depth < 8; ++depth) {
         const auto* record = type(index);
         if (!record) return std::nullopt;
         if (record->header.kind == TypeRecordKind::LF_MODIFIER) {
            index = record->data.LF_MODIFIER.type;
            continue;
         }
         auto info = classInfo(record);
         if (!info) return std::nullopt;
         if (!info->forwardRef) return index;
         return definitionOf(info->name);
      }
      return std::nullopt;
   }

   // Adds the vtable offsets of the virtuals an LF_METHODLIST (one overload set) introduces.
   void collectIntroSlots(uint32_t methodListIndex, std::vector<uint32_t>& slots) const {
      const auto* record = type(methodListIndex);
      if (!record || record->header.kind != TypeRecordKind::LF_METHODLIST) return;
      const char* begin = record->data.LF_METHODLIST.mList;
      const size_t size = record->header.size - sizeof(uint16_t);
      for (size_t i = 0; i + sizeof(TPI::MethodListEntry) <= size;) {
         const auto* entry = reinterpret_cast<const TPI::MethodListEntry*>(begin + i);
         i += sizeof(TPI::MethodListEntry);
         if (introducesVirtual(entry->attributes)) {
            slots.push_back(*reinterpret_cast<const uint32_t*>(begin + i));
            i += sizeof(uint32_t);
         }
      }
   }

   std::string_view className(uint32_t index) const {
      auto info = classInfo(type(index));
      return info ? info->name : std::string_view{};
   }

   // As C++ looks a name up: the class's own members hide its bases' (BlueprintRecordSlotButton's
   // `location` hides agui::Widget's), and the bases follow in declaration order.
   std::optional<Member> findMember(uint32_t classIndex, std::string_view name) const {
      auto info = classInfo(type(classIndex));
      if (!info) return std::nullopt;
      std::optional<Member> found;
      forEachField(info->fieldList, [&](const TPI::FieldList& field, const char* memberName, uint64_t offset) {
         if (field.kind == TypeRecordKind::LF_MEMBER && memberName == name) {
            found = Member{static_cast<uint32_t>(offset), field.data.LF_MEMBER.index};
            return false;
         }
         return true;
      });
      if (found) return found;
      forEachField(info->fieldList, [&](const TPI::FieldList& field, const char*, uint64_t offset) {
         if (field.kind == TypeRecordKind::LF_BCLASS) {
            auto base = classOf(field.data.LF_BCLASS.index);
            if (!base) return true;
            if (className(*base) == name) {
               found = Member{static_cast<uint32_t>(offset), *base};
               return false;
            }
            if (auto inner = findMember(*base, name)) {
               found = Member{static_cast<uint32_t>(offset) + inner->offset, inner->type};
               return false;
            }
         }
         return true;
      });
      return found;
   }

   // Calls visit(field, name, offset) for each entry of a field list, following LF_INDEX
   // continuations. `name` is null and `offset` 0 where they do not apply. visit returns false to
   // stop. Returns false when stopped early or on a record kind this walker does not know.
   template <class Visit>
   bool forEachField(uint32_t fieldListIndex, Visit&& visit) const {
      const auto* record = type(fieldListIndex);
      if (!record || record->header.kind != TypeRecordKind::LF_FIELDLIST) return false;
      const auto* begin = reinterpret_cast<const char*>(&record->data.LF_FIELD.list);
      const size_t size = record->header.size - sizeof(uint16_t);

      for (size_t i = 0; i < size;) {
         // Bytes LF_PAD0..LF_PAD15 (0xF0..0xFF) align the next entry; the low nibble is the skip.
         auto lead = static_cast<uint8_t>(begin[i]);
         if (lead >= 0xF0) {
            i += (lead & 0x0F) ? (lead & 0x0F) : 1;
            continue;
         }
         const auto* field = reinterpret_cast<const TPI::FieldList*>(begin + i);
         const char* next = nullptr;
         uint64_t value = 0;
         size_t numericSize = 0;
         switch (field->kind) {
         case TypeRecordKind::LF_MEMBER: {
            numericSize = readNumeric(field->data.LF_MEMBER.offset, value);
            if (numericSize == 0) return false;
            const char* name = field->data.LF_MEMBER.offset + numericSize;
            if (!visit(*field, name, value)) return false;
            next = name + std::strlen(name) + 1;
            break;
         }
         case TypeRecordKind::LF_BCLASS:
            numericSize = readNumeric(field->data.LF_BCLASS.offset, value);
            if (numericSize == 0) return false;
            if (!visit(*field, nullptr, value)) return false;
            next = field->data.LF_BCLASS.offset + numericSize;
            break;
         case TypeRecordKind::LF_VBCLASS:
         case TypeRecordKind::LF_IVBCLASS: {
            uint64_t ignored = 0;
            size_t first = readNumeric(field->data.LF_VBCLASS.vbpOffset, ignored);
            if (first == 0) return false;
            size_t second = readNumeric(field->data.LF_VBCLASS.vbpOffset + first, ignored);
            if (second == 0) return false;
            next = field->data.LF_VBCLASS.vbpOffset + first + second;
            break;
         }
         case TypeRecordKind::LF_INDEX:
            if (!forEachField(field->data.LF_INDEX.type, visit)) return false;
            next = reinterpret_cast<const char*>(&field->data.LF_INDEX + 1);
            break;
         case TypeRecordKind::LF_VFUNCTAB: next = reinterpret_cast<const char*>(&field->data.LF_VFUNCTAB + 1); break;
         case TypeRecordKind::LF_NESTTYPE:
            next = field->data.LF_NESTTYPE.name + std::strlen(field->data.LF_NESTTYPE.name) + 1;
            break;
         case TypeRecordKind::LF_STMEMBER:
            next = field->data.LF_STMEMBER.name + std::strlen(field->data.LF_STMEMBER.name) + 1;
            break;
         case TypeRecordKind::LF_METHOD:
            if (!visit(*field, field->data.LF_METHOD.name, 0)) return false;
            next = field->data.LF_METHOD.name + std::strlen(field->data.LF_METHOD.name) + 1;
            break;
         case TypeRecordKind::LF_ONEMETHOD: {
            // Introducing virtual methods carry their vtable offset before the name; it is passed
            // to visit as the offset.
            bool intro = introducesVirtual(field->data.LF_ONEMETHOD.attributes);
            const char* name =
               reinterpret_cast<const char*>(field->data.LF_ONEMETHOD.vbaseoff) + (intro ? sizeof(uint32_t) : 0);
            if (!visit(*field, name, intro ? field->data.LF_ONEMETHOD.vbaseoff[0] : 0)) return false;
            next = name + std::strlen(name) + 1;
            break;
         }
         case TypeRecordKind::LF_ENUMERATE: {
            numericSize = readNumeric(field->data.LF_ENUMERATE.value, value);
            if (numericSize == 0) return false;
            const char* name = field->data.LF_ENUMERATE.value + numericSize;
            if (!visit(*field, name, value)) return false;
            next = name + std::strlen(name) + 1;
            break;
         }
         default:
            log::error("Unknown field record kind {:#x} in field list {:#x}", static_cast<uint16_t>(field->kind),
                       fieldListIndex);
            return false;
         }
         i = static_cast<size_t>(next - begin);
      }
      return true;
   }

   std::unique_ptr<MappedFile> file_;
   PDB::RawFile raw_;
   PDB::DBIStream dbi_;

   PDB::ImageSectionStream sections_;
   PDB::CoalescedMSFStream symbolRecords_;
   PDB::PublicSymbolStream publicSymbols_;
   std::unordered_map<std::string_view, uint32_t> publics_;

   PDB::TPIStream tpi_;
   PDB::CoalescedMSFStream typeStream_;
   uint32_t firstType_ = 0;
   std::vector<const TPI::Record*> types_;
   std::unordered_map<std::string_view, uint32_t> classes_;
   std::unordered_map<std::string_view, uint32_t> enums_; // name to field list
};

std::string Identity::key() const {
   return std::format("{:08X}{:04X}{:04X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}-{}", guid.Data1, guid.Data2,
                      guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4],
                      guid.Data4[5], guid.Data4[6], guid.Data4[7], age);
}

std::optional<Identity> identityOf(HMODULE image) {
   auto* bytes = reinterpret_cast<const BYTE*>(image);
   auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes);
   auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes + dos->e_lfanew);
   const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
   auto* entries = reinterpret_cast<const IMAGE_DEBUG_DIRECTORY*>(bytes + dir.VirtualAddress);
   for (size_t i = 0; i < dir.Size / sizeof(IMAGE_DEBUG_DIRECTORY); ++i) {
      if (entries[i].Type != IMAGE_DEBUG_TYPE_CODEVIEW) continue;
      auto* cv = reinterpret_cast<const CodeViewPdb70*>(bytes + entries[i].AddressOfRawData);
      if (cv->signature != kRsds) continue;
      return Identity{cv->guid, cv->age, cv->path};
   }
   return std::nullopt;
}

SymbolTable::SymbolTable(HMODULE image, std::filesystem::path exePath, std::filesystem::path cacheFile)
   : image_(image), exePath_(std::move(exePath)), cacheFile_(std::move(cacheFile)), identity_(identityOf(image)) {
   loadCache();
}

SymbolTable::~SymbolTable() { finish(); }

// The cache's first line: the PDB it was resolved from, and the lookup rules it was resolved by. Bump
// kResolverRules when a rule changes, so a cache made under the old one is resolved afresh.
constexpr int kResolverRules = 2; // 2: a class's own members hide its bases'
std::string SymbolTable::cacheHeader() const { return std::format("{} rules {}", identity_->key(), kResolverRules); }

void SymbolTable::loadCache() {
   if (!identity_ || cacheFile_.empty()) return;
   std::ifstream in(cacheFile_);
   std::string line;
   if (!std::getline(in, line) || line != cacheHeader()) return;
   while (std::getline(in, line)) {
      auto tab = line.rfind('\t');
      if (tab == std::string::npos) continue;
      cache_[line.substr(0, tab)] = std::stoull(line.substr(tab + 1));
   }
   log::info("Symbol cache: {} entries for PDB {}", cache_.size(), identity_->key());
}

PdbFile* SymbolTable::pdb() {
   if (pdbTried_) return pdb_.get();
   pdbTried_ = true;
   if (!identity_) {
      log::error("The executable has no CodeView record naming its PDB");
      return nullptr;
   }
   // The PDB ships beside factorio.exe; the recorded path is the build machine's.
   auto path = exePath_.parent_path() / std::filesystem::path(identity_->pdbPath).filename();
   auto started = std::chrono::steady_clock::now();
   pdb_ = PdbFile::open(path, *identity_);
   if (pdb_) {
      log::info(
         "Opened {} in {} ms", path.string(),
         std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
   }
   return pdb_.get();
}

std::optional<uintptr_t> SymbolTable::address(std::string_view decoratedName) {
   auto key = std::format("rva {}", decoratedName);
   if (auto it = cache_.find(key); it != cache_.end()) return reinterpret_cast<uintptr_t>(image_) + it->second;
   auto* file = pdb();
   if (!file) return std::nullopt;
   auto rva = file->publicRva(decoratedName);
   if (!rva) {
      log::error("Symbol not found: {}", decoratedName);
      return std::nullopt;
   }
   cache_[key] = *rva;
   cacheDirty_ = true;
   return reinterpret_cast<uintptr_t>(image_) + *rva;
}

std::optional<uint32_t> SymbolTable::offset(std::string_view type, std::string_view path) {
   auto key = std::format("off {} {}", type, path);
   if (auto it = cache_.find(key); it != cache_.end()) return static_cast<uint32_t>(it->second);
   auto* file = pdb();
   if (!file) return std::nullopt;
   auto offset = file->memberOffset(type, path);
   if (!offset) return std::nullopt;
   cache_[key] = *offset;
   cacheDirty_ = true;
   return offset;
}

std::optional<uint32_t> SymbolTable::size(std::string_view type) {
   auto key = std::format("size {}", type);
   if (auto it = cache_.find(key); it != cache_.end()) return static_cast<uint32_t>(it->second);
   auto* file = pdb();
   if (!file) return std::nullopt;
   auto size = file->classSize(type);
   if (!size) return std::nullopt;
   cache_[key] = *size;
   cacheDirty_ = true;
   return size;
}

std::optional<int64_t> SymbolTable::enumValue(std::string_view type, std::string_view enumerator) {
   auto key = std::format("enum {} {}", type, enumerator);
   if (auto it = cache_.find(key); it != cache_.end()) return static_cast<int64_t>(it->second);
   auto* file = pdb();
   if (!file) return std::nullopt;
   auto value = file->enumeratorValue(type, enumerator);
   if (!value) return std::nullopt;
   cache_[key] = static_cast<uint64_t>(*value);
   cacheDirty_ = true;
   return value;
}

std::optional<uint32_t> SymbolTable::virtualSlot(std::string_view type, std::string_view method) {
   auto key = std::format("vslot {} {}", type, method);
   if (auto it = cache_.find(key); it != cache_.end()) return static_cast<uint32_t>(it->second);
   auto* file = pdb();
   if (!file) return std::nullopt;
   auto slot = file->virtualSlot(type, method);
   if (!slot) return std::nullopt;
   cache_[key] = *slot;
   cacheDirty_ = true;
   return slot;
}

std::vector<InlinedCopies> SymbolTable::inlinedCopies(std::span<const uintptr_t> functions) {
   auto* file = pdb();
   if (!file) return {};
   const auto base = reinterpret_cast<uintptr_t>(image_);
   std::vector<uint32_t> rvas;
   for (uintptr_t function : functions) rvas.push_back(static_cast<uint32_t>(function - base));
   auto results = file->inlinedCopies(rvas);
   for (auto& result : results) result.address += base;
   return results;
}

void SymbolTable::finish() {
   if (cacheDirty_ && identity_ && !cacheFile_.empty()) {
      std::ofstream out(cacheFile_, std::ios::trunc);
      out << cacheHeader() << '\n';
      for (const auto& [key, value] : cache_) out << key << '\t' << value << '\n';
      cacheDirty_ = false;
   }
   pdb_.reset();
}

} // namespace fa::pdb
