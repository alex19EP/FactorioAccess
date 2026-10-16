# FactorioAccess Development Guide for LLMs

This guide helps LLMs (particularly Claude) work effectively on the FactorioAccess mod, which makes Factorio playable by blind and visually impaired players through audio cues and keyboard controls.

## Critical Rules (Read First!)

### Most Common Mistakes

1. **API Access: Use dot notation for modules**
   ```lua
   -- WRONG: Viewpoint:get_viewpoint(pindex)
   -- RIGHT: Viewpoint.get_viewpoint(pindex)
   ```

2. **Always format, test, and lint localisation before committing**
   ```bash
   just format
   just test-mod
   just lint-locale
   ```

3. **Debug output: Use print(), not game.print()**
   ```lua
   -- WRONG: game.print("debug")  -- Goes to GUI, blind users can't see
   -- RIGHT: print("debug")        -- Goes to logs
   -- RIGHT: Speech.speak(pindex, "user message")  -- Goes to screen reader
   ```

4. **Type annotations: Use three dashes**
   ```lua
   ---@param pindex integer   -- CORRECT
   -- @param pindex integer   -- WRONG (linter will fail)
   ```

## Development Essentials

### Your Environment

You are running in the repo root. The `justfile` here is the entry point for every task; its paths come from `.env` (see `devdocs/dev-setup.md`). The recipes call the Python helpers (`launch_factorio.py`, `lint_localisation.py`), which hold the logic; run those directly only for options no recipe passes.

Commands should always be relative to the current working directory.

### Key Commands
```bash
# Format code (REQUIRED before committing)
just format

# Run the mod's tests (headless, in FACTORIO_SANDBOX_DIR or the install around the checkout)
just test-mod

# Run linter
just lint

# Run localisation linter (checks for unused/missing locale keys)
just lint-locale

# Everything CI checks, plus the native and mod tests
just check

# Put a rebuilt fa_native.dll and the mod's Lua into the running game, no restart
# (needs the dev marker, `just dev-on`, at the game's start)
just reload
```

`just` with no arguments lists every recipe.

After changing Lua or the native core, prefer `just reload` to restarting the game: it swaps the core in about half a second and runs `/fareload` (`game.reload_mods()`), keeping the save running. Restart instead for changes to `native/src/stub/` (winmm.dll), locale files or the data stage, which a reload does not pick up. See `devdocs/dev-setup.md`.

### Worktrees

Make worktrees with the EnterWorktree tool (or `claude --worktree`), not a plain `git worktree add`. The repo's `WorktreeCreate` hook (`.claude/hooks/worktree.ps1`, registered in `.claude/settings.json`) makes `.claude/worktrees/<name>` on branch `<name>` from the session's HEAD and links in what git leaves out: the `native/vendor` submodules, `.tools` and `.env`, so `just` builds and lints there at once. ExitWorktree's remove runs the `WorktreeRemove` hook, which unlinks those before removing, refuses a worktree with uncommitted work, and deletes the branch once it is merged. After a plain `git worktree add`, run `pwsh .claude/hooks/worktree.ps1 setup <path>`. Never delete a worktree folder recursively by hand: its links point into the main checkout.

### Lint: EmmyLua

`just lint` runs `emmylua_check` (about a second) with `.emmyrc.json`, against the Factorio API library in `.tools/`. `just setup-tools` installs both, pinned, and generates the library from the Steam game's `doc-html` with FMTK; rerun it after a game update. Warnings fail the lint. The same `.emmyrc.json` serves an editor's or Claude Code's `emmylua_ls`. `need-check-nil` and `strict.arrayIndex` are off: the project lets a missing value crash.

Type-checker habits that keep the lint clean:

- Integers are `integer` (indices, counts, `pindex`); Factorio's `uint32` and friends are aliases of it. EmmyLua reads a table indexed by a `uint32` value as nil: copy the index into a local typed `integer?` first.
- A local initialised with `0` and later given fractions needs `---@type number`. A function returning a count needs `---@return integer`, or the type checker takes it for the literal 0.
- A local assigned inside a different closure (tests' `ctx:init`) needs its type declared (`---@type LuaPlayer`), or it reads as nil.
- An API value typed maybe-nil that cannot be nil here gets `assert(...)` where it enters (`assert(game.get_player(pindex))`), never an `if` that skips.
- Positions the game returns are typed "struct or array" but are always the struct: cast where they enter, `entity.position --[[@as MapPosition.struct]]`. A tile position is `fa.Tile`, any point `fa.Point`.
- A lookup table whose missing keys mean "no" is typed with an optional value (`table<string, true?>`), or every lookup reads as always true.
- `MessageBuilder:build()` returns `""` for an empty message; use `build_or_nil()` when the caller needs to know.
- Casts are `--[[@as T]]`; a deliberate exception is `---@diagnostic disable-next-line: <code>` with the reason above it.

### Architecture Overview

**Loading Stages (Critical!)**
1. **Settings Stage** (`settings.lua`) - Defines settings, NO game access
2. **Data Stage** (`data.lua`) - Defines prototypes, NO runtime access
3. **Runtime Stage** (`control.lua`) - Event-driven gameplay logic

**Never access runtime objects (game, script, storage) from data stage!**

### Core Patterns

#### Player Data Access
```lua
local pindex = player.index
local player = game.get_player(pindex)
local player_data = storage.players[pindex]
```

#### Storage Declaration (StorageManager)
```lua
local my_storage = storage_manager.declare_storage_module('module_name', {
    -- default values
})

-- Access: automatically refers to storage.players[pindex].module_name
-- Also handles lazy initialization
local data = my_storage[pindex]
```

#### Localization (MessageBuilder)
```lua
local message = MessageBuilder.new()
message:fragment({"entity-name.transport-belt"})
message:fragment({"fa.direction", "north"})
Speech.speak(pindex, message:build())
```

**WARNING**: `MessageBuilder` handles spaces. `MessageBuilder:fragment(" ")` crashes in order to loudly catch this bug!

Correct:

```
mb:fragment("foo"):fragment("bar")
-- produces foo bar
```

Crashes!:

```
mb:fragment("foo"):fragment(" "):fragment("bar")
-- crashes: spaces were already added!
```

Style rules:

- Don't use `:` `()`.  Screen readers read these symbols verbatim in many setups. Better to use comma (aka list_item) or just leave out the punctuation.
- Don't be verbose
- Avoid emdash
- Avoid unicode
- Prefer MessageBuilder list_item() for managing placement of commas
- `MessageBuilder` and `Speech.speak` live in scripts/speech.lua (`Speech.MessageBuilder`); scripts/localising.lua holds localisation helpers like name fallbacks. Familiarize with both before writing user-facing text.
- Localisation keys should always be in section `fa` and must never contain `.`.  Example: `fa.foo-bar` is good, `fa.foo.bar` is bad.
- When possible, fold things into a parameterized localisation key rather than using `fragment({"fa.key-intro}"):fragment(p1)...`. This allows the word order to be changed in translations.

We are writing for a screen reader.  This means two core principles:

- The sooner a message conveying information varies, the faster the user can keep going.
  - Ex: "cursor anchored" "cursor unanchored" makes the user listen to "cursor".
  - ex: "anchored cursor" "unanchored cursor" lets the user move on as soon as the first syllable.
- Less punctuation is better, unless it's comma or period.  Many setups read colon left paren etc.

## Key Systems

### Scanner System
Lists what is on the surface in categories and subcategories. The list lives in the native DLL, on the scanning client only; moves reach every client through the scanner keys' own `cursor_position`. See `devdocs/scanner.md`.

- **List and moves**: `native/src/scanner.cpp`
- **Lua side**: `scripts/scanner/entrypoint.lua` (keys, landing, speech), `readout.lua`, `extras.lua` (pins, tags, pump spots)

### UI System
FA has no menus of its own. The game's own windows are the UI: the DLL reads them and clicks their widgets (`native/src/screens/`, built on the graph in `native/src/graph/`). What FA adds to a game window is a relative GUI beside it (see "Adding FA's own information to a game window" below). Never add a Lua menu or a key that opens one.

### Native DLL (required)
The mod requires the native DLL in `native/` (deployed as `winmm.dll`, a stub, and `fa_native.dll`, the core it loads). There is no Lua-only mode to keep working. With the dev marker, `just reload` loads a rebuilt core and the mod's Lua (`/fareload`, `game.reload_mods`) into the running game.

- When the game knows something the Lua API cannot read (engine flying text such as "Cannot reach", console messages other than player chat, vanilla GUI text, why an action failed), read it at the source in the native layer.
- Don't write Lua that guesses what happened from indirect events, and don't add Lua fallbacks for when the DLL is missing. Existing Lua guesses are to be replaced by native reads.
- Lua-only APIs that do expose the information (events, getters) remain the right tool for it. This does not apply to UI: a game GUI (window, HUD part such as the quickbar, shortcut bar or crafting queue) gets a native screen that reads and clicks the game's own widgets, even when Lua could read the same data.

#### Adding FA's own information to a game window
When a game window lacks something FA wants to say about its entity (what a belt carries, where a pipe connects, what a pole is wired to), show it in the window as an ordinary mod GUI: a relative GUI element anchored to the window (`player.gui.relative`, `anchor = { gui = defines.relative_gui_type.X, ... }`). Never send it to the DLL through `fa_native`.

- Register the views with `scripts/ui/entity-views.lua`: `EntityViews.register(entity_types, relative_gui_type, function(entity) return views end)`. Each view is a title and columns of cells (LocalisedStrings); the module builds a captioned frame per view to the right of the window as it opens, and destroys it as it closes. See `belt-analyzer.lua`, `fluid-views.lua`, `pole-views.lua`.
- The game moves a window with anchored elements into a `CustomGuiGameGuiWrapper`; every `EntityWindowScreen` unwraps it and reads each element in the wrapper's flows as a stop after the window's own, with the generic walker. No native code is needed per view.
- Every player gets the elements, with the DLL or without, so all peers change their GUI identically. Sighted players see them too: keep them plain and useful.
- The anchor's `relative_gui_type` is the game's window class, not the entity type: pipes and storage tanks share `pipe_gui` (`SingleFluidBoxEntityGui`). `NameToGuiTypeMappingHelpers::buildMapping` in Ghidra lists every class's name.
- `EventManager` keeps one handler per event and kind, and a second registration is an error: add entity types to `entity-views.lua`'s registry, not another `on_gui_opened` handler.

#### Multiplayer: the DLL is client-only
Only the blind player runs the DLL. The server and the other players run the same Lua without it, and every peer must change the game identically or the game desyncs.

- Lua never changes the game (writes `storage`, a player, an entity or a setting, opens or closes a Lua UI) based on whether `fa_native` exists or on anything it returns (`build_direction`, `walking_step`, `held_build`, ...). Those values feed speech and sounds only.
- Guard each `fa_native` call with `if native then`, and let the guard skip only that call.
- The DLL changes the game only through the game's own input (cursor, replayed clicks and keys), which the game sends to the server itself.
- A key that both a Lua UI and a native screen answer is the classic desync: the peers without the DLL open the Lua UI. When a native screen covers a game window, remove the Lua UI for it.

## Testing

### Test Framework
```lua
-- In scripts/tests/my-test.lua
local TestRegistry = require("scripts.test-registry")
local describe, it = TestRegistry.describe, TestRegistry.it

describe("Feature", function()
    it("should work", function(ctx)
        local test_value = 42  -- Use locals, not ctx.state

        ctx:at_tick(5, function()
            ctx:assert_equals(42, test_value)
        end)
    end)
end)
```

**Remember**:
- Add test to `test_files` table in `scripts/test-framework.lua` (around line 29)
- Test mod behavior, not Factorio API
- Use local variables for state

## Quick Reference

### File Locations
- **Main entry**: `control.lua` (use grep/partial reads - it's huge!)
- **Utilities**: `scripts/fa-utils.lua`
- **Entity info**: the DLL's tile reader, `native/src/reader.cpp` and `native/src/facts.cpp` (the cursor, the scanner and pins); Lua names an entity for it with `fa_native.entity_read`
- **Storage**: `scripts/storage-manager.lua`
- **Events**: `scripts/event-manager.lua` (migration in progress)
- **Tests**: `scripts/tests/`

### Common Tasks

#### Add Keybinding
1. Define in `data.lua`: `type = "custom-input"`
2. Handle in `control.lua`: `script.on_event("fa-keyname")`
3. Add locale in `locale/en/locale.cfg` under `[controls]`

Our keys are named `fa-s` (s, with no modifiers) or `fa-flags-s` where flags is c, a, or s (shift), e.g. `fa-cas-s` is s with control+alt+shift.

#### Add Storage Module
```lua
local my_storage = storage_manager.declare_storage_module('my_module', {
    -- defaults
})
```

#### Add a Setting
Settings allow users to configure mod behavior. They're defined in the settings stage and accessed at runtime.

1. **Add declaration** in `scripts/settings-decls.lua`:
   ```lua
   {
      name = "fa-my-setting",
      type = "bool-setting",  -- or "int-setting", "double-setting", "string-setting"
      setting_type = "runtime-per-user",
      default_value = false,
      order = "b",  -- Controls display order in the game's Mod settings window
   },
   ```

2. **Add locale** in `locale/en/settings.cfg`:
   ```ini
   [mod-setting-name]
   fa-my-setting=My setting label

   [mod-setting-description]
   fa-my-setting=Description shown in the Mod settings window
   ```

3. **Read at runtime**:
   ```lua
   local enabled = settings.get_player_settings(pindex)["fa-my-setting"].value
   ```

Players change settings in the game's own Mod settings window (Escape, Settings, Mod settings), which the DLL reads (`native/src/screens/ModSettingsScreen.cpp`). FA has no settings menu of its own. Prefer `runtime-per-user` for preferences such as sounds: a `runtime-global` setting is map-wide and only admins can change it.

## Performance Tips

- Cache globals locally
- Avoid table creation in hot loops
- Use appropriate tick intervals (15, 60, etc.)
- Validate entities: `if entity and entity.valid then`

## Syntrax (Rail Description Language)

Syntrax is not reachable from the game: its menus went with FA's Lua UI, and the vanilla rail planner replaces it. The compiler stays in the top-level `syntrax/` module (lexer/parser/compiler/vm), with a CLI at `syntrax-cli.lua`, its tests, and the runner and custom programs in `scripts/rails/`, until it is ported to act through the game's own input or removed.

## Important Notes

- **Everything** goes through `just`
- Requires only work at file top-level
- No global state beyond current file
- Check `player and player.valid` always
- Never use `game.create_player()` (doesn't exist)
- Use `prototypes.recipe["name"]` not `game.recipe_prototypes`

## External Communication

The mod communicates with a launcher via stdout:
```
out <player_index> <message>
```
This is why `Speech.speak()` is used for user messages.

Remember: This mod makes a visual game accessible through audio. Every feature must be designed with audio-first interaction in mind!

# Common LLM Antipatterns

## Comments Referring To What Changed

**WRONG**:

```
- Removed the old UI system. Now x does y.
```

**CORRECT**: consider whether a comment is required.

**WRONG**:

```
-- Changed to use controllers. Now handles force_close
```

**CORRECT**:

```
-- Can be closed with the controller
```

## Overuse of valid

`x.valid` is how the mod checks whether or not an entity from Factorio can be used without error.  But, it is **WRONG TO
DO THIS AT EVERY LEVEL**.  Before implementing a block of code, **ALWAYS** consider whether or not entity/object
validity has already been checked elsewhere.

## Defensive Coding

**CRITICAL**: Excessive validation hides bugs. Let code crash to find edge cases.

**WRONG**:

```lua
function process_signals(entity)
   if not entity or not entity.valid then return {} end

   local cb = entity.get_control_behavior()
   if not cb then return {} end

   for _, section in ipairs(cb.sections or {}) do
      local count = section.filters_count or 0
      for i = 1, count do
         local slot = section.get_slot(i)
         if slot and slot.value then
            -- process slot
         end
      end
   end
end
```

This hides bugs:
- Returns empty table when entity is invalid (should crash)
- `cb.sections or {}` returns empty on nil (should crash to find why cb.sections is nil)
- `section.filters_count or 0` hides missing filters_count (should crash)
- `if slot and slot.value` silently skips invalid slots (should crash if unexpected)

**CORRECT**:

```lua
function process_signals(entity)
   local cb = entity.get_control_behavior()

   for _, section in ipairs(cb.sections) do
      for i = 1, section.filters_count do
         local slot = section.get_slot(i)
         if slot.value then  -- Only check what's expected to be nil
            -- process slot
         end
      end
   end
end
```

**When to validate:**
- At UI entry points (user can trigger with bad state)
- When nil is a valid expected value (e.g., `slot.value` can legitimately be nil for empty slots)

**When NOT to validate:**
- Internal functions (caller should ensure valid state)
- Properties that should always exist (let it crash to find the bug)
- Returning empty/default silently (masks the real problem)

# Factorio 2.0 API

This is a Factorio 2.0 project, not a Factorio 1.1 project.  Factorio 2.0 comes with many API changes.

A complete reference (one file per class, concept, define, etc) is at `./llm-docs/api-reference`. List this directory recursively for a "table of contents".

Read `./llm-docs/api-reference/CLAUDE.md` for more specific information on browsing this documentation.

You **MUST** double check that you understand APIs before using them.  Your training knowledge cutoff was only barely after the Factorio 2.0 release and a vast majority of your training data refers to 1.1 APIs.  In addition to changes, the 2.0 API adds a lot of new functions and objects which may also simplify mod tasks.

IMPORTANT: the docs are at the root of this repo, `./` relative to your invocation working directory. They are not at the root of the Factorio install.  For example, you can run exactly this command to list all files (thousands of them!):

```
find ./llm-docs -type f
```

Due to the sheer number of files, searching with your built-in search tools is a better approach.  This is just illustrating the locations of files--don't run it unless you're sure you want all that output!

# Quick notes on common patterns

- To present items by quality as a list such as "legendary solar panel x 5", use `present_list` in `inventory-utils.lua` (it rolls the list up by name and quality with `TableHelpers.rollup2`).
- imports are CamelCase, not snake_case or camelCase.
