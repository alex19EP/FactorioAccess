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
