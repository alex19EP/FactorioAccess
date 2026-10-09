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
   python launch_factorio.py --format
   python launch_factorio.py --run-tests --timeout 60
   python lint_localisation.py lint
   ```

3. **Debug output: Use print(), not game.print()**
   ```lua
   -- WRONG: game.print("debug")  -- Goes to GUI, blind users can't see
   -- RIGHT: print("debug")        -- Goes to logs
   -- RIGHT: Speech.speak(pindex, "user message")  -- Goes to screen reader
   ```

4. **LuaLS annotations: Use three dashes**
   ```lua
   ---@param pindex integer   -- CORRECT
   -- @param pindex integer   -- WRONG (linter will fail)
   ```

## Development Essentials

### Your Environment

You are running in the mods/FactorioAccess directory of a Factorio install. `launch_factorio.py` is in your current working directory.

The full factorio install is available at `../..`. You almost never need to consult it.

Commands should always be relative to the current working directory.

### Key Commands
```bash
# Format code (REQUIRED before committing)
python launch_factorio.py --format

# Run tests
python launch_factorio.py --run-tests --timeout 60

# Run linter
python launch_factorio.py --lint

# Run localisation linter (checks for unused/missing locale keys)
python lint_localisation.py lint
```

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

#### Modern UI Pattern
```lua
local Router = require("scripts.ui.router")
local TabList = require("scripts.ui.tab-list")
local Menu = require("scripts.ui.menu")  -- exposes Menu.MenuBuilder

local my_menu = TabList.declare_tablist({
    ui_name = Router.UI_NAMES.MY_MENU,
    tabs = {
        -- Tab definitions using KeyGraph
    }
})
```

scripts/ui/controls.lua, scripts/ui/form-builder.lua, and scripts/ui/menu.lua (the MenuBuilder) are the major UI entrypoints for implementing a new GUI. scripts/ui/grid.lua is the grid builder.  scripts/ui/router.lua is the entrypoint for non-UI interaction with the UI system, e.g. opening a UI.

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
Modern graph-based architecture:
- **Router**: `scripts/ui/router.lua` - Central UI manager
- **TabList**: Multi-tab support with shared state
- **Builders**: Menu and Grid builders for common patterns
- Dynamic rendering with React-like rebuilding
- Explicit re-render is not possible because the UI does not have a painting step and is really a description of how to say things.  Controls which change values often say their new value.

### Native DLL (required)
The mod requires the native DLL in `native/` (deployed as `winmm.dll`). There is no Lua-only mode to keep working.

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
- **Entity info**: `scripts/fa-info.lua`
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
      order = "b",  -- Controls display order in settings menu
   },
   ```

2. **Add locale** in `locale/en/settings.cfg`:
   ```ini
   [mod-setting-name]
   fa-my-setting=My setting label

   [mod-setting-description]
   fa-my-setting=Description shown in settings menu
   ```

3. **Read at runtime**:
   ```lua
   local enabled = settings.get_player_settings(pindex)["fa-my-setting"].value
   ```

Settings automatically appear in the FA settings menu (opened via keybind). The menu is built dynamically from `settings-decls.lua`.

## Performance Tips

- Cache globals locally
- Avoid table creation in hot loops
- Use appropriate tick intervals (15, 60, etc.)
- Validate entities: `if entity and entity.valid then`

## Syntrax (Rail Description Language)

Syntrax is active. The compiler lives in the top-level `syntrax/` module (lexer/parser/compiler/vm), with a CLI at `syntrax-cli.lua` and runtime wiring under `scripts/rails/` and `scripts/ui/menus/` (router UIs `SYNTRAX_INPUT`, `RAIL_BUILDER`, `SYNTRAX_PROGRAM`).

## Important Notes

- **Everything** goes through `launch_factorio.py`
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

# Message List System for Help and Documentation

The mod includes a message list system for providing help and documentation that integrates with Factorio's localization system.

## Creating Message Lists

1. Create a `.txt` file anywhere under `locale/<language>/` (e.g., `locale/en/ui-help/my-feature.txt` or `locale/en/docs/controls.txt`)
2. Write messages separated by blank lines:
   ```
   ; Comments start with semicolon
   This is the first message.

   This is the second message.
   ```
3. Run `python build_message_lists.py` to generate locale files
4. The message list name is the basename of the file (without `.txt`)
5. Message list names must be globally unique across all directories

**Windows gotcha**: `build_message_lists.py` writes `scripts/message-list-index.lua` with CRLF line endings, which fails `--format-check` (stylua wants LF). git's `autocrlf` hides this in `git diff`, so always run `python launch_factorio.py --format` after regenerating.

## Using Message Lists in UIs

```lua
local Help = require("scripts.ui.help")

-- In your KeyGraph declaration or TabList callbacks:
get_help_metadata = function(ctx)
   return {
      Help.message_list("my-feature"),  -- From my-feature.txt
      Help.message({"fa.some-other-message"}),  -- Direct localised string
   }
end
```

## How Users Access Help

- Press `Shift+/` (question mark) while in a UI to open help
- Use W/S to navigate between help messages
- Press `Shift+/` again or `E` to close help

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

- Before performing aggregations of items by quality to produce lists such as "legendary solar panel x 5", read scripts/item-stack-utils.lua to learn about aggregation functions that already exist.
- See `inventory-utils.lua` for inventory-like list presentation helpers.
- imports are CamelCase, not snake_case or camelCase.
