# Factorio API Reference Documentation

Complete markdown documentation for Factorio 2.1.20 API (both runtime and prototype).

Generated from JSON documentation using `json_to_markdown.py` (in the mod root).

## Structure

### Runtime API (`runtime/`)
Documentation for the runtime Lua API (used in `control.lua`).

- **`runtime/classes/`** - 157 Lua classes (LuaEntity, LuaPlayer, etc.)
- **`runtime/concepts/`** - 495 concept types (Position, Color, etc.)
- **`runtime/events/`** - 226 events (on_tick, on_built_entity, etc.)
- **`runtime/defines/`** - 63 define enumerations
- **`runtime/metadata.md`** - API version and metadata

### Prototype API (`prototypes/`)
Documentation for prototype definitions (used in `data.lua`).

- **`prototypes/prototype/`** - 279 prototype types (TransportBeltPrototype, ItemPrototype, etc.)
- **`prototypes/concepts/`** - 703 type concepts (Animation, Trigger, etc.)
- **`prototypes/metadata.md`** - API version and metadata

## Statistics

- **Total files:** 2,013 markdown files
- **Total size:** 5.8 MB
- **API version:** 6
- **Game version:** 2.1.20

## Regenerating

Every Factorio install ships the source JSON in `doc-html/` (`runtime-api.json`, `prototype-api.json`). From the mod root:

```bash
python json_to_markdown.py --type runtime --input <factorio>/doc-html/runtime-api.json --output llm-docs/api-reference
python json_to_markdown.py --type prototype --input <factorio>/doc-html/prototype-api.json --output llm-docs/api-reference
```

The generator only writes files; it never deletes them. APIs removed by a game update leave stale files behind, so delete `runtime/` and `prototypes/` (keep this file) before regenerating.

## Usage

Use these docs as reference when developing Factorio mods. All cross-references, types, parameters, and examples from the official JSON documentation are preserved.

## Navigation

- **Classes:** Look in `runtime/classes/` for Lua object APIs
- **Prototypes:** Look in `prototypes/prototype/` for entity/item definitions
- **Types:** Look in `runtime/concepts/` or `prototypes/concepts/` for type definitions
- **Events:** Look in `runtime/events/` for event documentation
- **Enums:** Look in `runtime/defines/` for enumeration values
