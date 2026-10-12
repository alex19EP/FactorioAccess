# Dev setup and the justfile

The repo-root `justfile` is the entry point for every development task: building and deploying
the native DLL, launching the game, formatting, linting and testing. Run `just` for the list of
recipes. The recipes call the Python helpers (`launch_factorio.py`, `lint_localisation.py`,
`build_message_lists.py`), which still work on their own and are where the logic lives.

## Requirements

- [just](https://github.com/casey/just) and PowerShell 7 (`pwsh`). Every recipe runs in `pwsh`.
- Python 3.11 or newer on the path.
- For the DLL: Visual Studio 2026 or its Build Tools with the C++ workload, CMake 3.28 or newer,
  and Ninja. See `native/README.md`.
- For formatting: stylua 2.3.0 built with `--features lua52`, and clang-format 20 or newer.
- For `just lint`: `just setup-tools` once, which needs Node.js (`npx`) and the network. It puts
  pinned EmmyLua releases (`emmylua_check`, `emmylua_ls`) and the Factorio API library, which FMTK
  generates from the Steam game's `doc-html`, into `.tools\` (ignored by git). Run it again after
  a game update.

## `.env`

The justfile keeps no paths of its own. It reads them from `.env` in the repo root, which git
ignores. Copy `.env.example` to `.env` and fill it in. Wrap every value in single quotes: the
dotenv parser treats backslashes in unquoted or double-quoted values as escapes.

| Variable | What it is | Used by |
| --- | --- | --- |
| `FACTORIO_STEAM_DIR` | The Steam Factorio folder, the one holding `bin\x64\factorio.exe` | `deploy`, `run`, `reload`, `symbols`, `log`, `dev-on` |
| `FACTORIO_USER_DIR` | The Steam game's write-data folder, usually `%APPDATA%\Factorio` | `link-mod`, `game-log` |
| `VCVARS64` | Full path to `vcvars64.bat` from the Visual Studio install | `configure`, `build` and all that build |
| `FACTORIO_SANDBOX_DIR` | Optional. A zip Factorio install whose `mods\FactorioAccess` is a junction to this checkout | `test-mod`, `symbols` |
| `FACTORIO_STEAM_APPID` | Optional. Defaults to 427520 | `run`, `start` |

A recipe that needs a variable that is not set stops with a message naming it.

Without `FACTORIO_SANDBOX_DIR`, `just test-mod` runs the tests in the Factorio install this
checkout sits in: clone the repo to `mods\FactorioAccess` of a zip Factorio, as `CONTRIBUTING.md`
describes.

## First run

```
just setup-tools  # the Lua lint and language server, into .tools\
just link-mod   # junction from the Steam game's mods folder to this checkout
just dev-on     # arm the dev server, which also lets the game reload the DLL
just run        # build the DLLs, copy them next to the Steam factorio.exe, start the game
```

`just stop` closes the game, `just undeploy` removes the DLLs, and `just unlink-mod` removes the
junction.

## Worktrees

Claude Code's worktrees (EnterWorktree, `claude --worktree`) go through the repo's
`WorktreeCreate` and `WorktreeRemove` hooks, `.claude/hooks/worktree.ps1`. A new worktree,
`.claude/worktrees/<name>` on branch `<name>`, gets junctions to the main checkout's
`native/vendor` submodules and `.tools`, and a hard link to its `.env`, so every recipe works in
it. Removing unlinks them first and keeps a worktree with uncommitted work. For a worktree made
with `git worktree add`, run `pwsh .claude/hooks/worktree.ps1 setup <path>`.

## Changing code while the game runs

With the dev server armed, `just reload` rebuilds, puts the new `fa_native.dll` into the running
game (the game swaps it in within about a second, keeping the save running), then types
`/fareload`, which runs `game.reload_mods()`: every mod's Lua runs again from the files on disk,
with `storage` kept, as after a save and load. Restart the game instead after changing `winmm.dll`
(`native/src/stub/`, which `just reload` warns about), locale files, or the data stage. The
reloaded Lua comes from the mods folder, so from the checkout `just link-mod` linked, whatever
worktree the DLL was built in. `/fareload` does nothing in multiplayer.

## A game an AI agent drives

A game Claude Code launches with `just run` or `just start` starts muted: what it says goes to the
DLL's log and the dev server's `/speech`, but not to the screen reader of the developer at the same
desk. Claude's commands and the ones you type with `!` have the same environment, so the repo's
PreToolUse hook (`.claude/hooks/agent-launch.sh`, which Claude Code runs for its own commands only)
leaves a flag in `%LOCALAPPDATA%\FactorioAccess` when Claude launches. The recipe turns a fresh flag
into `factorio-access-mute.once` beside the DLL, which the DLL deletes as it starts, so only that
launch is muted. A game started straight from a Claude Code shell (the DRM-free sandbox) sees
`AI_AGENT` ending in `_agent` itself; so does one you start with `!` that way. `FA_MUTE=1` or `0`
overrides both. `just unmute` and `just mute` switch a running game, and a reloaded core keeps the
choice.

## Main recipes

| Recipe | What it does |
| --- | --- |
| `just format` | stylua and clang-format, required before committing |
| `just check` | everything CI checks, plus the graph tests and the mod's test suite |
| `just lint` / `just lint-locale` | EmmyLua (about a second), and unused or missing locale keys |
| `just setup-tools` | install or update EmmyLua and the Factorio API library in `.tools\` |
| `just test-mod [timeout]` | the mod's in-game test suite, headless |
| `just message-lists` | regenerate the message-list locale files, then format |
| `just build` / `just test` | build the DLL, run the graph kernel tests |
| `just symbols` | check the DLL's symbols against the installed Factorio builds |
| `just run` / `just start` / `just stop` | deploy and start, start only, close the game |
| `just reload` | build, load the new core and the mods' Lua into the running game |
| `just loaded` | whether the running game loaded this checkout's DLLs |
| `just dev-on` / `just dev-off` | arm or disarm the loopback dev server for the next start |
| `just mute` / `just unmute` | keep the running game's speech off the screen reader, or let it through |
| `just log` / `just game-log` | the end of the DLL's log and of the game's log |
| `just package` | the mod portal zip, as the publish workflow builds it |
