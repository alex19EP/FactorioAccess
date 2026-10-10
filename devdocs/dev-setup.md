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
- For `just lint`: lua-language-server and the FMTK type stubs, found through
  `.vscode/settings.json` (see `CONTRIBUTING.md`).

## `.env`

The justfile keeps no paths of its own. It reads them from `.env` in the repo root, which git
ignores. Copy `.env.example` to `.env` and fill it in. Wrap every value in single quotes: the
dotenv parser treats backslashes in unquoted or double-quoted values as escapes.

| Variable | What it is | Used by |
| --- | --- | --- |
| `FACTORIO_STEAM_DIR` | The Steam Factorio folder, the one holding `bin\x64\factorio.exe` | `deploy`, `run`, `symbols`, `log`, `dev-on` |
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
just link-mod   # junction from the Steam game's mods folder to this checkout
just run        # build the DLL, copy it next to the Steam factorio.exe, start the game
```

`just stop` closes the game, `just undeploy` removes the DLL, and `just unlink-mod` removes the
junction.

## Main recipes

| Recipe | What it does |
| --- | --- |
| `just format` | stylua and clang-format, required before committing |
| `just check` | everything CI checks, plus the graph tests and the mod's test suite |
| `just lint` / `just lint-locale` | LuaLS, and unused or missing locale keys |
| `just test-mod [timeout]` | the mod's in-game test suite, headless |
| `just message-lists` | regenerate the message-list locale files, then format |
| `just build` / `just test` | build the DLL, run the graph kernel tests |
| `just symbols` | check the DLL's symbols against the installed Factorio builds |
| `just run` / `just start` / `just stop` | deploy and start, start only, close the game |
| `just loaded` | whether the running game loaded this checkout's DLL |
| `just dev-on` / `just dev-off` | arm or disarm the loopback dev server for the next start |
| `just log` / `just game-log` | the end of the DLL's log and of the game's log |
| `just package` | the mod portal zip, as the publish workflow builds it |
