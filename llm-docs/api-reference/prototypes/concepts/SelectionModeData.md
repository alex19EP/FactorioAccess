# SelectionModeData

**Type:** `Struct`

## Properties

*These properties apply when the value is a struct/table.*

### border_color

**Type:** `Color`

**Required:** Yes

### count_button_color

**Type:** `Color`

**Optional:** Yes

**Default:** "Value of border_color"

### chart_color

**Type:** `Color`

**Optional:** Yes

**Default:** "Value of border_color"

### cursor_box_type

**Type:** `CursorBoxType`

**Required:** Yes

### draw_selection_counts

Whether the list of how many entities are selected should be shown.

**Type:** `boolean`

**Optional:** Yes

**Default:** True

### mode

**Type:** `SelectionModeFlags`

**Required:** Yes

### entity_filters

**Type:** Array[`EntityID`]

**Optional:** Yes

### entity_type_filters

**Type:** Array[`string`]

**Optional:** Yes

### tile_filters

**Type:** Array[`TileID`]

**Optional:** Yes

### started_sound

**Type:** `Sound`

**Optional:** Yes

### ended_sound

**Type:** `Sound`

**Optional:** Yes

### play_ended_sound_when_nothing_selected

**Type:** `boolean`

**Optional:** Yes

**Default:** False

### entity_filter_mode

**Type:** `"whitelist"` | `"blacklist"`

**Optional:** Yes

**Default:** "whitelist"

### tile_filter_mode

**Type:** `"whitelist"` | `"blacklist"`

**Optional:** Yes

**Default:** "whitelist"

### ignore_cannot_select_entities

If this is `false`, using any of the mode flags `"blueprint"`, `"deconstruct"`, `"cancel-deconstruct"`, `"upgrade"`, `"cancel-upgrade"` or `"downgrade"` without also specifying another flag that selects entities will result in an error, because the selection tool cannot select any entities and this is likely undesired behavior.

Setting this to `true` disables this error.

**Type:** `boolean`

**Optional:** Yes

**Default:** False

### ignore_cannot_select_tiles

If this is `false`, using any of the mode flags `"blueprint"` or `"deconstruct"` without also specifying any flag that selects tiles will result in an error, because the selection tool cannot select any tiles and this is likely undesired behavior.

Setting this to `true` disables this error.

**Type:** `boolean`

**Optional:** Yes

**Default:** False

