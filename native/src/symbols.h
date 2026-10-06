#pragma once

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace fa::pdb {

// Identity of the PDB that matches a loaded image, from the image's CodeView debug record.
struct Identity {
   GUID guid;
   DWORD age;
   std::string pdbPath;

   std::string key() const;
};

std::optional<Identity> identityOf(HMODULE image);

class PdbFile;

// Resolves names against the PDB of one mapped image, read with raw_pdb. The PDB is expected
// beside the executable. Answers are cached in a file keyed by the PDB identity, so the PDB (about
// 460 MB for Factorio) is only opened when a query is new for this build; an empty cache path
// disables the cache. Not thread-safe; use from one thread and call finish() when done.
class SymbolTable {
public:
   SymbolTable(HMODULE image, std::filesystem::path exePath, std::filesystem::path cacheFile);
   ~SymbolTable();
   SymbolTable(const SymbolTable&) = delete;
   SymbolTable& operator=(const SymbolTable&) = delete;

   const std::optional<Identity>& identity() const { return identity_; }

   // Address of a public function or global by its decorated (mangled) name, which pins the
   // exact overload, e.g. "?logic@Gui@agui@@UEAAX_N@Z".
   std::optional<uintptr_t> address(std::string_view decoratedName);

   // Byte offset of a dotted member path inside a class, e.g. ("agui::Gui", "focusManager").
   // A component may name a base class instead of a member, e.g. "agui::GenericTargetable".
   std::optional<uint32_t> offset(std::string_view type, std::string_view path);

   // Saves new answers to the cache and unmaps the PDB.
   void finish();

private:
   PdbFile* pdb();
   void loadCache();

   HMODULE image_;
   std::filesystem::path exePath_;
   std::filesystem::path cacheFile_;
   std::optional<Identity> identity_;
   std::map<std::string, uint64_t, std::less<>> cache_;
   bool cacheDirty_ = false;

   bool pdbTried_ = false;
   std::unique_ptr<PdbFile> pdb_;
};

} // namespace fa::pdb
