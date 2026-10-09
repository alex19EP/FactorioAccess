local Consts = require("scripts.consts")

local mod = {}

-- The scanner's categories, by the keys the DLL names them with (native/src/scanner.cpp
-- kCategoryKeys, which also keeps the order the category keys move in) and the locale names them
-- with (fa.scanner-category-<key>).
---@enum fa.scanner.Category
mod.CATEGORIES = {
   ALL = "all",
   -- Places the item in hand can be built, such as the shore for an offshore pump.
   BUILD_SPOTS = "build_spots",
   PINS = "pins",
   -- Map tags.
   TAGS = "tags",
   RESOURCES = "resources",
   ENEMIES = "enemies",
   LOGISTICSAndPower = "logistics_and_power",
   PRODUCTION = "production",
   VEHICLES = "vehicles",
   SPIDERTRONS = "spidertrons",
   TRAINS = "trains",
   GHOSTS = "ghosts",
   PLAYERS = "players", -- actually character.
   OTHER = "other",
   MILITARY = "military",
   REMNANTS = "remnants",
   CONTAINERS = "containers",
   CORPSES = "corpses",
   TERRAIN = "terrain",
}

-- With an offshore pump in hand, shore within this many tiles of the player is checked for places to
-- build it.
mod.PUMP_SPOT_DISTANCE = 30

-- Modded water is mostly not a thing. If it is we can extend the list.
mod.WATER_PROTOS = Consts.WATER_TILE_NAMES

-- No clue if modded icebergs is a thing. If it is we can extend the list.
mod.ICEBERG_PROTOS =
   { "brash-ice", "ice-rough", "ice-smooth", "snow-crests", "snow-flat", "snow-lumpy", "snow-patchy", "ice-platform" }

return mod
