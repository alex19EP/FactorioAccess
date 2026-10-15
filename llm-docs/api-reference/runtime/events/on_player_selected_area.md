# on_player_selected_area

Called after a player selects an area with a selection-tool item.

## Event Data

### area

**Type:** `BoundingBox`

The area selected.

### cursor_position

**Type:** `MapPosition`

The player cursor position.

### entities

**Type:** Array[`LuaEntity`]

The entities selected.

### item

**Type:** `string`

The item used to select the area.

### name

**Type:** `defines.events`

Identifier of the event.

### player_index

**Type:** `uint32`

The player doing the selection.

### position

**Type:** `MapPosition`

The position where the player started the selection.

### quality

**Type:** `string`

The item quality used to select the area.

### selection_mode

**Type:** `defines.selection_mode`

### surface

**Type:** `LuaSurface`

The surface selected.

### tick

**Type:** `MapTick`

Tick the event was generated.

### tiles

**Type:** Array[`LuaTile`]

The tiles selected.

