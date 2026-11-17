# UndergroundBeltPrototype

An [underground belt](https://wiki.factorio.com/Underground_belt).

**Parent:** [TransportBeltConnectablePrototype](TransportBeltConnectablePrototype.md)
**Type name:** `underground-belt`

## Properties

### max_distance

**Type:** `uint8`

**Required:** Yes

### structure

**Type:** `UndergroundBeltStructure`

**Optional:** Yes

### underground_sprite

**Type:** `Sprite`

**Optional:** Yes

### underground_remove_belts_sprite

**Type:** `Sprite`

**Optional:** Yes

### max_distance_underground_remove_belts_sprite

**Type:** `Sprite`

**Optional:** Yes

### underground_collision_mask

The underground connection may be defined as colliding with tiles in which case if any colliding tile is placed between underground ends the connection will not be established.

**Type:** `CollisionMaskConnector`

**Optional:** Yes

**Default:** "no masks"

### max_distance_tint

**Type:** `Color`

**Optional:** Yes

