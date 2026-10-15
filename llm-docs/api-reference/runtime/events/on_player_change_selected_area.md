# on_player_change_selected_area

Called after a player changes the selection area or selection type with a selection-tool item that has [SelectionToolPrototype::send_pre_selection_events](prototype:SelectionToolPrototype::send_pre_selection_events) set to `true`.

Because a player's selection state exists solely on their local client and not in game state, this event and the other area preselection events are provided with "best effort", but are not guaranteed. In some situations, such as after loading an existing save or in a multiplayer game, preselection events may not be received in an order that makes sense for the current game state or may be dropped altogether.

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

