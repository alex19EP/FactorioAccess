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

WARNING WARNING WARNING: blueprints and blueprint books are per save. We have no ability to access the blueprint library, which is the sighted version that is not per save.  To keep your blueprints, you need to export them and reimport them into future saves!  If you delete your save or otherwise lose it, they are gone forever.

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
- the preview;
- the button that creates or saves the blueprint.

To build a blueprint, click with `left bracket`.  Rotation, flipping, etc. all work the same as buildings, and you are building from the top left, same as buildings.

Due to API limitations, blueprints always take an inventory slot.  We cannot get access to the blueprint library, which is how sighted players work around this.  See the next section.

### Blueprint Books

A blueprint book is a bunch of blueprints grouped together.  Like blueprints API limitations means that it takes an inventory slot, but only one no matter how many blueprints are in it.

In terms of building, blueprint books work almost identically to blueprints.  The difference is that a blueprint book has a concept of active blueprint.  There are two ways to change the active blueprint:

- Hit `right bracket` and select the one you want, or
- Use `m` and `dot` to cycle to the one you want without opening a GUI

Blueprint books also have blueprint book level config. To get to it press `tab` as with other UIs.

To get a blueprint into a book, use the labeled row in the blueprint book's config tab to pull them from your inventory.

We do not support all blueprint books.  We only support those consisting of blueprints.  Some sighted blueprint books
contain blueprints in folders, and for a variety of reasons we cannot read those accurately, nor do we guarantee we
won't crash on it.  This is an unfortunate limitation of the API which is too big of a problem to work around given the
effort it would take.

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
