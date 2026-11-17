# PipePrototype

An entity to transport fluids over a distance and between machines.

**Parent:** [EntityWithOwnerPrototype](EntityWithOwnerPrototype.md)
**Type name:** `pipe`

## Properties

### fluid_box

The area of the entity where fluid/gas inputs, and outputs.

**Type:** `FluidBox`

**Required:** Yes

### horizontal_window_bounding_box

**Type:** `BoundingBox`

**Required:** Yes

### vertical_window_bounding_box

**Type:** `BoundingBox`

**Required:** Yes

### pictures

All graphics for this pipe.

**Type:** `PipePictures`

**Optional:** Yes

### circuit_wire_max_distance

**Type:** `double`

**Optional:** Yes

**Default:** 0

### draw_copper_wires

**Type:** `boolean`

**Optional:** Yes

**Default:** True

### draw_circuit_wires

**Type:** `boolean`

**Optional:** Yes

**Default:** True

### circuit_connector

If given, there must be exactly 16 elements in the table.

**Type:** Array[`CircuitConnectorDefinition`]

**Optional:** Yes

### default_fluid_temperature_signal

**Type:** `SignalIDConnector`

**Optional:** Yes

### show_fluid_visualization_when_in_cursor

When this is true, fluid pipelines will be visualized when this entity is held in the cursor.

**Type:** `boolean`

**Optional:** Yes

**Default:** True

**Overrides parent:** Yes

### random_variation_on_create

Whether a random graphics variation is chosen when placing the entity/creating it via script/creating it via map generation. If this is `false`, the entity will use the first variation instead of a random one.

**Type:** `boolean`

**Optional:** Yes

**Default:** True

### draw_window

When this is `false`, pipe will never attempt drawing window.

**Type:** `boolean`

**Optional:** Yes

**Default:** True

