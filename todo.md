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
