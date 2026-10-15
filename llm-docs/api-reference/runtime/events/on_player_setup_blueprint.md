# on_player_setup_blueprint

Called when a player selects an area with a blueprint.

## Event Data

### alt

**Type:** `boolean`

If normal selection or alt selection was used.

### area

**Type:** `BoundingBox`

The area selected.

### cursor_position

**Type:** `MapPosition`

The player cursor position.

### item

**Type:** `string`

The item used to select the area.

### mapping

**Type:** LuaLazyLoadedValue[Dictionary[`uint32`, `LuaEntity`]]

The blueprint entity index to source entity mapping. Note: if any mod changes the blueprint this will be incorrect.

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

### record

**Type:** `LuaRecord` *(optional)*

The record that is being reassigned. This blueprint record is writable even if it is in the "my blueprints" shelf.

### stack

**Type:** `LuaItemStack` *(optional)*

The item stack used to select the area.

### surface

**Type:** `LuaSurface`

The surface selected.

### tick

**Type:** `MapTick`

Tick the event was generated.

