# Windows

FactorioAccess has no menus of its own. It reads the game's own windows, the same ones sighted players use, and moves through them from the keyboard. Using a window through FactorioAccess clicks it exactly as the mouse would, so what you do there is what the game does.

## Keys

| Key | What it does |
| --- | --- |
| arrows, or `w`, `a`, `s`, `d` | Move |
| `ctrl + up`, `ctrl + down` | Jump to the previous or next region, such as a heading or a group |
| `shift + w`, `a`, `s`, `d` | Skip across a picture or a map, as on the map |
| `tab`, `shift + tab` | Next or previous part of the window |
| `home`, `end` | First or last item |
| `enter` or `[` | Click; with `shift` or `ctrl` held, shift click or control click |
| `]` or `backspace` | Right click, or open and close a submenu |
| `\` | Middle click |
| `y` | Read the tooltip |
| `k` | Read where you are, such as a slot's row and column |
| `ctrl + tab`, `ctrl + shift + tab` | Move to the next or previous part of the screen |
| `escape` | Close the window |

## Opening windows

- `[` on a building opens its window.
- `e` opens your character screen: your inventory, crafting, logistics and equipment.
- `]` on an item in an inventory slot opens the item, such as a blueprint, a planner or armor.
- `escape` opens the game menu, with Settings, Controls and Mod settings.
- The game's other keys open their windows: `ctrl + t` research, `p` production statistics, `l` logistic networks, `o` the trains overview, `b` the blueprint library, `tab` or `m` the map. See [the key reference](keys.md).
- `alt + [` opens Factoriopedia on what the cursor is on. In a window it opens on the item, recipe or slot you are on, as the mouse would.

## Moving through a window

A window is read as a few parts, such as the building's inventory, its recipe and your own inventory. `tab` moves to the next part and says its name. Inside a part, the arrows move between slots, buttons and fields, and FactorioAccess says each one as you land on it.

`ctrl + tab` moves beyond the window to the other parts of the screen: the quickbar, the shortcut bar, the side menu, the pins, the crafting queue, the HUD's research, alerts and goal. `ctrl + tab` with no window open does the same.

Where the game shows a tooltip only on hovering, `y` reads it.

## Using controls

`enter` (or `[`) clicks, as the left mouse button would; `]` right clicks. So an inventory works just as it does with the mouse:

- `[` on a slot picks its items up into your hand, or puts the hand's items down.
- `]` on a slot picks up half of it.
- `shift + [` moves the whole stack to the other inventory, such as from a chest to your character.
- `ctrl + [` moves all items of that kind.

In crafting, `[` crafts one, `]` crafts five, and `shift + [` crafts all you can.

`q` pipettes what you are on, as it does with the mouse over it: the items in a slot, or a slot's filter, a recipe's item, a filter button's item or signal, a Factoriopedia entry. With something in hand, `q` empties the hand first.

Some controls take a value:

- A text field takes your typing; `enter` confirms it.
- A drop-down opens on `enter`. Pick with the arrows and `enter`.
- A slider in a list is adjusted after `enter`: the arrows then change its value, and `enter` or `escape` stops.
- A control with a submenu opens it with `]` and closes it with `]` again.

## Alerts configuration

The side menu's Alerts button opens the game's alerts configuration: a line per kind of alert, its name and when it last happened, such as "Train out of fuel, 5 min ago" or "never". Right of the name are its Muted and Disabled checkboxes; `up` and `down` stay in a column and name each alert, and `enter` ticks one. Only the alerts that play a sound can be muted; the others' Muted checkbox says "no sound". `tab` reaches the sort buttons, which sort the list by a column and say its order.

## Pins

The game's pins panel lists the places, buildings, resource patches, tags, players and alerts you pinned. Pin them with the pin button beside a map search result or a group in the alerts window. While you have a pin, `ctrl + tab` reaches the panel, a line per pin named as the game names it: its label, or else what it points at, such as "Iron ore 273k" or "{10, -5}" for a place.

- `enter` on a pin opens the map on it. `shift + enter` sends the train you ride there, and `]` your selected spidertrons.
- Right of the name, edit opens the pin's settings: its label, whether it shows on the map from afar, and the size of its preview. Type the label and press `enter` to confirm.
- Remove deletes the pin.
- `ctrl + shift + up` and `ctrl + shift + down` move a pin up or down the list.

The scanner lists your pins too, under its pins category.

For a pin set to show on the map from afar, the game draws an arrow at the edge of the screen pointing to it while it is off screen. FactorioAccess plays a sound for those arrows while the scanner is in its pins category: every 2 seconds while the cursor or your character moves, each such pin sounds from its direction, muffled when it is to the south and quieter the farther it is. "Pin arrow sonification" in the game's Mod settings turns these off. The scanner plays the same sound when it reaches such a pin.

## Logistic requests

Whatever logistic robots fill or empty has a logistics part in its window: your character screen once robots are researched, requester, buffer and storage chests, roboports, vehicles, and in Space Age the platform hub and the cargo landing pad. Constant combinators set their signals the same way.

- It starts with its checkboxes, such as Personal logistics or Trash unrequested.
- Then come its sections, each a header and its slots. `ctrl + up` and `ctrl + down` jump between sections. The header's checkbox, named after the section's logistic group ("[No group assigned]" for none), turns the section on or off. Change logistic group picks or makes a group, and Remove deletes the section.
- A slot says its request. "Iron plate 200 to 400" has robots keep 200 and take away what is over 400; "Iron plate 200" has no maximum. A combinator's slot says its signal and count.
- `enter` on a slot opens the game's Set request window. Pick the item; `tab` then reaches the quality, the minimum and maximum fields and the confirm button.
- On a slot that holds an item, type a number and press `enter` to set its minimum, or a combinator signal's count. `backspace` takes back a digit and `escape` cancels. A minimum over the maximum raises the maximum with it.
- `ctrl + shift + up` and `ctrl + shift + down` on a section's header move the section up or down.
- Add section, last, adds an empty section.

A storage chest has one filter slot instead: the item robots bring there.

## Logistic networks

`l` opens the game's Logistic networks window beside the map, in remote view. It lists the networks on the surface you are viewing; choosing one moves the map to it. Then come the network's name with its Edit button, the search field, its Members (roboports, chests, robots, with counts) and its Items (everything stored in it, with counts).

`enter` on an item selects it and the game marks on the map every chest holding it; on a member, every one of that type, robots where they fly. A selected slot says "selected", and `enter` again clears it ("not selected").

`ctrl + tab` goes to the map beside the window, and `ctrl + shift + tab` comes back. There the cursor moves as in remote view and says "marked" on what the window marks, and the scanner's Marked category lists those places, nearest first, each type a subcategory: `page down` and `shift + page down` move through them, and `home` moves the cursor onto one, where `[` opens it as usual.

## FactorioAccess's views beside a window

Where a window lacks something worth saying, FactorioAccess adds a view beside it, as an ordinary part of the window that sighted players see too: what a belt carries (see [basics](basics.md)), where a pipe connects, what a pole is wired to. `tab` reaches it after the window's own parts.

## Settings

FactorioAccess's settings are in the game's Mod settings window: `escape`, Settings, Mod settings. Controls, including FactorioAccess's own keys, are in the Controls window beside it.
