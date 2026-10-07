# FactorioAccess native

A DLL loaded into `factorio.exe` that FactorioAccess requires. It replaces the old Python
launcher, and it reads the parts of the game the Lua API cannot reach. It speaks through
[Prism](https://github.com/ethindp/prism), so any screen reader Prism supports works. It runs on
Windows only.

## What it does

- **Speech for the mod.** The mod's Lua gets a `fa_native` table. `fa_native.speak` translates a
  LocalisedString with the game's own localisation and speaks it, in place of the launcher's
  stdout protocol.
- **The game's own GUI.** Vanilla windows and menus are read and navigated from the keyboard:
  Tab, the arrows and Enter, following a graph model of each window. The main menu and its
  screens, the character screen, entity windows, the quickbar, the shortcut bar, the side menu, the
  HUD's status (research, alerts, goal, bars), the crafting queue and other windows each have a
  recipe that gives a fixed order. Any other window is read generically.
  Activating a control replays the same mouse events vanilla gets, so using a window through FA
  never differs from clicking it. The OS mouse is never moved.
- **The world cursor.** The game's cursor position follows the FA cursor, so vanilla controls
  (building, mining, opening, selection tools) act where the FA cursor is.
- **Flying text.** Every flying text the game shows is spoken: "Cannot reach", "Cannot build
  here", "Not enough ingredients", item counts after mining or picking up, and other mods' local
  flying text.
- **Disclosure.** Automatic crash log upload is kept off, since crashes with a modified executable
  shouldn't reach Wube. The game's version text names the DLL.

## How it loads

The build produces `winmm.dll`. `factorio.exe` imports winmm, and winmm is not a KnownDLL, so a
copy in `bin/x64` is loaded instead of the system one. Every winmm export forwards to
`System32\winmm.dll` by absolute path, so the game and anything else using winmm keep working.
`version.dll` is left alone for the achievement enabler.

Nothing is hard-coded per Factorio build. Function addresses, class layouts and virtual slots
come from `factorio.pdb`, which ships next to the executable, read with
[raw_pdb](https://github.com/MolecularMatters/raw_pdb). Answers are cached in
`factorio-access-native.symbols` beside the DLL, keyed by the PDB's GUID. If any name is missing,
for example after a Factorio update renamed something, the DLL says so in its log and touches
nothing.

## Building

You need Visual Studio 2026 (or its Build Tools) with the C++ workload, and CMake 3.28 or newer.

```
git submodule update --init native/vendor
cd native
cmake --preset default -DFA_FACTORIO_BIN="C:/path/to/Factorio/bin/x64"
cmake --build --preset default
ctest --test-dir build -C RelWithDebInfo --output-on-failure
```

With `FA_FACTORIO_BIN` set, each build copies `winmm.dll` and its PDB there. Without it, copy
them by hand. To uninstall, delete `winmm.dll` and `winmm.pdb` from `bin/x64`.

`fa_symbols_check <path to factorio.exe>` resolves everything the DLL needs against a Factorio
build without running the game. Run it after every Factorio update.

## Diagnostics

- **Log:** `factorio-access-native.log`, beside the DLL.
- **GUI dumps:** each time a window is read, its widget tree goes to `gui-dumps/<WindowClass>.txt`
  beside the DLL.
- **Dev server:** a loopback HTTP server on port 8773 lets tools inspect and drive the DLL:
  `/health`, `/speech`, `/screen` and `POST /key`. It starts only when
  `factorio-access-dev.enable` sits beside the DLL or `FA_DEV=1` is set.

## Source

- `src/`: the DLL.
- `tests/`: the graph kernel's conformance suite, which runs without the game.
- `tools/`: the symbol checker.
- `vendor/`: Prism, MinHook and raw_pdb as git submodules.

`CLAUDE.md` holds the development rules.
