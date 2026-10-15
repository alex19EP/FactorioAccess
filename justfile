set shell := ["pwsh.exe", "-NoLogo", "-NoProfile", "-Command"]
set windows-shell := ["pwsh.exe", "-NoLogo", "-NoProfile", "-Command"]

# Load machine-specific paths from `.env` (gitignored).  Every absolute path
# this justfile needs lives there — never hardcode a path here.  Copy
# `.env.example` to `.env` on a fresh machine and fill it in; see
# devdocs/dev-setup.md.
set dotenv-load

NATIVE    := "native"
BUILD_DIR := NATIVE / "build"
BIN_DIR   := BUILD_DIR / "RelWithDebInfo"

# All path-bearing variables come from .env via env_var_or_default so that
# `just --list` still works on a machine without a populated .env; the recipes
# that actually need a path depend on the matching `_require-*` guard.
STEAM_DIR   := env_var_or_default("FACTORIO_STEAM_DIR", "")
SANDBOX_DIR := env_var_or_default("FACTORIO_SANDBOX_DIR", "")
USER_DIR    := env_var_or_default("FACTORIO_USER_DIR", "")
VCVARS_RAW  := env_var_or_default("VCVARS64", "")
STEAM_APPID := env_var_or_default("FACTORIO_STEAM_APPID", "427520")

# vcvars64.bat lives under "Program Files (x86)", so the path contains a space;
# quote it once here so every consumer can splat it raw.
VCVARS := '"' + VCVARS_RAW + '"'

# cl writes its /showIncludes notes in the console's code page (localised in a non-English Visual
# Studio), CMake keeps the prefix it saw when it configured, and ninja records a header dependency
# only where the bytes match. Without one code page for every configure and build, a tree set up
# from one terminal and built from another records no header dependencies, and objects go stale.
CODEPAGE := 'chcp 65001 >nul'

# List the recipes
default:
    @just --list

# --- native DLL ---------------------------------------------------------------

# Ninja calls cl directly, so configuring and building both run inside vcvars64.

# Configure the native build tree (Ninja Multi-Config, see CMakePresets.json)
configure: _require-vcvars
    @cmd.exe /c '{{CODEPAGE}} && {{VCVARS}} >nul 2>&1 && cmake -S {{NATIVE}} --preset default'

# Build winmm.dll, fa_native.dll, the native tests and the symbol checker (RelWithDebInfo)
build: _ensure-configured _require-vcvars
    @cmd.exe /c '{{CODEPAGE}} && {{VCVARS}} >nul 2>&1 && cmake --build {{BUILD_DIR}} --config RelWithDebInfo'

# Run the graph kernel conformance suite (pure STL, never touches the game)
test: build
    @ctest --test-dir {{BUILD_DIR}} -C RelWithDebInfo --output-on-failure

# Run this first after a Factorio update: a FAILED here is exactly what would
# make the DLL stand down in game.

# Resolve every symbol and layout the DLL needs against the Steam and sandbox builds
symbols: build _require-steam
    @$ErrorActionPreference = 'Stop'; \
     $exes = @('{{STEAM_DIR}}\bin\x64\factorio.exe'); \
     if ('{{SANDBOX_DIR}}') { $exes += '{{SANDBOX_DIR}}\bin\x64\factorio.exe' }; \
     $failed = $false; \
     foreach ($exe in $exes) { \
         Write-Host "[just] $exe"; \
         & '{{BIN_DIR}}/fa_symbols_check.exe' $exe 2>&1 | Select-Object -Last 2; \
         if ($LASTEXITCODE -ne 0) { $failed = $true } \
     }; \
     if ($failed) { exit 1 }

# compile_commands.json for clangd (Ninja, so it needs the VS environment)
clangd: _require-vcvars
    @cmd.exe /c '{{CODEPAGE}} && {{VCVARS}} >nul 2>&1 && cmake -S {{NATIVE}} --preset clangd'

# Remove the native build trees
clean:
    @foreach ($d in @('{{BUILD_DIR}}', '{{NATIVE}}/build-clangd')) { \
         if (Test-Path $d) { Remove-Item -Recurse -Force $d; Write-Host "[just] removed $d" } \
     }

# Clean, then build
rebuild: clean build

# --- mod ----------------------------------------------------------------------

# Apply stylua and clang-format formatting (required before committing)
format:
    @python launch_factorio.py --format

# Check formatting without changing anything
format-check:
    @python launch_factorio.py --format-check

# Run the Lua linter (emmylua_check)
lint:
    @python launch_factorio.py --lint

# Install emmylua_check, emmylua_ls and the Steam game's API library into .tools\ (again after a game update)
setup-tools: _require-steam
    @python launch_factorio.py --factorio-path '{{STEAM_DIR}}\bin\x64\factorio.exe' --setup-tools; exit $LASTEXITCODE

# Check for unused and missing locale keys
lint-locale:
    @python lint_localisation.py lint

# Test the dev updater with isolated portable and Steam fixtures, without network or a game
test-updater:
    @powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File helper-scripts/tests/update-tests.ps1
    @pwsh.exe -NoLogo -NoProfile -File helper-scripts/tests/update-tests.ps1

# With FACTORIO_SANDBOX_DIR set, the tests run in that install, whose
# mods\FactorioAccess is a junction to this checkout.  Without it they run in
# the install this checkout sits in (mods\FactorioAccess of a zip Factorio).

# Run the mod's in-game test suite headless
test-mod timeout="300":
    @$exe = if ('{{SANDBOX_DIR}}') { @('--factorio-path', '{{SANDBOX_DIR}}\bin\x64\factorio.exe') } else { @() }; \
     python launch_factorio.py @exe --run-tests --timeout {{timeout}}; exit $LASTEXITCODE

# Everything CI checks, plus the native tests
check: format-check lint lint-locale test test-mod test-updater

# Build the mod portal zip the same way the publish workflow does
package:
    @npx --yes factoriomod-debug package

# --- deploying ----------------------------------------------------------------

# factorio.exe imports winmm and it is not a KnownDLL, so the copy in bin\x64
# is the one the loader picks; it loads fa_native.dll, the core, beside it.
# The pdbs go along for crash symbols.

# Build, then copy winmm.dll and fa_native.dll next to the Steam factorio.exe
deploy: build _require-steam _require-not-running
    @$ErrorActionPreference = 'Stop'; \
     Copy-Item -Force '{{BIN_DIR}}/winmm.dll', '{{BIN_DIR}}/winmm.pdb', '{{BIN_DIR}}/fa_native.dll', '{{BIN_DIR}}/fa_native.pdb' '{{STEAM_DIR}}\bin\x64\'; \
     Write-Host '[just] deployed winmm.dll + fa_native.dll and their pdbs into {{STEAM_DIR}}\bin\x64'

# Remove the DLLs from the Steam install; the game runs vanilla again
undeploy: _require-steam _require-not-running
    @foreach ($f in 'winmm.dll', 'winmm.pdb', 'fa_native.dll', 'fa_native.pdb') { \
         Remove-Item -Force -ErrorAction SilentlyContinue "{{STEAM_DIR}}\bin\x64\$f" \
     }; \
     Remove-Item -Force -ErrorAction SilentlyContinue '{{STEAM_DIR}}\bin\x64\fa_native.live-*.dll'; \
     Write-Host '[just] removed winmm.dll + fa_native.dll and their pdbs from {{STEAM_DIR}}\bin\x64'

# With the dev marker, the stub (native/src/stub/stub.cpp) loads a copy of
# fa_native.dll and loads the new one whenever the file changes, so the core
# can be replaced while the game runs.  winmm.dll itself only loads at a start.
# /fareload (scripts/fa-commands.lua) then runs game.reload_mods: every mod's
# control stage again, from the mods folder (the main checkout's junction).
# Locale and data stage changes still need a restart.

# Build, then load the new fa_native.dll and the mod's Lua into the running game
reload: build _require-steam
    @$ErrorActionPreference = 'Stop'; \
     $bin = '{{STEAM_DIR}}\bin\x64'; \
     if (-not (Get-Process factorio -ErrorAction SilentlyContinue)) { Write-Error '[just] Factorio is not running; `just run` starts it'; exit 1 }; \
     if (-not (Test-Path "$bin\factorio-access-dev.enable")) { Write-Error '[just] the dev marker is off, so the game holds fa_native.dll itself; `just dev-on`, then restart the game'; exit 1 }; \
     $same = { param($f) (Test-Path "$bin\$f") -and (Get-FileHash "{{BIN_DIR}}/$f").Hash -eq (Get-FileHash "$bin\$f").Hash }; \
     if (-not (& $same 'winmm.dll')) { Write-Warning '[just] winmm.dll changed too; the game loads it only at its next start (`just stop`, `just run`)' }; \
     if (& $same 'fa_native.dll') { Write-Host '[just] fa_native.dll unchanged' } else { \
         $log = "$bin\factorio-access-native.log"; \
         $count = { @(Select-String -Path $log -Pattern '\[stub\] Reload took').Count }; \
         $before = & $count; \
         Copy-Item -Force '{{BIN_DIR}}/fa_native.dll', '{{BIN_DIR}}/fa_native.pdb' "$bin\"; \
         $deadline = (Get-Date).AddSeconds(15); \
         while ((& $count) -eq $before) { \
             if ((Get-Date) -gt $deadline) { Write-Error '[just] the game did not load the new fa_native.dll; see `just log`'; exit 1 }; \
             Start-Sleep -Milliseconds 250 \
         }; \
         Write-Host '[just] fa_native.dll reloaded' \
     }; \
     $port = if ($env:FA_DEV_PORT) { $env:FA_DEV_PORT } else { '8773' }; \
     curl.exe -s --data 'grave text=/fareload enter' "http://localhost:$port/key" | Out-Null; \
     Write-Host '[just] /fareload sent: the mods reload their Lua'

# A junction rather than a copy, so edits in this repo go live on the next game
# start without recopying anything.  Needs no elevation.

# Link this checkout into the Steam game's mods folder
link-mod: _require-user-dir
    @$ErrorActionPreference = 'Stop'; \
     $link = Join-Path '{{USER_DIR}}' 'mods\FactorioAccess'; \
     if (Test-Path $link) { Write-Host "[just] $link already exists"; exit 0 }; \
     New-Item -ItemType Directory -Force (Split-Path $link) | Out-Null; \
     New-Item -ItemType Junction -Path $link -Target '{{justfile_directory()}}' | Out-Null; \
     Write-Host "[just] linked $link -> {{justfile_directory()}}"

# Remove that junction again (the checkout itself is untouched)
unlink-mod: _require-user-dir
    @$link = Join-Path '{{USER_DIR}}' 'mods\FactorioAccess'; \
     $item = Get-Item -Force -ErrorAction SilentlyContinue $link; \
     if (-not $item) { Write-Host '[just] FactorioAccess was not linked'; exit 0 }; \
     if (-not $item.LinkType) { Write-Error "[just] $link is a real folder, not a link; leaving it alone"; exit 1 }; \
     $item.Delete(); Write-Host "[just] removed $link"

# Deploy the DLL and link the mod
install: deploy link-mod

# Undeploy the DLL and unlink the mod
uninstall: undeploy unlink-mod

# --- running ------------------------------------------------------------------

# Launching through Steam keeps the overlay, cloud saves and achievements as a
# player has them, and applies the Launch Options from the game's Properties.

# Deploy the fresh DLL, then launch Factorio through Steam
run: deploy _mute-for-agent
    @Start-Process 'steam://run/{{STEAM_APPID}}'; \
     Write-Host '[just] launched Factorio via Steam (app {{STEAM_APPID}}); `just loaded` once it is up'

# Launch through Steam without rebuilding or redeploying
start: _require-not-running _mute-for-agent
    @Start-Process 'steam://run/{{STEAM_APPID}}'; \
     Write-Host '[just] launched Factorio via Steam (app {{STEAM_APPID}})'

# A game Claude launches starts muted: its speech reaches the log and the dev server's /speech but
# not the screen reader of the developer at the same desk. The flag comes from the PreToolUse hook
# .claude/hooks/agent-launch.sh, which runs for Claude's own commands and never for one the
# developer types (`! just run`); a flag older than the longest build is left over from a launch
# that never got here. The DLL deletes the marker as it starts, so only that launch is muted
# (native/src/speech.h), and a launch of the developer's own removes one a failed launch left.
_mute-for-agent: _require-steam
    @$marker = '{{STEAM_DIR}}\bin\x64\factorio-access-mute.once'; \
     $flag = Get-Item -ErrorAction SilentlyContinue (Join-Path $env:LOCALAPPDATA 'FactorioAccess\agent-launch'); \
     if ($flag) { Remove-Item -Force $flag }; \
     if ($flag -and $flag.LastWriteTime -gt (Get-Date).AddMinutes(-15)) { \
         New-Item -Force -ItemType File $marker | Out-Null; \
         Write-Host '[just] launched by Claude, so the game starts muted; just unmute lets its speech through' \
     } elseif (Test-Path $marker) { Remove-Item -Force $marker }

# Keep the running game's speech off the screen reader (log and dev server only)
mute:
    @$port = if ($env:FA_DEV_PORT) { $env:FA_DEV_PORT } else { '8773' }; \
     curl.exe -s "http://localhost:$port/mute?on=1"

# Let the running game's speech through to the screen reader again
unmute:
    @$port = if ($env:FA_DEV_PORT) { $env:FA_DEV_PORT } else { '8773' }; \
     curl.exe -s "http://localhost:$port/mute?on=0"

# Close the running game (unsaved progress is lost)
stop:
    @$p = Get-Process factorio -ErrorAction SilentlyContinue; \
     if (-not $p) { Write-Host '[just] Factorio is not running'; exit 0 }; \
     $p | Stop-Process; $p | Wait-Process -Timeout 30 -ErrorAction SilentlyContinue; \
     Write-Host '[just] Factorio stopped'

# Report whether our winmm.dll and its core are loaded in the running game
loaded:
    @$p = Get-Process factorio -ErrorAction SilentlyContinue; \
     if (-not $p) { Write-Host '[just] Factorio is not running'; exit 0 }; \
     $m = $p.Modules | Where-Object { $_.FileName -like '*\bin\x64\winmm.dll' } | Select-Object -First 1; \
     if ($m) { Write-Host "[just] LOADED: $($m.FileName)" } \
     else { Write-Host '[just] our winmm.dll is NOT loaded (only the system one, if any)' }; \
     $p.Modules | Where-Object { $_.ModuleName -like 'fa_native*' } | ForEach-Object { Write-Host "[just] LOADED: $($_.FileName)" }

# The dev server (native/src/devserver.h) starts only when this marker sits next
# to the DLL, so a player's install never has it.

# Arm the loopback dev server (http://localhost:8773) for the next game start
dev-on: _require-steam
    @New-Item -Force -ItemType File '{{STEAM_DIR}}\bin\x64\factorio-access-dev.enable' | Out-Null; \
     Write-Host '[just] dev server armed; it starts with the next game launch'

# Disarm the dev server
dev-off: _require-steam
    @Remove-Item -Force -ErrorAction SilentlyContinue '{{STEAM_DIR}}\bin\x64\factorio-access-dev.enable'; \
     Write-Host '[just] dev server disarmed'

# Show the end of the native DLL's log (written next to the DLL, fresh per game start)
log lines="60": _require-steam
    @Get-Content -Tail {{lines}} '{{STEAM_DIR}}\bin\x64\factorio-access-native.log'

# Show the end of the game's own log
game-log lines="60": _require-user-dir
    @Get-Content -Tail {{lines}} '{{USER_DIR}}\factorio-current.log'

# --- internals ----------------------------------------------------------------

_ensure-configured:
    @if (-not (Test-Path '{{BUILD_DIR}}/CMakeCache.txt')) { just --justfile '{{justfile()}}' configure }

# The loader holds winmm.dll open while the game runs, so a copy would fail
# halfway; refuse up front instead.
_require-not-running:
    @if (Get-Process factorio -ErrorAction SilentlyContinue) { Write-Error '[just] Factorio is running - close it first (`just stop`)'; exit 1 }

# --- env guards ---------------------------------------------------------------
# Each path-bearing recipe depends on the matching guard below.  The guard fails
# fast with an actionable message if .env has not been populated, so nobody has
# to think about paths — only target names.

_require-steam:
    @if (-not '{{STEAM_DIR}}') { Write-Error '[just] FACTORIO_STEAM_DIR is not set in .env — the Steam Factorio folder, the one holding bin\x64\factorio.exe.'; exit 2 }; \
     if (-not (Test-Path '{{STEAM_DIR}}\bin\x64\factorio.exe')) { Write-Error '[just] FACTORIO_STEAM_DIR has no bin\x64\factorio.exe: {{STEAM_DIR}}'; exit 2 }

_require-user-dir:
    @if (-not '{{USER_DIR}}') { Write-Error '[just] FACTORIO_USER_DIR is not set in .env — the Steam game''s write-data folder, %APPDATA%\Factorio, holding mods\ and factorio-current.log.'; exit 2 }; \
     if (-not (Test-Path '{{USER_DIR}}')) { Write-Error '[just] FACTORIO_USER_DIR points at a folder that does not exist: {{USER_DIR}}'; exit 2 }

_require-vcvars:
    @if (-not '{{VCVARS_RAW}}') { Write-Error '[just] VCVARS64 is not set in .env — path to vcvars64.bat from your Visual Studio Build Tools install.'; exit 2 }
