#pragma once

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fa::pdb {

// Where the compiler copied a function inline into other functions. A hook on the function's
// address never sees the calls those copies stand for.
struct InlinedCopies {
   uintptr_t address;
   std::string name;               // the out-of-line copy's procedure name; empty if the PDB has none
   size_t sites = 0;               // inline copies in the whole image
   std::vector<std::string> into;  // the functions holding them, each once, sorted
};

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

   // sizeof a class.
   std::optional<uint32_t> size(std::string_view type);

   // Index of a virtual method in the class's primary vtable, by its undecorated name, e.g.
   // ("agui::Widget", "keyDown"). Fails when the name is overloaded among the introduced virtuals.
   std::optional<uint32_t> virtualSlot(std::string_view type, std::string_view method);

   // For each function starting at one of these addresses, where it was inlined. Reads the symbols
   // of every module, so it takes seconds and is not cached: meant for fa_symbols_check.
   std::vector<InlinedCopies> inlinedCopies(std::span<const uintptr_t> functions);

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
