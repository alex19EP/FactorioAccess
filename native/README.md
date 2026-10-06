# FactorioAccess native

A DLL loaded into `factorio.exe` that reads the game's own GUI (`agui` widgets) and speaks it
through [Prism](https://github.com/ethindp/prism). It is the start of replacing the Python
launcher and making the vanilla and other mods' GUIs accessible. Today it is a spike: it reads
the main menu.

## How it loads

The build produces `winmm.dll`. `factorio.exe` imports winmm and winmm is not a KnownDLL, so a
copy in `bin/x64` is loaded instead of the system one. Every winmm export forwards to
`System32\winmm.dll` by absolute path, so the game and anything else using winmm keep working.
`version.dll` is left alone for the achievement enabler.

Nothing is hard-coded per Factorio build. Function addresses and class layouts come from
`factorio.pdb`, which ships next to the executable, read with
[raw_pdb](https://github.com/MolecularMatters/raw_pdb). Answers are cached in
`factorio-access-native.symbols` beside the DLL, keyed by the PDB's GUID. If any name is missing,
for example after a Factorio update renamed something, the DLL says so and touches nothing.

## What the spike does

It hooks `agui::Gui::logic`, the per-frame GUI update on the main thread, and after each frame:

- announces each newly shown top-level window by reading its visible text in tree order
- speaks the keyboard-focused widget when focus changes
- speaks the widget under the mouse when it changes

Each new window's full widget tree, with class names, is written to
`factorio-access-native.log` beside the DLL.

## Building

You need Visual Studio 2026 (or its Build Tools) with the C++ workload, and CMake 3.28 or newer.

```
git submodule update --init native/vendor
cd native
cmake --preset default -DFA_FACTORIO_BIN="C:/path/to/Factorio/bin/x64"
cmake --build --preset default
```

With `FA_FACTORIO_BIN` set, each build copies `winmm.dll` and its PDB there. Delete `winmm.dll`
from `bin/x64` to uninstall.

`fa_symbols_check <path to factorio.exe>` resolves everything the DLL needs against a Factorio
build without running the game. Run it after a Factorio update.

## Layout

- `src/dllmain.cpp`: entry point; initialization runs on its own thread
- `src/symbols.*`: PDB reader and the symbol cache
- `src/game.*`: every game symbol and offset the DLL uses
- `src/agui.*`: read-only views of widgets: text, children, visibility, RTTI class name
- `src/gui_reader.*`: the per-frame reader that decides what to say
- `src/speech.*`: Prism on a dedicated thread
- `src/text.*`: strips Factorio rich-text tags for speech
- `src/proxy/winmm.exports`: the export list the proxy forwards
- `vendor/`: Prism, MinHook and raw_pdb as git submodules
