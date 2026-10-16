# The Scanner

## Key Reference

NOTE: alternate keys for keyboards without the following are not currently available but will return soon

Refresh the scan: `end`

Refresh the scan, filtering it to the direction your character is facing: `shift + end`

Move the cursor to the selected item: `home`

Move between scanner categories: `ctrl + page up/down`

Move between scanner subcategories: `page up/down`

Move between instances in a subcategory: `shift + page up/down`

## Description

IMPORTANT: API limitations in 2.0 have temporarily degraded scanner latency. We are working to fix this.

IMPORTANT: mod upgrades sometimes cause the scanner to reset itself and take some time to start working again because the scanner does computation in the background. This is expected.  If the scanner's data seems odd after an upgrade, it will usually correct itself within 1 to 2 minutes as it has a chance to rescan the map.  Most mod upgrades will not wipe out scanner state.

If the cursor is the mouse, the scanner is your eyes.  The scanner takes the map of tiles and extracts useful features out of it, for example "a million iron ore".

The scanner is a 3-level hierarchy composed of a category, subcategory, and instance.

Categories are "what kind of thing do I want, in general?" and include things such as resources, terrains, and various kinds of buildings.

Subcategories are things like "all iron ore patches".

Instances are things like "this individual iron ore patch".

Moving through categories, subcategories and instances only reads them; the cursor stays where it is.  `home` moves the cursor onto the instance you are on, opening remote view when it is off the screen.

This seems more complicated than it is, so here is a concrete example.  You want to find iron ore. To do that:

- Use `ctrl + page up/down` to find "resources"
- Use `page up/down` to find the closest example of iron ore
- Use `shift + page up/down` to move through all iron ore patches, sorted by distance
- Press `home` to move your cursor onto the patch you are on

It is important to digress and talk about charting.  Factorio has fog of war.  What this means is that the map does not show up until you explore it.  This does not just include not seeing it, but literally not generating it.  For that reason, as well as to line up with Vanilla game mechanics, the scanner will not account for tiles until that part of the map is charted.  This can manifest in a few ways, the most notable of which being that sometimes new resource patches appear, but in some cases things such as water that were on the edge of the map can suddenly become larger.

Unfortunately walking and driving to explore are not fun experiences if you are blind.  To make up for this, the mod adds the accessibility radar, found in the crafting menu.  Once you have power, you can set one up and it will slowly reveal the map within a 1024 by 1024 box centered on the radar.  If you need more, you must set up new radars at the edges of that box.  For 99% of players, however, one accessibility radar is enough.

Do not confuse accessibility radars with normal radars.  Normal radars have a much smaller range.  The primary difference is that an accessibility radar charts a huge range of the map for the first time but does not refresh where things are rapidly, so for sighted players it's not enough to let them see enemy movement.

While the map is open, the first category after "all" is alerts: the alerts the game shows on the map, such as a building being damaged or a turret out of ammunition, on the surface you are on. The game draws them on the map only, so the scanner lists them only there. Each kind of alert is a subcategory, named as the side menu's alerts configuration names it, and each alert an instance, read as the thing it is about. An alert that ended, as its icon leaves the map, is passed over, and one of a kind you disabled in the alerts configuration is never listed, as the game does not show it.

Finally, the scanner always remembers what you were last on, so `home` also serves as a bookmark: it moves your cursor back to that entry.
