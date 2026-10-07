local Consts = require("scripts.consts")
local DataToRuntimeMap = require("scripts.data-to-runtime-map")

--[[
See https://forums.factorio.com/viewtopic.php?f=28&t=114820

We need resource_patch_search_radius to write the scanner algorithm, though
hopefully in future we can just ask the engine.  The problem of today is that we
don't have it at runtime.  We use ModData prototypes to pass this data from
data stage to runtime. Parsed back out in scripts.scanner.resource-patches.lua.

If nil we default to 3.
]]

local resource_search_radiuses = {}

for name, proto in pairs(data.raw["resource"]) do
   if proto.type == "resource" then resource_search_radiuses[name] = proto.resource_patch_search_radius or 3 end
end

DataToRuntimeMap.build(Consts.RESOURCE_SEARCH_RADIUSES_MAP_NAME, resource_search_radiuses)

--[[
Combinator bounding boxes for wire connections.

Combinators have separate input and output connection points, and the drag_wire API uses
position to determine which side to connect to. We need the bounding box data from prototypes
to calculate the correct positions for each side.
]]

local CombinatorBoundingBoxes = require("scripts.combinator-bounding-boxes")
CombinatorBoundingBoxes.build_map()

--[[
Combat data extraction.
Extracts targeting type, range limits, area damage info, and enemy stats from prototypes.
]]

local CombatData = require("scripts.combat.combat-data")
CombatData.build_map()
