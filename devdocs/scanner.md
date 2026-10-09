Last reviewed: 2026-10-09 (update this if reviewing this doc)

# Introduction

The scanner turns the map into a list the player moves through with `PAGE UP` and `PAGE DOWN` plus
modifiers: categories (`CONTROL`), subcategories (no modifier) and entries (`SHIFT`). `HOME` says
the current entry again and `END` lists the surface again (`SHIFT + END` only in the direction the
character faces). Examples: an assembling machine is category Production, subcategory by its recipe;
an iron ore patch is category Resources, subcategory iron ore.

The list lives in the native DLL (`native/src/scanner.h`, `scanner.cpp`), on the client of the
player who scans. Nothing of it is in `storage`, and other clients have none. The Lua side is
`scripts/scanner/`.

# What the DLL lists

A refresh (`fa_native.scanner_refresh`, from the `END` handler, only where `fa_native` exists) walks
every chunk of the surface within `SCANNER_DISTANCE` that the player's force has charted:

- Entities by type (`kTypes` in `scanner.cpp`), each its own entry, grouped by prototype. Types not
  in the table are not listed. Rocks are resources, `*-remnants` are remnants.
- Subcategories that say more than the prototype (`kDetails`, read by `subcategoryKey`): machines
  by recipe, drills by what they mine, chests by contents (empty, one item, mixed), pipes and tanks
  by fluid (pipes also by whether they end), wagons by train, ghosts by type, spawners by pollution,
  roboports by network name.
- Trees as forests: trees in the same or touching 8-tile cells. Trees within 25 tiles are listed one
  by one, and a forest of one is said as its tree.
- Resources as patches, by the map's own rule (`ResourcePatchInfo::addPatch` and `scanPatch`): cells
  twice the prototype's patch search radius a side, joined to their 8 neighbours. A patch says the
  map's label of what is left in it. Wells of an infinite resource within 50 tiles are listed one
  by one.
- Water and ice as bodies of 8-way connected tiles, landing on the tile nearest the player.

Trees and resources are counted by cell while the chunks are walked, not kept one by one: a big
base charts millions of them. The chunk iterator also gives entities standing just past the
chunk's edge, which the next chunk gives again, so each chunk keeps only the entities standing in
it.

Entries keep their entities through the game's own weak references (`Targeter`, as its GUIs keep
theirs), so moving entities stay listed where they move to, and an entry drops out once the game
removes its entity. Forests and patches keep none: landing on one finds a live tree or resource at
its place, else the nearest one left in it.

# What Lua adds

Lua says the entries, and lists a few things itself. This runs only on the scanning client, so it
must only read the game, never change it (no `storage`, no `math.random`):

- `readout.lua`: what is said of an entity (fa-info's scanner readout, with a spawner's pollution).
- `extras.lua`: pins, map tags and the spots near the player where the offshore pump in hand can be
  built, handed to the DLL at refresh. Lua keeps these objects in a table of its own until the next
  refresh, and says them.

# Moving through the list: why it stays in sync in multiplayer

Moving onto an entry moves the cursor and the player's selection, which are game state, so every
client must do the same. But only the scanning client has the list.

So the move rides on the key's own custom input. When a scanner key reaches the game, the DLL moves
through its list and makes `PlayerInputSource::getCursorMapPosition` read as the entry's position
while the game turns the key into its input action. That action carries the position to every
client as the event's `cursor_position`. The Lua handler (`entrypoint.lua`, `move`) lands there,
the same on every client, and only then asks the DLL (`fa_native.scanner_entry`) what to say, which
is speech alone. Category keys move nothing in the game; Lua asks `fa_native.scanner_category` what
to say.

The DLL leaves the keys alone while a game window is open or one of the mod's own UIs is
(`fa_native.scanner_mod_ui`, set each tick); those take the keys for themselves.

# Adding to the scanner

- A new entity type: add it to `kTypes` in `scanner.cpp` with its category.
- A subcategory that says more than the prototype: add the type to `kDetails` in `scanner.cpp`
  with the member its entity class keeps it in (a `game::Layout` field), and read it in
  `subcategoryKey`.
- Something only Lua can find (like pins): add a function to `extras.lua` returning entries with a
  category, a subcategory key, a position, `valid` and `readout`, and call it from `collect`.
- A new category: add its key to `kCategoryKeys` (in the order the category keys move through) and
  `Cat` in `scanner.cpp`, to `CATEGORIES` in `scanner-consts.lua`, and `scanner-category-<key>` to
  the locale.
