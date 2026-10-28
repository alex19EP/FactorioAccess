# CargoPodPrototype

Entity used to send cargo between the [cargo hatches](prototype:CargoHatchDefinition) of [cargo landing pads](prototype:CargoLandingPadPrototype), [space platform hubs](prototype:SpacePlatformHubPrototype), [cargo bays](prototype:CargoBayPrototype) and [rocket silos](prototype:RocketSiloPrototype).

The pod for sending cargo is determined by [CargoHatchDefinition::cargo_unit_entity_to_spawn](prototype:CargoHatchDefinition::cargo_unit_entity_to_spawn). A cargo pod will only be sent to a destination if [receiving_cargo_units](prototype:CargoHatchDefinition::receiving_cargo_units) of the destination hatch contains the cargo pod.

**Parent:** [EntityWithOwnerPrototype](EntityWithOwnerPrototype.md)
**Type name:** `cargo-pod`

## Properties

### default_graphic

Has to be of type 'pod-catalogue'.

**Type:** `ProcessionGraphic`

**Optional:** Yes

### default_shadow_graphic

Has to be of type 'pod-catalogue'.

**Type:** `ProcessionGraphic`

**Optional:** Yes

### procession_graphic_catalogue

**Type:** `ProcessionGraphicCatalogue`

**Optional:** Yes

### procession_audio_catalogue

**Type:** `ProcessionAudioCatalogue`

**Optional:** Yes

### shadow_slave_entity

**Type:** `EntityID`

**Optional:** Yes

### inventory_size

The amount of item stacks that this cargo pod can transport.

**Type:** `ItemStackIndex`

**Required:** Yes

### spawned_container

The entity that is created if this cargo pod lands on the ground instead of in a cargo hatch. The transported items will be transferred into the spawned entity.

**Type:** `EntityID`

**Required:** Yes

### impact_trigger

**Type:** `Trigger`

**Optional:** Yes

### selection_priority

The entity with the higher number is selectable before the entity with the lower number.

The value `0` will be treated the same as `nil`.

**Type:** `uint8`

**Optional:** Yes

**Default:** 20

**Overrides parent:** Yes

