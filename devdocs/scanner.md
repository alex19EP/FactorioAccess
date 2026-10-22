Last reviewed: 2026-10-12 (update this if reviewing this doc)

# Introduction

The scanner turns the map into a list the player moves through with `PAGE UP` and `PAGE DOWN` plus
modifiers: categories (`CONTROL`), subcategories (no modifier) and entries (`SHIFT`). `HOME` says
the current entry again. The list keeps itself up to date; `END` lists the surface again at once,
sorted from where the player is (`SHIFT + END` only in the direction the character faces, which
stays until the next `END`). Examples: an assembling machine is category Production, subcategory by
its recipe; an iron ore patch is category Resources, subcategory iron ore.

The list lives in the native DLL (`native/src/scanner.h`, `scanner.cpp`), on the client of the
player who scans. Nothing of it is in `storage`, and other clients have none. The Lua side is
`scripts/scanner/`.

# What the DLL lists

A refresh (`fa_native.scanner_refresh`, only where `fa_native` exists) walks every chunk of the
surface that the player's force has charted, as sighted players see all of it on the map:

- Entities, each its own entry, grouped by prototype. Every entity type is listed but the effects
  and helpers in `kSkippedTypes` (projectiles, explosions, fire, stickers, smoke, beams, spider legs,
  demolisher segments, the rocket in a silo ...). `kTypes` picks a type's category; a type it does
  not know goes to Other. Mods add prototypes, never types, so modded entities are listed too.
  Rocks are resources: a simple entity is a rock when the game counts it as one for the
  deconstruction planner's "trees and rocks only" (`count_as_rock_for_filtered_deconstruction`:
  volcanic rocks, chimneys, Fulgoran ruins, stromatolites, lithium icebergs, demolisher corpses,
  modded rocks). `*-remnants` are remnants.
- Subcategories that say more than the prototype (`kDetails`, read by `subcategoryKey`): machines
  by recipe, drills by what they mine, chests by contents (empty, one item, mixed), pipes and tanks
  by fluid (pipes also by whether they end), wagons by train, ghosts by type, spawners by pollution,
  roboports by network name.
- Trees and plants (Gleba's, and what agricultural towers plant) as forests: trees in the same or
  touching 8-tile cells. Trees within 25 tiles are listed one by one, and a forest of one is said as
  its tree.
- Resources as patches, by the map's own rule (`ResourcePatchInfo::addPatch` and `scanPatch`): cells
  twice the prototype's patch search radius a side, joined to their 8 neighbours. A patch says the
  map's label of what is left in it. Wells of an infinite resource within 50 tiles are listed one
  by one.
- Bodies of 8-way connected tiles of one kind and one fluid, landing on the tile nearest the player
  and named by the tile most of the body is ("Deep water 54 by 40"). The kind comes from the tile's
  collision layers (`prototypes::tileKind`, data/base/prototypes/tile/tile-collision-masks.lua), so
  modded tiles take part: water_tile without ground_tile is water-like, a liquid when it also blocks
  the player (water, lava, ammoniacal ocean, brash ice) and shallow when it does not (shallow water,
  Gleba's wetlands, the oil ocean); meltable is Aquilo's ice. A body an offshore pump pumps from (the
  tile has a fluid) is a resource, the others terrain. The cursor reads the same kinds: a water-like
  tile says its shape, and the skip treats it as water.
- Asteroid chunks floating past a platform, each kind a subcategory of Resources. They are no
  entities (`SpacePlatform::asteroidChunks`), but are linked as entities are, so entries follow them
  and drop out once collected.

Things that move (units, vehicles, characters, robots: `kMovingTypes`) of another force are left out
of chunks under fog of war, which the player's force has not charted in the last 600 ticks
(`Chart::isChunkCoveredByFogOfWar`). The map shows them there where they were when last charted, or
not at all. The force's own are listed wherever they are, as the map draws them live.

# The map's list

While the full map is open (`Player::renderMode` chart, remote view zoomed out past 200 tiles), the
list holds only what the map names rather than drawing as a pixel of a building's colour (`kMapTypes`,
`Rule::onMap`): vehicles and trains, stations, players, enemies, display panels shown on the map,
resource patches, forests, water, ice, pins and tags, and alerts, which only the map draws.

The map's list also holds the player's alerts on the surface, each type (`AlertType::str`) a
subcategory of Alerts: what `ChartRenderer::drawAlerts` draws, read from `Player::alertCenter` as
`AlertCenter::getAllUniqueAlerts` reads it (each type's alerts, then the custom ones shown on the
map), except that an entity with alerts of several types is listed under each. An alert about an
entity is linked to it. An alert lasts its type's TTL unless the game raises it again, and disabling
a type erases its alerts, so each move checks the game still holds the alert and passes over one it
dropped, as its icon leaves the map. Lua says it by the alerts configuration's name of its type
(`alerts-config-gui.name-<type>`), or a custom one by its message, then its entity. These are what the map's own pointing selects
(`Chart::getSelection`) or labels. Prototypes flagged `not-on-map` are left out, single trees and
wells are not listed apart from their forest or field, and build spots are left out, as nothing but
blueprints and rails is built on the map.

Either list, map or not, holds what the logistic networks window marks on the map (Marked, each
prototype a subcategory, kept out of All). `GameView::draw` has the window gather them every frame
(`LogisticNetworksGui::drawSelectedItem` or `drawSelectedEntity` fill `entitiesToDraw`) and draw them
with `drawEntities`, which FA hooks while the world stands still: a changed set is linked at once and
starts an automatic refresh, and a set not drawn for half a second (the window closed, the selection
cleared) is no longer marked, so each move passes over an entry the window stopped marking. Without a
selection the window draws only its roboports on the full map, which mark nothing. The tile reader
says "marked" on a marked entity.

Opening or closing the map starts a new list at once, from where the camera is, and the cursor stays
on what it was on when the new list has it. Until the new list is put in place, the old one is used.

Trees and resources are counted by cell while the chunks are walked, not kept one by one: a big
base charts millions of them. The chunk iterator also gives entities standing just past the
chunk's edge, which the next chunk gives again, so each chunk keeps only the entities standing in
it.

The walk runs on several threads (`parallelFor`), each taking batches of chunks and keeping its own
items, cells and tiles. This is safe because the world stands still while the update thread waits in
the Lua call, and everything the walk calls in the game only reads (the entity iterator,
`ForceData::isChunkCharted`, prototype getters). Anything that writes into the game, such as
linking the weak references, stays on the calling thread. The threads' items are put back in chunk
order, and ties for the nearest go to the topmost, then the leftmost, so the list is the same however
the chunks were shared out. Sorting the subcategories also runs on several threads.

# Automatic refresh

Each tick, `entrypoint.lua`'s `on_tick` calls `fa_native.scanner_tick`, which answers true when a
refresh is due for this client's player: at once on another surface, else a second after the last
one, or longer after a long one, so that automatic refreshes take at most 5% of the update thread
(`kCycleShare`). On a 4M SPM base's Nauvis, one runs every 18 seconds. Lua then starts one with
`automatic = true`.

An automatic refresh walks the chunks nearest first, 2 ms each tick (`kSliceBudget`), while the
update thread waits in the Lua call. Each slice links the entities it found before the world moves
on: an entity found one tick may be gone the next. Once all chunks are walked, the list is put
together (merged, clustered, grouped) on a thread of its own, as that reads nothing of the game. At
the next tick after that, the list takes the old one's place, and the cursor stays on what it was
on: the same entity, the forest, patch or body holding where it was, or else the same subcategory.
The old list's links are dropped a slice at a time, and the rest of it on a thread of its own.

An automatic refresh keeps the list's direction filter and, in remote view, its origin, since the
camera follows the cursor onto the entries. A list of a surface the player has left is not used
while the new one is made.

The mod's extras are collected again for each refresh, while the list made from the last collection
may still be in use. So each collection has a generation, which the DLL says back with each extra
(`extras.lua` keeps the latest three).

Entries keep their entities through the game's own weak references (`Targeter`, as its GUIs keep
theirs), so moving entities stay listed where they move to, and an entry drops out once the game
removes its entity. Forests and patches keep none: landing on one finds a live tree or resource at
its place, else the nearest one left in it.

# What Lua adds

Lua says the entries, and lists a few things itself. This runs only on the scanning client, so it
must only read the game, never change it (no `storage`, no `math.random`):

- `readout.lua`: what is said of an entry. An entity is read by the DLL's tile reader as on its own
  tile (`fa_native.entity_read`), so the scanner says of it what the cursor does; Lua adds a
  spawner's pollution, the direction and distance, and the entry's place in its list.
- `extras.lua`: pins, map tags and the spots near the player where the offshore pump in hand can be
  built, handed to the DLL at each refresh. Lua keeps these objects in a table of its own (see
  "Automatic refresh"), and says them.

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

The DLL leaves the keys alone while a game window is open, which takes them for itself.

# Adding to the scanner

- An entity type in the wrong category: add it to `kTypes` in `scanner.cpp` with its category (a
  new engine type lands in Other until then). One that is no thing on the map: `kSkippedTypes`.
- A subcategory that says more than the prototype: add the type to `kDetails` in `scanner.cpp`
  with the member its entity class keeps it in (a `game::Layout` field), and read it in
  `subcategoryKey`.
- Something only Lua can find (like pins): add a function to `extras.lua` returning entries with a
  category, a subcategory key, a position, `valid` and `readout`, and call it from `collect`.
- A new category: add its key to `kCategoryKeys` (in the order the category keys move through) and
  `Cat` in `scanner.cpp`, to `CATEGORIES` in `scanner-consts.lua`, and `scanner-category-<key>` to
  the locale.
