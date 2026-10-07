// Resolves everything the DLL needs against a Factorio build without running it, so a new
// Factorio release can be checked in a second. Usage: fa_symbols_check <path to factorio.exe>

#include "game.h"
#include "hook-list.h"
#include "log.h"
#include "symbols.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string_view>
#include <vector>

namespace {

using fa::game::Layout;

struct HookTarget {
   uintptr_t Layout::* field;
   std::string_view name;
};

constexpr HookTarget kHookTargets[] = {
#define FA_HOOK_TARGET(field, detour, original, name) {&Layout::field, name},
   FA_HOOKS(FA_HOOK_TARGET)
#undef FA_HOOK_TARGET
};

// Inline copies of hooked functions that were read and need no hook of their own. Any other copy
// fails the check: hook the function holding it too, or read it and add it here with the reason.
struct ReviewedCopy {
   uintptr_t Layout::* field;
   std::string_view into;
   std::string_view reason;
};

constexpr ReviewedCopy kReviewedCopies[] = {
   {&Layout::playerCursorPosition, "PlayerInputSource::getCursorMapPosition",
    "its own hook returns the FA cursor before the copy runs"},
   {&Layout::playerCursorPosition, "Player::buildFromCursor",
    "it runs for scripts on every peer and must build where they do"},
   {&Layout::playerCursorPosition, "CopyEquipmentGridLogic::checkRequiredItemsAndShowResult",
    "it only places the item count flying text, which is spoken wherever it is"},
};

// Every hooked function must be a procedure in the PDB with no inline copy outside kReviewedCopies:
// the compiler can inline a function into some callers and keep the copy we hook for the rest,
// and those callers never reach the hook.
bool checkInlining(fa::pdb::SymbolTable& symbols) {
   std::vector<uintptr_t> addresses;
   for (const auto& target : kHookTargets) addresses.push_back(fa::game::layout.*target.field);
   const auto copies = symbols.inlinedCopies(addresses);
   if (copies.size() != std::size(kHookTargets)) return false;
   bool ok = true;
   for (size_t i = 0; i < copies.size(); ++i) {
      const auto& target = kHookTargets[i];
      if (copies[i].name.empty()) {
         fa::log::error("Hooked {} has no procedure record, so its inline copies cannot be checked", target.name);
         ok = false;
         continue;
      }
      for (const auto& into : copies[i].into) {
         auto reviewed = std::ranges::find_if(kReviewedCopies, [&](const ReviewedCopy& copy) {
            return copy.field == target.field && copy.into == into;
         });
         if (reviewed != std::end(kReviewedCopies)) {
            fa::log::info("Hooked {} is inlined into {}; reviewed: {}", target.name, into, reviewed->reason);
         } else {
            fa::log::error("Hooked {} is inlined into {}, which never reaches the hook", target.name, into);
            ok = false;
         }
      }
   }
   for (const auto& copy : kReviewedCopies) {
      auto target = std::ranges::find(kHookTargets, copy.field, &HookTarget::field);
      const auto& found = copies[static_cast<size_t>(target - std::begin(kHookTargets))];
      if (std::ranges::find(found.into, copy.into) == found.into.end())
         fa::log::info("Reviewed copy of {} in {} is gone from this build", target->name, copy.into);
   }
   fa::log::info("Checked {} hooked functions for inline copies", copies.size());
   return ok;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
   if (argc != 2) {
      std::fputs("usage: fa_symbols_check <path to factorio.exe>\n", stderr);
      return 2;
   }
   std::filesystem::path exe = argv[1];
   // Mapped as an image (sections at their RVAs) but never executed or relocated.
   HMODULE mapped = LoadLibraryExW(exe.c_str(), nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
   if (!mapped) {
      std::fprintf(stderr, "cannot map %ls (error %lu)\n", exe.c_str(), GetLastError());
      return 1;
   }
   // Resource mappings come back with low tag bits set on the handle.
   auto image = reinterpret_cast<HMODULE>(reinterpret_cast<uintptr_t>(mapped) & ~uintptr_t{3});

   fa::pdb::SymbolTable symbols(image, exe, {});
   if (const auto& identity = symbols.identity()) fa::log::info("PDB {} ({})", identity->key(), identity->pdbPath);
   bool ok = fa::game::resolve(symbols);

   const auto base = reinterpret_cast<uintptr_t>(image);
   if (ok) {
      fa::log::info("agui::Gui::logic rva {:#x}, agui::Gui::instance rva {:#x}", fa::game::layout.guiLogic - base,
                    fa::game::layout.guiInstance - base);
      ok = checkInlining(symbols);
   }
   symbols.finish();
   FreeLibrary(mapped);
   std::fputs(ok ? "OK\n" : "FAILED\n", stdout);
   return ok ? 0 : 1;
}
