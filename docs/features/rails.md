## Keys

### For the Train Window and its Schedule

Move between the window's parts (the train, the schedule, the interrupts): tab

Jump to the next or previous station of the schedule: control + down and control + up

Move a station, wait condition or interrupt down or up the list: control + shift + down and control + shift + up

Go to the map beside the window, and on through the HUD: control + tab; back with control + shift + tab

On the map, in map view, add a temporary stop where the cursor is: control + left bracket

On the map, add the train stop under the cursor to the schedule: shift + left bracket

On the map, each cursor move ends with what these two would add, as the game draws it while control or shift is held:
"temporary stop" when a rail is within 20 tiles, or "no path to temporary stop" when the train cannot reach it, then
"stop" and the name of the nearest train stop within 20 tiles, again with "no path to" when the train cannot reach it.
The same is said in map view while you ride a train with no window open, where the two clicks also work.

## Explanation

IMPORTANT: See https://wiki.factorio.com/Railway if you don't know how rails work. This is documentation of mod
functionality, not of vanilla behavior.

### Overview of Driving

You can run trains in two modes: manual and automatic.  In automatic mode, trains follow a schedule.  This unlocks after
you research automated rail transportation.

This section mostly exists to tell you: don't try manual driving.  To do it, you use the arrow keys (west is left, etc)
but because track layouts are very complex the train is not likely to do what you think. Additionally, it is liable to
run into other trains.

A sighted player who wants to drive a train through a complicated base, and the accessible way for you to do so, is to
add a temporary stop. Open the locomotive, press control + tab to go to the map, put the cursor near the track where you
want to go (the map says "temporary stop" there, or "no path to temporary stop"), and press control + left bracket.
The game adds the temporary stop to the schedule and the train heads
there. Add "passenger not present" to it as a wait condition if it should wait for you to get off.

What we do offer is a radar that tells you which way and how fast a train is moving. When sitting in a moving train, you
will hear a quiet tone in the direction the train is going.  Higher pitch is "more north", and pan is east/west.  So a
high leftish tone is northwest, etc.  The tone plays when the train changes orientation, or after the train has traveled
50 tiles. This at least allows you to (1) have the sighted experience of advanced players who can't really drive trains
manually anyhow, and (2) know what's going on and roughly where and how far you have gone.

### Overview of Track Reporting

Factorio's train system consists of 3 track shapes: straight (can be oriented 16 ways), and two kinds of curves (a and
b, hidden behind the mod).  Most of this is hidden from you.  Rails are built with the game's own rail planner, a rail
in hand, as the sighted build them.

Sometimes, you will observe more than one track on a tile.  This can happen in two ways:

- If using shift + f to cycle, in which case the track may be spurious because some track pieces share tiles to connect
- When moving, in which case you are on some form of crossing where two tracks overlap.

We simplify each tile into a layout description.

You may find:

- Vertical or horizontal rails
- Diagonal rails facing northeast or southeast
- Half-diagonal rails facing in any of the secondary eastern directions, for example east northeast
- Various kinds of curves
- Various kinds of forks:
  - Fork: 3-way in a given direction
  - fork left/right of dir: fork continuing in direction dir, and also turning left/right
  - split: The track does not continue straight. It splits into left/right instead. This can also be thought of as a y.

The straight rails only ever face half of the compass because they are bidirectional. A horizontal east is also a
horizontal west.

The mod helps out with curves by abstracting where possible.  It will announce either a direction and a "left/right"
specifier, or it will announce "x and y turn" if it understands that this is part of a 90-degree turn between cardinal
directions (for example north and east).

Some examples:

- Right of southeast: curve going southeast to south southeast
- left of north: curve going from north to north northwest
- East and south turn bottom: the southernmost part of a east/south 90 degreee turn

NOTE: in Factorio 2.0 a 90 degree curve is 4 turns as the shallowest corners  are now only 22.5 degrees.

As you build more of a larger shape, the mod will recognize more of that larger shape and report it to you.  For
example, until the 4th place of a 90 degree turn is placed, it will describe the three smaller curves separately.  You
can use this to help guide you as to whether or not you have built what you wanted.

For 90 degree turns off the cardinals we use the interesting fact that all turns have a "height" and assign the 4
segments a bottom/lower half/upper half/top designation, going from southmost to northmost on the map.

Unlike 1.1, there is no requirement that straight pieces be present anywhere.  You can now do circles with only curved
pieces.  You can do long straight paths with s-bends going back and forth like a snake.  I don't suggest either for a
variety of reasons but you can, so can the sighted, and it's your game.

Ghosts are supported.  The only thing to note is that tiles containing both ghosts and non-ghosts are potentially overly
verbose.  We cannot do better here in any reasonable amount of time.

### Schedules and Stations

The train stop's window and the locomotive's window are the game's own, read as they show. Teaching how train schedules
work is beyond the scope of mod documentation.  See the wiki for that.

A locomotive's window opens beside the map, in remote view following the train. Its first part holds the train's
controls: its group, manual or automatic, drive remotely, and the Schedule and Fuel tabs. The schedule lists each
station with its buttons (go now, change station, remove, unload) and, under it, its wait conditions, each with its
own controls and remove button, then the station's "add wait condition" button. Enter on a button does what clicking it
does: "add station" and "add wait condition" open the game's lists to choose from. The interrupts are the next part;
"edit interrupt" opens the game's window for it, whose conditions and targets read the same way.

The game orders stations, conditions and interrupts by dragging them; control + shift + up and down drag the one under
the cursor by one place, and the cursor follows it.

Train groups are set in the locomotive's window, as in vanilla. Groups are created by just deciding to use their name and
are destroyed by destroying the last train (or ghost) using the group.  It's more like channels than groups: any train
"tuned" to the same name "receives" the schedule.

### Interrupts and Rich Text

The mod reads rich text wherever the game shows it: an icon is said by its name.

This is relevant to trains (and pretty much only trains) so it is documented here.  In the interrupt system, it is
possible to put icons into station names and then make an interrupt condition like "any cargo > 5".  The train will then
go to (for example) "copper plate item dropoff" if it happens to be copper plates and there is a station with that name,
or "iron plate item dropoff" if it is iron plates.  As with other game mechanics, see the Factorio wiki's railway page
to learn the power of this system--here we just explain how it currently works with the mod.  The two wiki pages you
need are:

- For railways: https://wiki.factorio.com/Railway
- For rich text: https://wiki.factorio.com/rich_text

If you are reading this document and don't know what an interrupt does, start by reading those pages.

To start with how to get to and make interrupts, the train schedule has an add interrupt button, which opens the game's
list of interrupts and a field to type a new name.  Like groups, there is no explicit create or delete.  If a train is
using an interrupt it exists, otherwise it does not. So to create one just start using the name. Interrupts move like
stations to change their priority, and "edit interrupt" opens the game's window for one.

This brings us to the stuff about rich text.  There are two places you may wish to use a wildcard:

- In conditions: the game's signal chooser holds the parameter signals ("Item parameter" and the like).
- In station names: the game's text fields take rich text typed in its long form, for example `[item=iron-plate]`, or
  the wildcard `[virtual-signal=signal-item-parameter]`.

Factorio wiki pages usually list the prototype name for a given item. There is also the
[data.raw](https://wiki.factorio.com/Data.raw) page with a complete listing.

### The Global Trains Overview

One unique problem with trains is that they move around and do not stay still long enough for you to click them.  The
game's trains overview, `o`, lists every train, stop and train group of a surface.

Each train reads its state; Enter on it shows the train on the map, and the button beside it opens the train's window.

A typical workflow:

- Find the train in the overview
- Open its window from there, and set it to manual if it should stop where it is
- Reconfigure it

Well-designed train layouts with proper signal placement are robust against trains suddenly switching to manual at any
time.  Figuring out what well-designed means is the point of the game, however, and so not covered further here.
