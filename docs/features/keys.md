# Keys

FactorioAccess plays with the game's own controls wherever the game has one. The mod adds keys only for what a sighted player gets by looking: the cursor, reading, the scanner, and a few aids. Everything else is the game's own key, and you can rebind it in the game's Controls window (Escape, Settings, Controls).

## What the keys script changes

`factorio-access-keys.cmd` (see the README) moves these game controls:

- Walking: from W, A, S and D to the arrow keys. W, A, S and D move the cursor instead.
- Connect and disconnect train: from J and K to `ctrl + j` and `ctrl + k`.
- Research (the technology window): from T to `ctrl + t`.
- Zoom in and zoom out: `=` and `-`, beside the mouse wheel.
- Every control on a mouse button also gets a key, with the same modifiers: the left button is `left bracket` (`[`), the right button `right bracket` (`]`), the middle button `backslash` (`\`). This includes other mods' controls, such as Kruise Kontrol's.

So wherever the game says "left click", press `[`, and wherever it says "right click", press `]`.

## The game's keys you will use most

These are the game's defaults, after the keys script:

| Key | What it does |
| --- | --- |
| `[` | Build what is in hand; open the selected entity's window |
| `shift + [` | Build a ghost; paste entity settings |
| `ctrl + shift + [` | Super forced build |
| `]` | Mine (hold it); open an item; use a capsule or remote |
| `shift + ]` | Copy entity settings |
| `e` | Your inventory (the character screen) |
| `escape` | Close a window; the game menu |
| `q` | Empty the hand; with an empty hand, pipette what the cursor is on |
| `r`, `shift + r` | Rotate the hand or the selected entity |
| `h`, `v` | Flip the hand horizontally or vertically |
| `f` | Pick up items near your character (hold it) |
| `z` | Drop one item from the hand |
| `enter` | Get in or out of a vehicle; cancel Kruise Kontrol |
| `space` | Shoot |
| `c` | Next weapon |
| `tab` or `m` | The map |
| `b` | The blueprint library |
| `o` | The trains overview |
| `p` | Production statistics |
| `l` | Logistic networks |
| `ctrl + t` | Research |
| `alt` | Alt mode, which shows more icons on entities |
| `ctrl + c`, `ctrl + x`, `ctrl + v` | Copy, cut, paste |
| `ctrl + z`, `ctrl + y` | Undo, redo |
| `alt + b`, `alt + d`, `alt + u` | A new blueprint, deconstruction planner or upgrade planner |
| `alt + r`, `alt + g`, `alt + c` | A red, green or copper wire |
| `alt + l` | Turn personal logistic requests on or off; FA says the new state |
| `alt + f` | Turn the personal roboport on or off; FA says the new state |
| `1` to `0` | Quickbar slots |

## FactorioAccess's keys

### The cursor

| Key | What it does |
| --- | --- |
| `w`, `a`, `s`, `d` | Move the cursor one tile |
| `shift + w`, `a`, `s`, `d` | Skip up to 100 tiles, stopping at the first change |
| `ctrl + w`, `a`, `s`, `d` | Move by the size of the building in hand; with a detached cursor, move the pinned build one tile |
| `shift + f` | Cycle through everything on the cursor's tile |
| `i` | With a building or blueprint in hand, detach the cursor from it, or attach it again; otherwise, anchor the cursor in front of your character as you walk, or free it |
| `j` | Move the cursor back to your character |
| `k` | Read the cursor's coordinates; while you drive, the vehicle's heading and speed |
| `shift + k` | Read the cursor's distance and direction from your character |
| `alt + k` | Read the cursor's distance east or west and north or south of your character |
| `alt + j` | Read your character's coordinates |

Hold `w`, `a`, `s`, `d`, alone or with `ctrl`, to keep moving at your system's key repeat rate. The skips and the cycle move once a press.

While you drag a building along with `[` held, each step says only what changes under the cursor: the ground when it differs from the tile before, a building that was already there as the cursor enters it, and why the building can't go there.

### Reading

| Key | What it does |
| --- | --- |
| `y` | Read the game's full information panel for what the cursor is on |
| `shift + y` | Read a description of the item in hand |
| `shift + q` | Read what is in your hand |
| `t` | Read the time of day |

### The scanner

See [the scanner](scanner.md).

| Key | What it does |
| --- | --- |
| `end` | Refresh the scan |
| `shift + end` | Refresh the scan, in the direction your character faces |
| `ctrl + page up`, `ctrl + page down` | Previous or next category |
| `page up`, `page down` | Previous or next subcategory |
| `shift + page up`, `shift + page down` | Previous or next entry in the subcategory |
| `home` | Move the cursor onto the entry |

### Doing

| Key | What it does |
| --- | --- |
| `ctrl + alt + b` | Take the audio ruler tool |
| `ctrl + tab`, `ctrl + shift + tab` | Move between the parts of the screen: an open window, the quickbar, the shortcut bar, the HUD |

### Combat

See [combat](combat.md).

| Key | What it does |
| --- | --- |
| `ctrl + shift + i` | Enter or leave combat mode |
| `shift + w`, `a`, `s`, `d` | In combat mode, throw the capsule in hand in that direction |
| `ctrl + shift + w`, `a`, `s`, `d` | In combat mode, throw it even in safe mode |
| `ctrl + r` | Aim at the strongest or the closest enemy first |
| `shift + r` | In combat mode, aim at spawners first or not |
| `ctrl + shift + r` | Safe mode on or off |

### Settings

| Key | What it does |
| --- | --- |
| `ctrl + alt + shift + i` | Inserter sounds on or off |
| `ctrl + alt + shift + c` | Crafting sounds on or off |
| `ctrl + alt + shift + e` | Enemy sounds on or off |
| `ctrl + alt + shift + s` | Spawner sounds on or off |
| `ctrl + alt + shift + v` | Vanilla mode on or off (see the README) |

These sound settings are also in the game's Mod settings window.

## In the game's windows

FactorioAccess reads every game window and moves through it from the keyboard. See [windows](ui.md).

| Key | What it does |
| --- | --- |
| arrows, or `w`, `a`, `s`, `d` | Move |
| `ctrl + up`, `ctrl + down` | Jump to the previous or next region, such as a heading |
| `shift + w`, `a`, `s`, `d` | Skip across a picture or a map, as on the map |
| `tab`, `shift + tab` | Next or previous part of the window |
| `home`, `end` | First or last item |
| `enter` or `[` | Click; `shift` and `ctrl` click with those keys held |
| `]` or `backspace` | Right click, or open and close a submenu |
| `\` | Middle click |
| `y` | Read the tooltip |
| `k` | Read where you are, such as a slot's row and column |
| `ctrl + tab`, `ctrl + shift + tab` | Move to the next or previous part of the screen |
| `escape` | Close the window, or stop adjusting a slider |

### In a train's window

| Key | What it does |
| --- | --- |
| `ctrl + up`, `ctrl + down` | Previous or next station of the schedule |
| `ctrl + shift + up`, `ctrl + shift + down` | Move a station, wait condition or interrupt up or down, as dragging it does |
| `ctrl + tab` | Go to the map beside the window; `ctrl + shift + tab` comes back |
| `ctrl + [` on the map | Add a temporary stop near the cursor, in map view |
| `shift + [` on the map | Add the train stop under the cursor to the schedule |

On the map, each cursor move also says what these two would add: a temporary stop on the nearest rail and the nearest train stop, each within 20 tiles, with "no path to" when the train cannot reach it.

See [rails](rails.md).

## Kruise Kontrol

With the [Kruise Kontrol](https://mods.factorio.com/mod/Kruise_Kontrol) mod installed, `ctrl + alt + ]` sends your character to what the cursor is on, and `ctrl + alt + shift + ]` queues it after the current job. `enter` cancels. FactorioAccess says what Kruise Kontrol reports.
