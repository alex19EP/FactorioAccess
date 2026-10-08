# Movement, the Cursor, and building

## Quick Key Reference:

Walk your character: `arrow keys`

Move the cursor: `wasd`

Pick items up from near your character: `hold f`

Toggle whether the cursor stays in front of you when walking: `i`

Return the cursor to your character: `j`

Enter cursor coordinates and jump to them: `shift + c`

Open fast travel: `alt + v`

Place the audio ruler: `ctrl + alt + b`

Clear the audio ruler: `alt + shift + b`

Increase the size of the cursor: `shift + i`

Decrease the size of the cursor: `ctrl + i`

Announce cursor coordinates and other info depending on context: `k`

Announce a detailed description of the item in your hand: `y`

Announce cursor's relative position to the character: `alt + k`

Build, configure, interact, and perform other "primary" actions: `left bracket`

Force buildt: `shift + [`

Superforce build: `control + shift + left bracket`

Mine: `hold x`

Instantly mine everything mineable within 10 tiles: `shift + x` (a cheat)

Clear all ghosts within 100 tiles: `ctrl + shift + x` (not a cheat--only removes ghost entities)

Get the status of an entity: `right bracket`

Check the pollution level at the cursor: `shift + u`

Fast transfer full stacks from the hand to the selected entity: `ctrl + left bracket`

Fast transfer half stacks from the hand to the selected entity: `ctrl + right bracket`

Drop 1 item from the hand into an entity: `z`

Open the offshore pump assist tool or set splitter filters to item in hand: `alt + left bracket`

Skip the cursor up to 100 tiles or until the first change: `shift + wasd`

Move the cursor by the size of the item in hand: `ctrl + wasd`

Move larger cursors by 1 tile: `ctrl + WASD`

Teleport to the cursor: `shift + t`

Teleport to the cursor when enemies are present at the destination: `ctrl + shift + t`

Trigger Kruise Kontrol: `ctrl + alt + right bracket`

Cancel Kruise Kontrol: `enter`

Rotate item in hand or selected entity clockwise: `r`

Rotate item in hand or selected entity counterclockwise: `shift + r`

Flip horizontal: `h`

Flip vertical: `v`

Empty the hand: `q`

Announce the contents of the hand: `shift + q`

Pippette: `q` with an empty hand

Cycle through all entities on a tile, announcing them: `shift + f`

Do the appropriate action for the item in hand, for example equipping equipment: `ctrl + shift + left bracket`

Build a line: hold `left bracket` and move the cursor

Turn a belt line while holding `left bracket`: `r`, then step to the side

Cycle your cursor through vehicles near your cursor: `shift + v`

Open the tutorial: `control + t`

## Description

The mod replaces the mouse with keyboard commands.  We call this the cursor.  This is the mod equivalent of the sighted cursor.

The Factorio world is continuous, but all buildings in Vanilla build on a tile grid.  The mod takes the continuous world and turns it into a tile grid for you.  You navigate this grid with WASD.

Walking always remains "smoothe", e.g. your character can be at `x = 1.2323`.  Most mod users do not walk much.

If you want to directly center your character on a tile, `shift + t` can be used to teleport. This works for long range teleports (a cheat) as well as short range "move me a couple tiles and line me up perfectly" teleports.

For those who wish to avoid teleporting long distances, place your cursor on an empty tile and press `ctrl + alt + right bracket` to trigger Kruise Kontrol.

The keys for the cursor are not logically arranged because of ergonomics.  By putting things on different hands, most operations are a "drum roll". For example, `d` `left bracket` over and over can build a line of stuff, and because it's both hands you can press it much, much faster.

You can think of brackets like the mouse buttons. They roughly match vanilla, and you will often see left bracket referred to as clicking on Discord and when discussing with other players.

### The Character

Your character is your body.  You always have one, unless you are dead or in other advanced situations that do not come up much in the base game.

You mostly do not need to be concerned with it, especially if you teleport, but it must be in range of building unless you place ghosts (10 tiles by default)

Your character is also what you "pick things up from". For example, putting your character on a belt and holding `f` picks up from that belt, approximately.  This is not precisely announced due to API limitations.

To remember this, your character is the thing with arms.  If you need your arms, then the position of the character matters.

For convenience, Factorio does not require you be close for everything, thus the larger building radius and the ability to place ghosts anywhere.

Unfortunately, however, API limitations mean that we cannot precisely announce when the problem is that you are out of range.  If you get an error bonk and can't figure out what else it could be, try moving closer.

The following sections elaborate on mod features.  Not all interactions are covered because many are very simple.  See the key list above for the full list.

### The Cursor and Hand

The cursor is where you are looking and operating.  As the cursor moves around, the entity you are operating on changes with it.  This is called selecting an entity.

The most basic use of the cursor is mining.  To do so, get your character within a few tiles of an entity like a resource and hold `x`.  This is also how you remove placed buildings.
Note that some entities take a while to mine.  An example of such is the rocket pieces in the initial crash site, all of which may be removed.

What the game calls the hand is not your character's hand, but your virtual hand.  Your hand's position is always at the cursor.

### Alt mode and status icons

The cursor reads the icons the game draws on an entity, as a sighted player sees them.  Status icons such as no power, no fuel or no ammo are read whenever the game shows them.  The rest are alt mode icons: a machine's recipe and modules, a chest's contents, filters, the fluid in a pipe, a turret's ammo, a combinator's signals.  These are read only while alt mode is on, which the game's own key toggles (`alt` by default).  An icon shown more than once is read once, an item with its count, and a quality badge is read with its icon.  Toggling alt mode says so.  These are the game's own icons, so modded entities read the same way.

### Building

To build, you click with `left bracket` with something buildable in hand.  This will place the entity.  You are holding the top left corner.  It is important to note that the sighted build from the center.  This isn't keyboard friendly, so we changed it, but it sometimes matters when reading things or discussing with the sighted.

See the page on blueprints and blueprint books for more info on those special cases.

To place a ghost, add `shift`.  Ghosts are entities that mark where something will be built later, and can be placed at any range.  This is useful in the early game because Kruise Kontrol can build them for you; `ctrl + alt + right bracket` on a ghost starts building all ghosts.  In the late game, this is how you interact with construction robots.

### What the preview shows

With something buildable in hand, each cursor move reads what the game's preview shows a sighted player there: why it cannot be built, then what it highlights, each with its distance and direction.

- `power from` the poles that would power it, or `no power`.
- `wire to` the poles a pole would connect to, or `no wires`.
- `covers` what a pole's supply area would reach.
- `works with` the inserters, drills and machines that would put into or take from it.
- `pairs with` its underground partner.
- `replaces`, `turns` or `changes` what building it would affect.
- `links to` the roboports a roboport would connect to, and `in network` or `no network` for a logistic chest.

These are the game's own highlights, so modded entities read the same way.  During a drag only what blocks the build is read.

For a few items, the mod adds special behavior:

- For underground belts, the underground belt will become an exit if it was going to match an entrance.
- For the offshore pump, which has weird placement rules, you can get a menu of proposed locations with `alt + left bracket`.

### Drag Building

Hold `left bracket` and move the cursor to place a line of stuff.  This is the game's own drag building, the same as a sighted player holding the mouse button and dragging, so it follows vanilla's rules:

- The line stays straight, along the way you first moved.  Steps to the side build nothing.
- To turn a belt line, press `r` while still holding `left bracket`, then step the cursor the way the belt should go.  `r` says "turn at next step", and the step says the corner and its new direction.
- Electric poles are placed the maximum distance from the last pole.

While `left bracket` is held, cursor steps are silent unless something blocks the build; the game's build sound marks each placed entity.

To build a line while walking, press `i` so the cursor stays one tile in front of your character, then hold `left bracket` and run:

- Move the cursor to where you want it to start
- Press `shift + t` to teleport
- Put a belt in hand, press `i`, and hold `left bracket`
- Run your character in a straight line to where you want the belts to stop, then let go

### The Ruler and fast travel

The ruler is for alignment.  You place it with `ctrl + alt + b` and it forms an audio cross.  When your cursor or character crosses it, it plays a tone.  Clear it with `alt + shift + b`.

You can use rulers to quickly place large numbers of items.  For example, to place a belt going east, find where you want it to end, arrow a few tiles north, and place your ruler.  Then, you can hold `left bracket` and run in a straight line until your character hits the ruler, like hitting a wall.  IMPORTANT: the line does not stop itself at the ruler, these are still separate features.

To remember map locations, use fast travel, accessible with `alt + v`.  Fast travel is like your browser's bookmarks menu, but for map locations.


### Cursor Skipping

For larger distances, the mod offers cursor skipping.  Holding `shift` plus the cursor movement keys will move up to 100 tiles in any given direction, stopping at the first "change".  Change is somewhat ill-defined.  Examples which count include water to ground, belt corners, and rows of entities beginning or ending.

Many entities are larger than 1x1 and moving 3 tiles to place the next one is annoying.  For this reason we also offer skip by preview.  `ctrl + WASD` moves the cursor so that, if you built the item, it would form a perfect row or column.  For example an assembling machine is 3x3, so pressing `ctrl + d` would move you 3 tiles east. This also works on blueprints.

### Larger Cursors and Placing Tiles

Exploring the map one tile at a time is annoying, so the mod offers larger cursors.  Larger cursors are boxes, for example 3x3, 5x5, etc.  They perform two functions:

- As the larger cursor moves, a summary of everything in the box is announced.
- If building tiles such as landfill, clicking fills the entire cursor box with the tile (not a cheat--vanilla offers a similar function)

To change the size of your cursor, use `shift + i` to increase it and `ctrl + i` to decrease it.

In larger cursor mode, your cursor is at the center of the box and the box is always odd in dimension. So for example 3x3 is 1 tile in every direction from your cursor.

Two control changes happen when your cursor is larger than 1x1:

- `WASD` now moves by the size of the cursor instead of by 1 tile
- `ctrl + WASD` moves by 1 tile instead of performing skip by preview


### The Pipette tool

This is a vanilla function but deserves brief coverage.  Pressing `q` with an empty hand on a tile or entity puts "the most appropriate thing" in your hand.  For entities, this is the same entity if you have some in your inventory.  For tiles it depends: water, for example, is an offshore pump, and resources are drills.

It's not complicated at all, but it's important to know it exists


### Automation with Kruise Kontrol

Kruise Kontrol is a mod that we forked which provides some automation functions.  It is useful because it can make up for not being able to use the mouse.

Place your cursor on a tile or item and press `ctrl + alt + right bracket` to trigger it.  Pressing `enter` cancels.

What happens depends on what the cursor is on:

- If an empty tile, you walk to it
- If a resource, rock, or tree, you mine it
- If a ghost, you build it, crafting as needed
- If an item marked for upgrade or deconstruction, you upgrade or deconstruct
- If on something that takes fuel, you refuel it
- If on an enemy, you fight it.

Kruise Kontrol supports vehicles, and can thus be used to easily drive places.

It will continue the current action until there is nothing else to act on close by.  Triggering on a ghost for example will build all ghosts until it can't find any ghosts or can't find the items to build it.


### Belts

Belts are explained in the tutorial and mostly match vanilla but there are some specific notes that should be coverred here.

The mod provides the belt analyzer.  This is a 4-tab interface:

- This belt is a 2x4 grid, showing the current belt discretized into 8 slots
- Upstream is a summary of all belts behind this one ("toward the source of the river")
- Downstream is a summary of all belts ahead of this one
- Total is both upstream and downstream, plus this belt

The reason it deserves documentation is because of some edge cases which you may need to be aware of.

First, note that it stops where the circuit network would: at a splitter or a pouring end.  It also doesn't traverse loops more than once, and may give somewhat nonsensical results in that situation (which part of the loop is upstream?)

There are two odd behaviors you may observe, both happening for roughly the same reason: percents over 100% and seeing two items on the same belt slot.

This can happen for two reasons.  The first is that you are playing space age with stacked belts.  In that case the percentage is a measure of how "stacked" the belt is.

The second is a bit more subtle.  Belts have 8 slots for items in the steady state.  But when inserters or drills place items on them, they form corners, or they feed a splitter this stops being true by a small amount.  This happens because the game allows things to temporarily crunch together by a little bit to keep things running.  The specific rules for when this occurs are not straightforward, but in general the easiest way to see percents over 100% is to turn a belt segment to the side so that it becomes a corner, then turn it straight again after it fills up.

This said, if you are playing in 2.0 base or space age without stack inserters and see numbers significantly over 100%, you may have found a bug; please report it to us.
