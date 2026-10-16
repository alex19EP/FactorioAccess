# Movement, the Cursor, and Building

Every key is listed in [the key reference](keys.md). This page explains how they fit together.

## The cursor

The mod replaces the mouse with a cursor that you move over the map's tile grid with `w`, `a`, `s` and `d`. The game's own mouse position follows it, so every game control acts where the cursor is: `[` builds or opens there, `]` mines there, `q` pipettes there. This is why the keys script gives every mouse button a key: `[` is the left button, `]` the right button and `\` the middle button.

As the cursor moves, it reads what is on its tile, as a sighted player would see it: the entity, its state, and the ground. `y` reads the game's full information panel for it, the one a sighted player sees on hovering. `shift + f` steps through everything on the tile, for example a belt and the item lying on it.

The Factorio world is continuous, but all buildings sit on the tile grid. Walking stays smooth: your character can stand at x = 1.23. Walk with the arrow keys.

The keys for the cursor are spread over both hands on purpose. `d` then `[`, over and over, builds a row of things quickly because each hand does one key.

To find where things are:

- `k` reads the cursor's coordinates.
- `shift + k` reads how far and in which direction it is from your character, and `alt + k` the same as east or west and north or south.
- `alt + j` reads your character's coordinates, and `j` brings the cursor back to your character.
- [The scanner](scanner.md) lists what is on the map and moves the cursor to it.

### Cursor skipping

`shift` with `w`, `a`, `s` or `d` skips up to 100 tiles, stopping at the first change: water to ground, a belt's corner, the start or end of a row of entities, the line of an audio ruler.

Many buildings are bigger than one tile. `ctrl` with `w`, `a`, `s` or `d` moves the cursor by the size of the building in hand, so that building again makes a neat row or column. An assembling machine is 3 by 3, so `ctrl + d` moves 3 tiles east. This works for blueprints too.

### The character

Your character is your body. You mostly need not think about it, but it must be within reach of what you build, mine or open, unless you place ghosts. When something fails, the game says why, for example "Cannot reach".

Your character is also what picks things up. Stand on or beside a belt and hold `f` to pick up from it; the mod says what is being picked up.

`i` anchors the cursor one tile in front of your character, so it moves as you walk. Press it again to free the cursor.

### Alt mode and status icons

The cursor reads the icons the game draws on an entity. Status icons such as no power, no fuel or no ammo are read whenever the game shows them. The rest are alt mode icons: a machine's recipe and modules, a chest's contents, filters, the fluid in a pipe, a turret's ammo, a combinator's signals. These are read only while alt mode is on, which the game's own key toggles (`alt` by default). These are the game's own icons, so modded entities read the same way.

The cursor also reads what the game draws on an entity besides icons: an arithmetic, decider or selector combinator's operation on its display ("multiply", "greater than", "select maximum"), always; which way a loader works ("input" or "output"); and a display panel's text. In alt mode, with the interface setting for combinator settings, a combinator's operation comes with its signals: "Iron plate multiply, output Signal B".

## Building

With something buildable in hand, `[` places it. You hold a building by its north-west corner, so the cursor's tile is the building's top left. Sighted players hold buildings by the centre; it can matter when you talk with them.

`shift + [` places a ghost instead. Ghosts mark where something will be built later, and can be placed at any range. Construction robots, and Kruise Kontrol, build them.

`r` and `shift + r` rotate the building in hand, and `h` and `v` flip it; the mod says the new direction. On a placed building they rotate or flip it.

With a building in hand, the mod then says how it would be built: its size, whether it is flipped, and where its pipe and heat connections leave it, such as "pipes South 2", the middle tile of the south side. A pipe connection that lets fluid only one way says so: "pipes out North". A mining drill says where its output comes out and what takes it there, such as "output West 2 to Transport belt on North lane". When a drill drops onto a belt running straight away from it or toward it, flipping the drill moves its output to the other lane.

With a building or blueprint in hand, `i` detaches the cursor: the build stays where it is while you move the cursor to look around it. `ctrl` with `w`, `a`, `s` or `d` then moves the build one tile, and `[` builds it where it is. `i` again brings the cursor back to it.

See [blueprints and planners](blueprints-and-planners.md) for blueprints.

### What the preview shows

With something buildable in hand, each cursor move reads what the game's preview shows a sighted player there: why it cannot be built, then what it highlights, each with its distance and direction.

- `power from` the poles that would power it, or `no power`.
- `wire to` the poles a pole would connect to, or `no wires`.
- `covers` what a pole's supply area would reach.
- `works with` the inserters, drills and machines that would put into or take from it.
- `pairs with` its underground partner.
- `replaces`, `turns` or `changes` what building it would affect.
- `links to` the roboports a roboport would connect to, and `in network` or `no network` for a logistic chest.

These are the game's own highlights, so modded entities read the same way. During a drag only what blocks the build is read.

### Drag building

Hold `[` and move the cursor to place a line. This is the game's own drag building, the same as a sighted player dragging with the mouse button held, so it follows the game's rules:

- The line stays straight, along the way you first moved. Steps to the side build nothing.
- To turn a belt line, press `r` while still holding `[`, then step the cursor the way the belt should go. `r` says "turn at next step", and the step says the corner and its new direction.
- Electric poles are placed as far apart as they can reach.

While `[` is held, cursor steps are silent unless something blocks the build; the game's build sound marks each placed entity.

To build a line while walking, anchor the cursor with `i`, hold `[`, and walk:

- Walk your character to where the line should start.
- Put a belt in hand, press `i`, and hold `[`.
- Walk in a straight line to where the belts should stop, then let go.

### The audio ruler

The ruler is for alignment. `ctrl + alt + b`, or the audio ruler button on the shortcut bar, puts the audio ruler tool in hand. Click a tile with it (`[`) and a ruler forms an audio cross there. When your cursor or character crosses it, it plays a tone. You can place as many rulers as you like; `shift + [` on a ruler's line removes the nearest one on it.

Cursor skipping stops where the cursor comes into line with more rulers than before: entering a line, a crossing line, or a ruler's own tile.

Each ruler is drawn on the ground, 100 tiles along each arm, in its owner's colour, so sighted players see it too. The tool works with the mouse as well: click to place, shift click to remove.

For example, to place a belt going east, find where it should end, move a few tiles north, and place a ruler. Then hold `[` and walk east until you hear the ruler. The line does not stop by itself at the ruler.

## Mining

Hold `]` on something to mine it. Ore must be next to your character; rocks, trees and buildings can be mined from as far as you can reach. This is also how you take buildings back. To clear many trees, rocks or ghosts at once, use a deconstruction planner; see [blueprints and planners](blueprints-and-planners.md).

## The pipette

This is the game's own. `q` with an empty hand puts the most fitting thing for what the cursor is on in your hand: the same building, if you have one, a drill for ore, an offshore pump for water.

## Kruise Kontrol

[Kruise Kontrol](https://mods.factorio.com/mod/Kruise_Kontrol) is a mod that automates walking and simple jobs, which makes up for not having a mouse. Put the cursor on something and press `ctrl + alt + ]`. `enter` cancels.

What happens depends on what the cursor is on:

- An empty tile: you walk to it.
- A resource, rock or tree: you mine it.
- A ghost: you build it, crafting as needed.
- Something marked for upgrade or deconstruction: you upgrade or deconstruct it.
- Something that takes fuel: you refuel it.
- An enemy: you fight it.

It keeps doing the job until there is nothing more nearby; started on a ghost, it builds every ghost it can find and has the items for. It drives vehicles too. FactorioAccess says what Kruise Kontrol reports.

To just walk, whatever the cursor is on, press `ctrl + shift + ]` instead. This is FactorioAccess's key, there only with Kruise Kontrol installed: it has Kruise Kontrol walk to the cursor as it does to an empty tile, so ore, trees and rocks are not mined. In a car, tank or spidertron it drives there, as Kruise Kontrol always does in one.

## Belts

A belt's window has FactorioAccess's belt analyzer beside it, with four views:

- This belt, its two lanes in four slots each.
- Upstream, everything on the belts feeding it.
- Downstream, everything on the belts it feeds.
- Total, both, plus this belt.

It stops where the circuit network would: at a splitter or a belt's end. It does not go round a loop more than once, so on a loop upstream and downstream mean little.

Percentages over 100% and two items in one slot have two causes. In Space Age, stacked belts make a belt more than full. Otherwise, belts briefly squeeze items together where inserters or drills drop them, at corners and before splitters. In the base game without stack inserters, numbers far over 100% may be a bug; please report it.

## Time of day

`t` reads the time of day. The game has no clock, only light: you hear day, dusk, night or dawn, and at dusk and dawn how much light there is.
