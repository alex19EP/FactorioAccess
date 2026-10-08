Development build for internal testing, rebuilt on every push to `native`. Not a release.

Built from ${COMMIT_SHORT}: ${COMMIT_SUBJECT}

The game's version text and `factorio-access-native.log` name this build as `+dev.${COMMIT_SHORT}`. Please give that in bug reports.

## Install

You need Factorio 2.1 on Windows. Download `winmm.dll`, `winmm.pdb` and the `FactorioAccess_*.zip` below. With the game closed, put the DLL and the pdb in the game's `bin\x64` folder, next to `factorio.exe`. Put the mod zip in your mods folder, after deleting older `FactorioAccess_*.zip` from it. Then start the game the usual way, without the old launcher.

The full steps, the game settings the launcher used to change for you, and troubleshooting are in [the README of this build](https://github.com/${GITHUB_REPOSITORY}/blob/${GITHUB_SHA}/README.md#installing-factorio-access).

## Reporting problems

Attach `factorio-access-native.log` from `bin\x64` and `factorio-current.log`. That log is in the game folder for the zip version, otherwise in `%APPDATA%\Factorio`.
