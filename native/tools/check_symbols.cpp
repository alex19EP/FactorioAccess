// Resolves everything the DLL needs against a Factorio build without running it, so a new
// Factorio release can be checked in a second. Usage: fa_symbols_check <path to factorio.exe>

#include "game.h"
#include "log.h"
#include "symbols.h"

#include <windows.h>

#include <cstdio>
#include <filesystem>

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
   symbols.finish();

   const auto base = reinterpret_cast<uintptr_t>(image);
   if (ok) {
      fa::log::info("agui::Gui::logic rva {:#x}, agui::Gui::instance rva {:#x}", fa::game::layout.guiLogic - base,
                    fa::game::layout.guiInstance - base);
   }
   FreeLibrary(mapped);
   std::fputs(ok ? "OK\n" : "FAILED\n", stdout);
   return ok ? 0 : 1;
}
