# SplitterPrototype

A [splitter](https://wiki.factorio.com/Splitter).

**Parent:** [TransportBeltConnectablePrototype](TransportBeltConnectablePrototype.md)
**Type name:** `splitter`

## Properties

### structure

**Type:** `Animation4Way`

**Optional:** Yes

### structure_patch

Drawn 1 tile north of `structure` when the splitter is facing east or west.

**Type:** `Animation4Way`

**Optional:** Yes

### frozen_patch

**Type:** `Sprite4Way`

**Optional:** Yes

### structure_animation_speed_coefficient

**Type:** `double`

**Optional:** Yes

**Default:** 1

### structure_animation_movement_cooldown

**Type:** `uint32`

**Optional:** Yes

**Default:** 10

### related_transport_belt

The name of the [TransportBeltPrototype](prototype:TransportBeltPrototype) which is used for the sound of the underlying belt.

**Type:** `EntityID`

**Optional:** Yes

### circuit_wire_max_distance

The maximum circuit wire distance for this entity.

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

**Type:** (`CircuitConnectorDefinition`, `CircuitConnectorDefinition`, `CircuitConnectorDefinition`, `CircuitConnectorDefinition`)

**Optional:** Yes

### default_input_left_condition

**Type:** `CircuitConditionConnector`

**Optional:** Yes

### default_input_right_condition

**Type:** `CircuitConditionConnector`

**Optional:** Yes

### default_output_left_condition

**Type:** `CircuitConditionConnector`

**Optional:** Yes

### default_output_right_condition

**Type:** `CircuitConditionConnector`

**Optional:** Yes

### exchange_input_lines

When set to true, right lane of left input belt is swapped with left lane of right input belt. This can be used to create a splitter which spreads both lanes of left input belt onto left lanes of both outputs, and both lanes of right input belt onto right lanes of both outputs.

**Type:** `boolean`

**Optional:** Yes

**Default:** False

### exchange_output_lines

When set to true, right lane of left output belt is swapped with left lane of right output belt. This can be used to create a splitter which combines left lanes of both input lanes into left output belt and right lanes of both input lanes into right output belt.

When this option is used, a [IconDrawSpecification::scale_for_many](prototype:IconDrawSpecification::scale_for_many) inside of [EntityPrototype::icon_draw_specification](prototype:EntityPrototype::icon_draw_specification) will need to be provided in case a filter is to be drawn as it will be rendered on respective sides of both output belts. Suggested value is `0.35`.

**Type:** `boolean`

**Optional:** Yes

**Default:** False

