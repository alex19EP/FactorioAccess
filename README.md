# Welcome to Factorio Access BETA!

This is an accessibility mod for the popular game Factorio which enables blind people to play and win the game.  It supports almost all game features including advanced things such as the circuit network.
We have had players successfully build post-endgame bases with it in a reasonable amount of time.

# Important Warnings

The mod tries to support multiplayer but game limitations mean that your experience will be laggy and kind of terrible. We cannot do anything about this.  We also have limited ability to do anything about bugs, because that would require two mod developers playing at the same time.

The mod needs Factorio 2.1 on Windows. It comes with a DLL, `winmm.dll`, that the game loads from its own folder. The DLL reads the game's screens and speaks through your screen reader. [The DLL's README](native/README.md) says what it does, how it loads, and how to build it. An antivirus may flag it, because it hooks into the game. We build it from source in a GitHub Actions runner, so we are confident that our build is clean. If you need to, add your Factorio folder as an exception in Windows Defender.

The DLL turns off the game's automatic crash log upload. Please report crashes to us, not to Wube, because a game with the DLL loaded is not vanilla.

Linux and macOS are not supported by this version. On those systems, use FactorioAccess 0.16 with the launcher on Factorio 2.0, as described in [the README of the main branch](https://github.com/Factorio-Access/FactorioAccess/blob/main/README.md).

Finally, we do not yet support Space Age. If you bought Space Age, you need to disable quality, elevated rails, and Space Age in the Mods menu of the game's main menu.

# Installing Factorio

The game can be purchased from Factorio.com or from Steam. Any purchase gives access to all versions. We recommend installing it using ONLY one of the options below. The zip version keeps the game, its mods and its settings in one folder, which makes troubleshooting easiest. Steam is easier for multiplayer and updates itself. The mod needs Factorio 2.1 or newer.

## Windows Zip Version / Standalone Version (recommended for easy mod install and troubleshooting)

1. Go to https://www.factorio.com/download
1. If needed, login using your Factorio account or Steam account.
1. Among the install options, find version 2.1 or newer.
1. Go to the section with "Other download options" and select "Download full game for Windows (ZIP package)". This will download a zip file that is about 1.5 gigabytes in size. It might also be called the manual install. Note that this is different from the regular Windows version, which downloads an exe file.
1. If you got a .exe or are running an installer at any point in the process you got the wrong thing
1. Create a folder where you want to keep the game. Extract the zip file into this folder.
1. If you want, create a desktop shortcut for your Factorio folder.
1. All done! You need to install the mod next.

## Steam Version (better for multiplayer setup)

1. Install Factorio using Steam settings, like any other game on Steam.
1. If Steam installs a version older than 2.1, open the Properties menu for Factorio, go to the "Betas" section, and pick the newest version there.
1. All done! You need to install the mod next.

## Regular Windows Version (not recommended)

1. Consider if you need to do this. It has no advantage over the standalone zip at all.
1. Go to https://www.factorio.com/download
1. If needed, login using your Factorio account or Steam account.
1. Among the install options, find version 2.1 or newer.
1. Select "Download full game for Windows". This will download an "exe" file which is the setup application.
1. Run the exe file and follow the instructions.
1. All done! You need to install the mod next.

# Installing Factorio Access

The mod comes in two parts, and you need both. Both are attached to each release on [the releases page](../../releases).

- `FactorioAccess_<version>.zip` is the mod itself. It goes in the mods folder as it is, without extracting it.
- `FactorioAccess-native.zip` holds the DLL, `bin\x64\winmm.dll`, and the keys script, `factorio-access-keys.cmd`. It is extracted into the game folder.

The same steps work for the zip, Steam and regular Windows versions of the game.

1. If you use the old launcher, stop using it. The game is now started the usual way. If you use Steam and put the launcher in Factorio's Launch Options, open the Properties menu for Factorio and clear the "Launch Options" field in the "General" section.
1. Run the game at least once if you have not, and then exit it. This creates the folders for the next steps.
1. Download both files from the release you want.
1. Find your game folder, the one that holds the folder `bin`.
   - Zip version: the folder you extracted the game into.
   - Steam: in your Steam Library, open the "Manage" menu for Factorio and select "Browse local files". This is usually `C:\Program Files (x86)\Steam\steamapps\common\Factorio`.
   - Regular Windows version: usually `C:\Program Files\Factorio`. Windows asks for administrator permission to change files there.
1. Extract `FactorioAccess-native.zip` into the game folder, and replace the files if asked. `winmm.dll` and `winmm.pdb` land in `bin\x64`, next to `factorio.exe`, and `factorio-access-keys.cmd` and `factorio-access-keys.ps1` land in the game folder itself.
1. Find your mods folder.
   - Zip version: the folder `mods` inside the game folder.
   - Steam and regular Windows version: `%AppData%\Factorio\mods`. You can paste this path into the address bar of File Explorer. The full path is something like `C:\Users\Your_User_Name_Here\AppData\Roaming\Factorio\mods`.
1. Delete any older `FactorioAccess_*.zip` from the mods folder, then copy the new one there.
1. Start the game the usual way, from Steam, a shortcut or `factorio.exe`. Speech goes to your screen reader as the game loads. When the main menu appears, exit the game. The game now lists the mod's controls in its settings file.
1. Run `factorio-access-keys.cmd` from the game folder. It moves the game's controls to the keys the mod expects. See "Keys" below.
1. Start the game again. It is ready to play.
1. If you bought Space Age, go into the Mods menu of the game's main menu and disable elevated rails, quality, and Space Age.

To update, close the game, extract the new `FactorioAccess-native.zip` into the game folder and replace the mod zip in the mods folder. Always update both parts together. A DLL and a mod from different releases may not work together. Then start the game once, exit it, and run `factorio-access-keys.cmd` again, so new controls get their keys too.

To uninstall the DLL, delete `winmm.dll` and `winmm.pdb` from `bin\x64`. The mod does not work without it.

## Keys

`factorio-access-keys.cmd` changes the game's controls in its settings file, `config.ini`, so they do not collide with the mod and every mouse action has a key:

- Walking moves from W, A, S and D to the arrow keys, leaving W, A, S and D to the cursor.
- Connect and disconnect train move from J and K to CONTROL + J and CONTROL + K.
- Research moves from T to CONTROL + T, leaving T to the time of day.
- Zoom in and zoom out get EQUALS and MINUS.
- Every control on a mouse button also gets a key: left button LEFTBRACKET, right button RIGHTBRACKET, middle button BACKSLASH, with the same modifiers. The key goes in the control's free second slot, so the mouse keeps working.

Close the game before running it, since the game rewrites `config.ini` when it exits. It works out which `config.ini` the game uses: for the zip version the one in the game folder, for Steam and the regular Windows version the one in `%AppData%\Factorio\config`. It says what it changed, and keeps a copy of the old file beside it. Running it again changes only what is missing.

To undo the changes, run it with `-Revert` from a command prompt in the game folder: `factorio-access-keys.cmd -Revert`. If you run the script from somewhere other than the game folder, give it the game folder: `factorio-access-keys.cmd -GameDir "D:\Games\Factorio"`.

## Troubleshooting

- The DLL writes `factorio-access-native.log` next to itself in `bin\x64`. When something goes wrong, send us that file and `factorio-current.log`. That log is in the game folder for the zip version, otherwise in `%AppData%\Factorio`.
- If the game starts with no speech, check that `winmm.dll` is in `bin\x64` and not in `bin` or the game folder.
- If the DLL says that it does not support this Factorio version, a game update changed something the DLL needs. Wait for a new release of the DLL.

# Mod Documentation

There are a huge number of keystrokes and functions in the mod. To help keep this README reasonable, we have divided them into other documents:

- [Basics](docs/features/basics.md) explains walking, the cursor, and building.
- [Combat, Vehicles, and Military](./docs/features/combat.md)
- [Ui](docs/features/ui.md) explains how menus and other controls work
- [Blueprints and Planners](docs/features/blueprints-and-planners.md) explains how to use blueprints, blueprint books, deconstruction planners, and upgrade planners.
- [Circuit Network](docs/features/circuit-network.md) explains functionality applying specifically to the circuit network, such as how to drag wires.

Please note that the mod wiki is out of date. The above documentation is maintained by developers and is for 2.0. The wiki is by users and primarily for 1.1.

# Vanilla Mode

Vanilla Mode is for sighted people to play using mouse controls without disabling the mod. The most likely use case for
this is when you play multiplayer with sighted friends, as this requires everyone to install identical mod lists. There
may also be other reasons to use this mode such as sharing your save file and so on.

NOTE: Maintaining this mode is NOT a recommendation that you should try multiplayer, and nothing here makes multiplayer
better for mod players. Your experience will continue to lag a lot and parts of the game such as combat will continue to
be essentially unplayable in multiplayer, with vanilla mode or otherwise. We maintain this feature for the small number
of users who want to play multiplayer anyway or have different use cases.

When Vanilla Mode mode is toggled on, it disables sonifiers and speech, stops all of the mod's key handling, and closes
any open mod UIs. Mod data such as fast travel points are left untouched, and so you should be able to toggle Vanilla
Mode without any penalties. If the sighted person does not run our keys script, then their game in
Vanilla Mode should function as if this mod is not present. If the mod's config tweaks are used and then Vanilla Mode is
enabled, the keymapping changes of the mod are still present but inactive. In this case one can play using the arrow
keys and mouse while relying on GUI buttons to open most menus.

Vanilla Mode is toggled on or off by pressing CONTROL + ALT + SHIFT + V.

NOTE: If the ESCAPE key does not work for pausing the game, please try the alternative of SHIFT + ESCAPE.

NOTE: Maintaining a bug-free Vanilla Mode relies on active sighted users and/or developers. By the nature of it, blind
developers cannot test it at all, and whether or not this project is primarily maintained by the blind varies day to
day.

# Help and Support

If your question wasn't answered here or on our wiki, feel free to contact us at our [Discord server](https://discord.gg/CC4QA6KtzP).

# Changes

An updated changelog can be found [here](https://github.com/Factorio-Access/FactorioAccess/blob/main/CHANGES.md).
