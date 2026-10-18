# To do

Larger pieces of work agreed on but not started.

## Graph sonification

The game's statistics windows draw graphs that FA does not read yet. The electric network window
(an electric pole's, `ElectricNetworkScreen`) leaves out the graph above each column's table:
production, consumption and accumulators. The production statistics window draws the same kind of
graph (`FlowDataFrame::graph`).

Sonify them as a project of its own: play a graph's line as sound over time, so its shape (rising,
falling, steady, spikes) is heard.

- Where they are: `FlowDataFrame<...>::graph` (an agui `Graph`), the time span from the window's
  precision buttons (`precisionIndex`), and the series and their colours from `colorMapping`.
  `layout.flowFrameGraph` resolves the graph's offset.
- Clicking a row's icon filters the graph to that row, and right clicking leaves the row out.
  That is useless until the graph is heard.

## Tutorials in Tips and tricks

Write FA's own tutorials as tips with a tutorial map, opened from the game's Tips and tricks
window like the base game's five (`data/base/tutorials`). This is the game's own UI, so FA adds no
menu, and the map is played on its own, set aside from the player's game.

- A `tips-and-tricks-item` prototype gives the text, a `trigger` that unlocks it, `dependencies`,
  an optional `simulation` and `tutorial`, the name of a `tutorial` prototype. That prototype's
  `scenario` is a folder in the mod's `tutorials` folder: a map and a `control.lua` that runs the
  lesson. A `tips-and-tricks-item-category` prototype gives FA its own heading.
- The engine runs no mod's `control.lua` in a tutorial. `native/src/tutorial.cpp` loads
  FactorioAccess and Kruise_Kontrol there, so FA's keys and scanner work in its own tutorials too.
  An error in FA's `on_init` there crashes the game as it returns to the suspended game.
- Tutorials point with `arrow` entities, which the reader says as "Indicated spot".
- Unlocks are kept per profile, not per save: on Steam in the Steam Cloud copy of
  `player-data.json`.
- Decide which lessons go here and which go in the campaign levels.
