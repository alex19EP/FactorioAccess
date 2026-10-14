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
  HUD's status (research, alerts, goal, bars), the alerts window an alert button opens, the crafting
  queue, Factoriopedia (its entries list,
  and each entry's page with the description's icons as links), tips and tricks (its tips grouped
  under their headings) and other windows each have a
  recipe that gives a fixed order. Any other window is read generically.
  Activating a control replays the same mouse events vanilla gets, so using a window through FA
  never differs from clicking it. The OS mouse is never moved.
- **The world cursor.** The game's cursor position follows the FA cursor, so vanilla controls
  (building, mining, opening, selection tools) act where the FA cursor is. Their mouse buttons
  need keys, which `helper-scripts/factorio-access-keys.ps1` gives them (see "Installing").
- **Flying text.** Every flying text the game shows is spoken: "Cannot reach", "Cannot build
  here", "Not enough ingredients", item counts after mining or picking up, and other mods' local
  flying text.
- **Console.** Each line the console shows is spoken as it arrives: chat, research completed,
  players joining and leaving, and what scenarios and mods print.
- **Pop-ups.** The "New tip" notification, speech bubbles over the map, and the boxes for saving,
  autosaving and multiplayer (waiting for a player, reconnecting, desynced) are spoken as they
  appear.
- **Disclosure.** Automatic crash log upload is kept off, since crashes with a modified executable
  shouldn't reach Wube. The game's version text and the log name the DLL and its build.

## How it loads

The build produces two DLLs:

- `winmm.dll`, a small stub. `factorio.exe` imports winmm, and winmm is not a KnownDLL, so a
  copy in `bin/x64` is loaded instead of the system one. Every winmm export forwards to
  `System32\winmm.dll` by absolute path, so the game and anything else using winmm keep working.
  The stub then loads the core beside it. `version.dll` is left alone for the achievement enabler.
- `fa_native.dll`, the core: everything this README describes.

The game keeps winmm.dll until it exits, but the core can be replaced while it runs. With the dev
server's marker (see "Diagnostics"), the stub loads a copy of the core, `fa_native.live-<n>.dll`,
and watches `fa_native.dll`. When a build replaces it, the stub stops the old core, unloads it and
loads the new one, in about a second, without restarting the game. The mod's Lua keeps working
across it: the `fa_native` functions Lua holds belong to the stub, which passes each call to the
core loaded at the time. A reloaded core starts fresh (the scanner's list, the cursor's state).

Nothing is hard-coded per Factorio build. Function addresses, class layouts and virtual slots
come from `factorio.pdb`, which ships next to the executable, read with
[raw_pdb](https://github.com/MolecularMatters/raw_pdb). Answers are cached in
`factorio-access-native.symbols` beside the DLL, keyed by the PDB's GUID. If any name is missing,
for example after a Factorio update renamed something, the DLL says so in its log and touches
nothing.

## Installing

Players install the DLL from a release, as the root README's
[install steps](../README.md#installing-factorio-access) describe. Each release carries
`FactorioAccess-native.zip`, laid out as the game folder so it is extracted straight into it:

```
bin/x64/winmm.dll
bin/x64/winmm.pdb
bin/x64/fa_native.dll
bin/x64/fa_native.pdb
factorio-access-keys.cmd
factorio-access-keys.ps1
factorio-access-update.cmd
factorio-access-update.ps1
```

`factorio-access-keys.cmd` runs `factorio-access-keys.ps1` through Windows PowerShell. It edits the
`[controls]` section of the game's `config.ini`: walking moves to the arrow keys, connect and
disconnect train to CONTROL + J and CONTROL + K, zoom gets EQUALS and MINUS, and every control on a
mouse button gets the same binding on LEFTBRACKET, RIGHTBRACKET or BACKSLASH. It finds the
`config.ini` through the game folder's `config-path.cfg`, as the game does, so it serves the zip
and the Steam versions. `-GameDir` names the game folder when the script runs from elsewhere, and
`-Revert` undoes the changes. The source is in `helper-scripts/`.

With the game closed, `factorio-access-update.cmd` downloads both archives from the latest `dev`
release of `alex19EP/FactorioAccess` and updates the DLLs, helper scripts and mod zip together.
It uses the game folder it is installed in, or finds the Steam installation across Steam libraries.
Use `-GameDir "D:\Games\Factorio"` for another installation and `-ModsDir` to override the mods
folder when launch options select a different configuration. It reads `config-path.cfg` and
`config.ini` for the normal portable and Steam paths. Saves, settings and other mods are preserved.

## Dev builds

`.github/workflows/dev-build.yaml` runs on every push to `native`. It builds the DLL and runs the
graph tests on a Visual Studio 2026 runner, packs `FactorioAccess-native.zip`, builds the mod zip
with fmtk, and moves the `dev` tag and its pre-release to the pushed commit. The DLL is stamped
with `FA_BUILD_ID=dev.<commit>`, so its version reads, for example, `0.1.0+dev.fc39a54`. The
symbol check needs a `factorio.exe` and does not run there: run it locally before pushing.

## Building

You need Visual Studio 2026 (or its Build Tools) with the C++ workload, CMake 3.28 or newer, and
Ninja. The build uses the Ninja Multi-Config generator, which calls the compiler directly, so run
configure and every build from an x64 developer environment: the "x64 Native Tools Command Prompt
for VS", or a shell after `vcvars64.bat`. The output goes to `build/RelWithDebInfo`.

The repo-root `justfile` does all of this for you (`just build`, `just test`, `just deploy`); see
`devdocs/dev-setup.md`. By hand:

```
git submodule update --init native/vendor
"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd native
cmake --preset default -DFA_FACTORIO_BIN="C:/path/to/Factorio/bin/x64"
cmake --build --preset default
ctest --test-dir build -C RelWithDebInfo --output-on-failure
```

With `FA_FACTORIO_BIN` set, each build copies both DLLs and their PDBs there. Without it, copy
them by hand. While the game runs, winmm.dll can't be replaced, but fa_native.dll can, and with
the dev marker the game loads it at once (`just reload`). To uninstall, delete `winmm.dll`,
`winmm.pdb`, `fa_native.dll` and `fa_native.pdb` from `bin/x64`.

`-DFA_BUILD_ID=<id>` appends `+<id>` to the version the game shows and the log writes, so a build
can be told apart. Local builds leave it empty.

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

- `src/`: the core, fa_native.dll.
- `src/stub/`: winmm.dll, the stub that loads and reloads it.
- `tests/`: the graph kernel's conformance suite, which runs without the game.
- `tools/`: the symbol checker.
- `vendor/`: Prism, MinHook and raw_pdb as git submodules.

`CLAUDE.md` holds the development rules.
