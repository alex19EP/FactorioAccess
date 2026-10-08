Development build for internal testing, rebuilt on every push to `native`. Not a release.

Built from ${COMMIT_SHORT}: ${COMMIT_SUBJECT}

The game's version text and `factorio-access-native.log` name this build as `+dev.${COMMIT_SHORT}`. Please give that in bug reports.

## Install

You need Factorio 2.1 on Windows. With the game closed:

1. Extract `FactorioAccess-native.zip` into the game folder, the one that holds `bin`. This puts `winmm.dll` in `bin\x64` and adds `factorio-access-keys.cmd`. Replace the files if asked.
2. Delete any older `FactorioAccess_*.zip` from your mods folder, then copy the new `FactorioAccess_*.zip` there.
3. Start the game the usual way, without the old launcher, and exit it from the main menu. The game then lists the mod's controls in its `config.ini`.
4. Run `factorio-access-keys.cmd` from the game folder. It moves the game's controls to the keys the mod expects. Run it again after each update: it changes only what is missing.
5. Start the game.

The full steps and troubleshooting are in [the README of this build](https://github.com/${GITHUB_REPOSITORY}/blob/${GITHUB_SHA}/README.md#installing-factorio-access).

## Reporting problems

Attach `factorio-access-native.log` from `bin\x64` and `factorio-current.log`. That log is in the game folder for the zip version, otherwise in `%APPDATA%\Factorio`.
