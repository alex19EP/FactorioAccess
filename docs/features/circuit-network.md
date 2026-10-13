# The Circuit Network

## Keys

These are the game's own keys, after the keys script:

| Key | What it does |
| --- | --- |
| `alt + r`, `alt + g`, `alt + c` | Take a red, green or copper wire |
| `[` with a wire in hand | Start a wire on the entity the cursor is on, or connect it to this one |
| `q` | Stop the wire; press again to empty the hand |
| `shift + [` on a pole | Remove the pole's copper cables |
| `[` with an empty hand | Open the entity's window, where its circuit settings are |

## Description

The circuit network is configured in the game's own windows: a combinator's window, and the circuit connection part of an inserter's, belt's or chest's window. They are read like any other window; see [windows](ui.md). With alt mode on, the cursor reads the signals a combinator shows.

Teaching the circuit network is beyond these pages. Start at [the Factorio wiki](https://wiki.factorio.com/Circuit_network) if you are new to it.

### Connecting wires

When you take a wire, neither end is connected yet. `[` on an entity attaches it there. Each further `[` on another entity connects the wire to it and carries on from there, so to connect a, b and c you click a, then b, then c: a is wired to b and b to c, but not c to a.

After the first click, the first `q` ends the chain, so the next click starts a new one; a second `q` empties your hand. If this is confusing, empty your hand, take a wire, click everything you want connected, then empty your hand again.

To remove a wire between two entities, connect them again: the second connection of a to b removes it.

Combinators and power switches have two sides. An arithmetic, decider or selector combinator takes two tiles: its input end and its output end. Put the cursor on the end you want before clicking. A power switch's left and right connections are its left and right halves.

What a pole is wired to is in a view beside its window.
