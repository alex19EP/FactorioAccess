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
- `python launch_factorio.py --format` (stylua and clang-format; CI checks both);
- build clean with no new warnings (`/W4`);
- the graph tests pass;
- `fa_symbols_check` reports OK.

A plain cmake build does NOT deploy unless `FA_FACTORIO_BIN` was set at configure time.

The generator is Ninja Multi-Config, so raw `cmake` configure and build commands must run inside
`vcvars64.bat` (`cmd /c "<vcvars64.bat> >nul && cmake ..."`); `just configure` and `just build`
already do. Switching generators needs a fresh tree: `just clean` first.

## Rules

### Symbols and reverse engineering

- Never hard-code an address or offset. Add a field to `game::Layout` and resolve it by name:
  - functions: decorated public name;
  - members: `offset(type, "a.b")`;
  - sizes;
  - virtual slots: `virtualSlot`, asked on the class that introduces the method. Identical-code
    folding makes vtable scans unsafe.

  If any name is missing, the DLL stands down. Run `fa_symbols_check` after a Factorio update.
- Every hook is one entry in `src/hook-list.h`; `hooks::install` installs them all from it.
  `fa_symbols_check` fails when the compiler inlined a hooked function anywhere, because those calls
  never reach the hook. Hook the function holding the copy too, or read the copy in Ghidra and add
  it to `kReviewedCopies` in `tools/check_symbols.cpp` with the reason it needs no hook.
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
  mod's and the DLL's OWN words, which live in `vocab` (see below).
- Rich-text icons are spoken as their names (`text::speakable`).
- Every keypress the navigator handles interrupts speech.

### Localisation of the DLL's own words

Never put an English word or phrase the player hears in C++. The DLL's own words are keys of the
mod's locale, which the game translates into the player's language:

- Add a `vocab::Word` to `src/vocab.h` with an `fa.native-...` key, and its English text to
  `locale/en/native.cfg`. Say it as `std::string(vocab::kWord)`, or pass it where a `std::string`
  is taken.
- A phrase with values ("3 of 10", "bar 2", "filter iron plate") is ONE key with parameters
  (`native-position=__1__ of __2__`), said as `vocab::kPosition(index, count)`, so a translation
  can change the word order. Never glue words and values together with `std::format`. Counts take
  `__plural_for_parameter__`. A `Word` takes up to three parameters, as many as the game's
  `LocalisedString` constructors do.
- Where the mod's Lua already says the same thing, use its key (`fa.direction`).
- Keep a `Word`, not its text, in anything that outlives one announcement (static tables, captured
  lambdas), so the text follows the game's language.
- Translation runs where the game translates (GUI logic and game hooks), never on a thread of our
  own.
- `python lint_localisation.py lint` checks the keys `native/src` uses against the locale files.
- Two exceptions stay English:
  - a failure message said when the game's locale may be out of reach: the version and hook
    failures in `dllmain.cpp`, and the navigator's crash in `ui.cpp`;
  - the text that names the DLL to Wube: the version suffix and the log line in `disclosure.cpp`.

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
- The rest of `src/` (core, hooks, agui, game) and `tools/` use camelCase, attached braces and a
  3-space indent.
- clang-format (20 or later; CI pins 23.1.2) holds the layout: `native/.clang-format` is the core
  style, and the CyberAccess directories each carry the same override. Lines stop at 120 columns.
  Naming is not formatted: match the file you are in.
- A trailing comment must end before column 120, or clang-format re-spaces every hand-aligned
  comment after it. Put a long one on its own line above.
- `vendor/` is never formatted. `// clang-format off` keeps a deliberate layout, such as the
  graph tests' builder chains; give the reason after a colon.
- Comments say why and what something is for, not what changed.
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

Rules for starting the game and using the dev server:
- Don't poll the dev server while the game starts or loads a save. Watch `factorio-current.log`
  instead: "Factorio initialised" after a start, "Checksum for script __FactorioAccess__" after a
  load. Then wait a few more seconds and send one request. A start can take minutes.
- Send keys in small batches and read the result before the next batch.
- Other sessions share the one Steam game and the dev server port. Tell them before launching, and
  again when the game is free.
- Close the game (`just stop`) as soon as the test is done.

The DLL's log is `factorio-access-native.log` beside it. The game's log is
`factorio-current.log` in the user data directory.
