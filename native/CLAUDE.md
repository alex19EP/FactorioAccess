# FactorioAccess native DLL: guide for LLMs

The mod requires this DLL (see the root `CLAUDE.md`, "Native DLL (required)"). It is loaded
into `factorio.exe` as a `winmm.dll` proxy. It reads the game's own state and GUI (`agui` widgets,
the world, flying text) and speaks it through Prism. It also gives the mod's Lua a `fa_native`
table. Anything the Lua API cannot read is read here, at the source. Don't write Lua guesses or
Lua fallbacks.

## Commands

From the repo root. `justfile` and `.env` are the developer's local tooling and may not exist. The
raw commands are given as well.

```
just build      # cmake --build native/build --config RelWithDebInfo
just test       # ctest --test-dir native/build -C RelWithDebInfo --output-on-failure (graph kernel suite)
just symbols    # fa_symbols_check <factorio.exe>: resolve every symbol and layout against a build
just deploy     # copy winmm.dll + winmm.pdb into the Steam bin/x64 (the game must not be running)
just run        # deploy, then start Factorio through Steam
just log        # tail factorio-access-native.log beside the DLL
just dev-on     # enable the loopback dev server on the next start (see below)
```

Before committing native work:
- build clean with no new warnings (`/W4`);
- the graph tests pass;
- `fa_symbols_check` reports OK.

A plain cmake build does NOT deploy unless `FA_FACTORIO_BIN` was set at configure time.

## Rules

### Symbols and reverse engineering

- Never hard-code an address or offset. Add a field to `game::Layout` and resolve it by name:
  - functions: decorated public name;
  - members: `offset(type, "a.b")`;
  - sizes;
  - virtual slots: `virtualSlot`, asked on the class that introduces the method. Identical-code
    folding makes vtable scans unsafe.

  If any name is missing, the DLL stands down. All MinHook detours are installed in one place,
  `hooks::install`. Run `fa_symbols_check` after a Factorio update.
- Do RE in Ghidra through the `mcp__ghidra__*` tools, project `factorio-full`: the DRM-free
  2.1.21 build with full PDB types. Function addresses differ between the DRM-free and Steam builds;
  class layouts match. `xrefs` lists caller addresses only. `get_code` on an address inside a
  function names that function.
- Write each finding down as you go: the layout, the function, and who calls it.

### Threads and lifetime

- `agui::Gui::logic` runs on the main thread for two Guis per frame, and `agui::Gui::instance`
  points at whichever one is running. Touch a Gui only inside its own `logic` call, and keep
  per-Gui state.
- Game hooks (simulation, latency, input handlers) can run on other threads and at other times. In
  a hook, copy what you need and hand it off. `speech::say` already queues.
- Never call into Lua from a hook. Lua runs only when the game runs it.
- Never change game state differently on one client: that desyncs multiplayer. Lua must not
  branch on native-only data in ways that write `storage`.

### Speaking

- Read what is shown: speak game text as the game displays it, through `text::speakable`. Keep
  bullets, colons and dashes. The root CLAUDE.md style rules (no colons, and so on) apply to the
  mod's and the DLL's OWN words, which live in `vocab`.
- Rich-text icons are spoken as their names (`text::speakable`).
- Every keypress the navigator handles interrupts speech.

### Input and GUI interaction

The GUI layer follows the graph-a11y spec, `E:\Games\modding\WH40KRTAccess\docs\graph-a11y-spec.md`.

- Never move the user's mouse or warp the OS cursor.
- Activating a widget replays the vanilla mouse sequence on it (enter, down, click, up, leave;
  `agui::press`), so FA's use of a GUI never differs from vanilla.
- Arrows move, they never select. Enter selects. Dropdowns open into `DropDownScreen` on Enter.
  A slider inside a grid adjusts only after Enter.
- One `WindowScreen` subclass per game window class, with explicit stops in a fixed order, built
  from the game's own members by PDB name. `GenericWindowScreen` takes any window no recipe claims.
  Screens hold no view state; every `Build` reads the game fresh.

### Lua bridge

- `fa_native.*` is registered in every Lua state through `initLuaState`. LuaPlayer::index is the
  engine's `Player::index` + 1.
- Translate text natively: `LuaHelper::parseLocalisedString`, then `LocalisedString::str(nullptr)`.
  Only the local player's calls act.

## Code style

- `src/graph/`, `src/navigator/`, `src/screens/` and `tests/` keep the CyberAccess style:
  PascalCase functions, Allman braces, 4-space indent.
- The rest of `src/` (core, hooks, agui, game) uses camelCase, attached braces and a 3-space
  indent.
- Match the file you are in. Comments say why and what something is for, not what changed.
- Commit subjects are prefixed by area: `Native: ...`, `Character screen: ...`,
  `Machine screen: ...`.

## Testing in game

Run `just run` and turn on the dev server (`just dev-on`) to check behaviour without asking the
user:

```
curl -s localhost:8773/screen                   # live screens, focus, every node
curl -s --data "tab down enter" localhost:8773/key   # inject keys, returns the speech produced
curl -s "localhost:8773/speech?since=0"         # everything said
```

The DLL's log is `factorio-access-native.log` beside it. The game's log is
`factorio-current.log` in the user data directory.
