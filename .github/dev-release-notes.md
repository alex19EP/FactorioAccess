Development build for internal testing, rebuilt on every push to `native`. Not a release.

Built from ${COMMIT_SHORT}: ${COMMIT_SUBJECT}

The game's version text and `factorio-access-native.log` name this build as `+dev.${COMMIT_SHORT}`. Please give that in bug reports.

## Install

You need Factorio 2.1 on Windows. The Steam and the zip versions both work.

1. Close Factorio.
2. Copy `winmm.dll` and `winmm.pdb` into the game's `bin\x64` folder, next to `factorio.exe`. For Steam that is usually `C:\Program Files (x86)\Steam\steamapps\common\Factorio\bin\x64`. Replace the files if they are already there.
3. Delete any older `FactorioAccess_*.zip` from your mods folder, then copy the new `FactorioAccess_*.zip` there. The mods folder is `%APPDATA%\Factorio\mods`, or `mods` inside the game folder for the zip version.
4. Start Factorio as usual, without the old launcher. Speech goes to your screen reader.

To uninstall the DLL, delete `winmm.dll` and `winmm.pdb` from `bin\x64`.

## Settings the launcher used to change

The launcher is not used any more, so set these yourself if you have not already. Edit `config.ini` in `%APPDATA%\Factorio\config`, or in `config` inside the game folder for the zip version, while the game is closed. Each line goes under the section shown in brackets.

```
[other]
check-updates=false
enable-mod-settings-load-save-confirmation=false

[interface]
active-quick-bars=1
shortcut-bar-rows=1
show-tips-and-tricks-notifications=false

[controls]
toggle-map-alternative=
toggle-driving-alternative=

[input]
pipette=
smart-pipette=
```

## Reporting problems

Attach `factorio-access-native.log` from `bin\x64` and `factorio-current.log` from `%APPDATA%\Factorio`.
