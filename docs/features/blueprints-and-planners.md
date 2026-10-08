# Blueprint and Planner support

## Keys

These use all the normal keys, with only a few changes:

Start selecting a blueprint, upgrade, or deconstruction planner's area: `left bracket` with a 1x1 cursor

Finish selecting: `left bracket` on a second point. Releasing the key does nothing; the selection stays open until the second press, as on a gamepad.

While the selection is open, each move of the cursor says the box's size in tiles, such as "5 by 3", and then the counts the game shows beside the box, largest first: the items a copy or blueprint would take, what an upgrade planner would upgrade and to what, or what a deconstruction planner would remove and the items that would give.

When a deconstruction or upgrade selection finishes, you hear how many entities it changed, such as "12 marked for deconstruction" or "3 upgrade cancelled". Building a blueprint over entities that it marks is said the same way.

Cancel the selection without doing anything: `escape`

Cancel upgrade or deconstruction orders in an area: start the selection with `shift + left bracket`. The first press decides what the selection does; the second only finishes it.

Open a blueprint's or planner's window: `right bracket` on it in an inventory slot

Cycle through blueprints in a blueprint book without opening the GUI: `m` and `dot (.)`

Announce the currently active blueprint in a book: `comma (,)`

Get a blueprint: `alt + b`

Get an upgrade planner: `alt + u`

Get a deconstruction planner: `alt + d`

Get a new blueprint book: `ctrl + alt + shift + b`

Copy: `ctrl + c`

Cut: `ctrl + x`

Paste: `ctrl + v`

Destroy a planner item with the GUI closed (important! With the gui closed): `ctrl + backspace` with the item in hand

## Description

Blueprints and blueprint books in your inventory belong to the save. To keep one for your other games, put it in the blueprint library, opened with `b`; see the Blueprint Library section below.

### Blueprints and Copy/Paste

Blueprints are a mechanism to save a group of entities and put it somewhere else later.  There are a few ways to get one:

- `alt + b` gives you a blank blueprint. Click twice to select the corners of a box.
- `ctrl + c` gives you the copy tool. This is like a blank blueprint, but the resulting blueprint is temporary and goes away when you empty your hand.
- `ctrl + x` gives you the cut tool, which is exactly like the copy tool but marks the area selected for deconstruction as well. This is useful to move a bunch of entities by a few tiles, for example.
- `ctrl + v` gives you a temporary blueprint, whatever you copied or cut last.

To stop selecting if you change your mind, press `escape`.

When a blank blueprint's selection finishes, the game opens the blueprint's setup window, as it does for sighted players. The same window opens with `right bracket` on a blueprint in an inventory slot. Its stops, moved between with `tab`:

- the blueprint: its name, then buttons to select new contents, copy it, upgrade its entities, parametrise it, export it to a string, and delete it. `enter` on the name starts renaming it: type the name and press `enter`;
- its four icons in a row, where `enter` opens the chooser and `right bracket` clears one; under them its description, and a button that puts an icon in the description's text;
- snap to grid: the checkbox, then the grid size, the grid position, and the absolute and relative snapping, one line at a time. The fields say disabled until the box is checked, as the game greys them;
- the components the blueprint holds, ten to a row. `right bracket` takes every entity of that kind out of the blueprint, and `enter` puts them back; a kind taken out says removed;
- what to include, such as modules, tiles, trains or fuel, when the blueprint has any of them. For example, you can blueprint only tiles, even if entities are over them;
- the preview: the game's hint, then the picture itself, which you read a tile at a time;
- the button that creates or saves the blueprint.

In the picture, the arrows or `WASD` move a cursor one tile, and `shift` with them skips to the next tile that reads differently, as the map cursor skips. `home` and `end` go to the ends of the row, and `k` (or wherever you bound read coordinates) says the tile's place, counted from the top left corner. Past the top of the picture you are back at the hint.

Each tile reads what the game draws there: the entity, its direction, which way an underground belt or loader faces, a combinator's operation, and the items to be delivered to it ("with 2 speed module"). A tile with no entity reads its floor tile, or empty. While alt mode is on you also hear what alt mode shows on the entity, as sighted players see it in the picture: its quality, an assembler's recipe (with locked when your force has not unlocked it yet, where the game crosses the recipe out), inserter, loader, splitter, pump and asteroid collector filters, splitter priorities, and with the interface setting for combinator settings, a combinator's signals.

`right bracket` takes the entity or tile out of the blueprint and it reads removed; `enter` puts it back. `shift + enter` makes the tile the grid's position and reads the grid position fields.

To build a blueprint, click with `left bracket`.  Rotation, flipping, etc. all work the same as buildings, and you are building from the top left, same as buildings.

A blueprint in your inventory takes a slot. Books group blueprints into one slot, and the blueprint library holds them without taking any; see the next sections.

### Blueprint Books

A blueprint book is a bunch of blueprints, planners and other books grouped together.  It takes one inventory slot, no matter how many blueprints are in it.

A blueprint, book or planner in a slot reads as sighted players see it: its name if it has one, what it is, then the up to four icons its owner chose, such as "Smelting, Blueprint, Stone furnace, Iron plate". A book with no icons of its own shows those of its active blueprint, and a planner with no icons its first filters. The slot of whatever you hold reads in hand.

In terms of building, blueprint books work almost identically to blueprints.  The difference is that a blueprint book has an active blueprint, the one you build.  Use `m` and `dot` to cycle to the one you want without opening the book, or open the book.

To open a book, press `right bracket` on it in an inventory slot. Its window has these stops, moved between with `tab`:

- the book: its name and the button to edit its name, description and icons, its description, then where the book is, such as "Inventory: name". When the book is inside another book, each book above it is a button that goes up to it;
- the book's buttons: copy it, upgrade every blueprint in it (press it holding an upgrade planner), export it to a string, and destroy it;
- the contents, laid out as the game lays them out in the view you chose. Each slot reads like an inventory slot, and the active one says active. `enter` takes or puts a slot's item, as a click does, and `right bracket` opens it: a blueprint's setup window, or a book inside the book;
- the view: the game's hint on cycling, and the List, Grid and Slots buttons. In List view each item is a row and reads its description too;
- your inventory. Take a blueprint here with `enter` and put it into an empty slot of the contents to add it to the book.

### Blueprint Library

The blueprint library keeps blueprints, books and planners outside your inventory. Open it with `b`, or with its button on the shortcut bar. It has two tabs: My blueprints, which you keep in every game, and the blueprints of this game, shared by everyone playing it. Its window has these stops, moved between with `tab`:

- the tabs, My blueprints and the game's blueprints. `enter` switches;
- the records of the chosen tab, laid out as the game lays them out in the view you chose. Each reads like an inventory slot: its name, what it is and its icons, then not available yet while it is still arriving, how far it has arrived, and in hand on the one you hold. The empty slots after them take whatever you drop there;
- the view: the game's warning when the library is not synchronised, and the List, Grid and Slots buttons;
- the window's history: the back and forward buttons, and the game's warning when the library takes a lot of memory;
- your inventory.

On a record, `enter` takes it into your hand, and while you hold a blueprint, `enter` on a slot puts it there. `right bracket` opens it: a blueprint's setup window, a planner's window, or a book inside the library. `shift + enter` moves it into your inventory.

To keep a blueprint for other games, take it from your inventory with `enter` and put it into an empty slot of My blueprints.

A book opened in the library replaces the tabs and has the stops of a book's window: the book (its name, the button to edit it, its description, then the way back up to the shelf and to each book it is inside), its buttons (copy, upgrade, export, delete), its contents with the active one marked, and the view.

### Planners

You can mark everything in an area for deconstruction or upgrade.  To do so, get the planner, then click the corners of the box.

To configure a deconstruction planner, put it in a slot of your inventory and press `right bracket` on it, as a sighted player right-clicks it. Its window has four stops, moved between with `tab`:

- the planner: its name, then buttons to edit its name, description and icons, copy it, export it to a string, and delete it;
- the settings: trees and rocks only, the Entities and Tiles tabs, whitelist or blacklist (`enter` flips it), and on the Tiles tab the tile mode;
- the chosen tab's filters, ten to a row: `enter` opens the chooser, `right bracket` clears a slot;
- your inventory.

You can "undo" or "cancel" by getting the same kind of planner in the hand and reselecting the same box, starting it with `shift + left bracket` instead of `left bracket`.

An upgrade planner is configured the same way, with `right bracket` on it in a slot. Its window has three stops:

- the planner: its name and the same buttons as the deconstruction planner's;
- the rules, one row per rule: its From slot, then its To slot. `up` and `down` say the rule's number with the slot, such as "2, From, wooden chest"; `left` and `right` say only the other side. `enter` opens the chooser, `right bracket` clears a slot;
- your inventory.

You can pull planners into blueprint books and use them from the book as if they were directly in your hand.
