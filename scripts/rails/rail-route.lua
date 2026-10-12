--The rail planner's route as the coordinates key reads it while planning: the runs of straight rail
--with their direction and length, the curves between them and which way they turn, and the ramps.
local FaUtils = require("scripts.fa-utils")
local MessageBuilder = require("scripts.speech").MessageBuilder

local mod = {}

---@alias fa.RailRouteKind "straight"|"curve"|"ramp"

---@param piece fa.NativeRailPiece
---@return fa.RailRouteKind
local function kind_of(piece)
   if piece.type == "rail-ramp" then return "ramp" end
   if string.find(piece.type, "curved", 1, true) then return "curve" end
   return "straight"
end

---@param from fa.NativeRailPoint
---@param to fa.NativeRailPoint
---@return integer
local function tiles_between(from, to)
   local dx, dy = to.x - from.x, to.y - from.y
   return math.floor(math.sqrt(dx * dx + dy * dy) + 0.5)
end

---Whether going from one direction to the other turns right (clockwise).
---@param from integer
---@param to integer
---@return boolean
local function turns_right(from, to)
   return (to - from) % 16 < 8
end

---@class fa.RailRouteRun
---@field kind fa.RailRouteKind
---@field from fa.NativeRailPoint Where the run starts
---@field to fa.NativeRailPoint Where it ends
---@field right boolean A curve: whether it turns right

---Groups the pieces into runs: straight rails going one way, curves turning one way, a ramp.
---@param route fa.NativeRailRoute
---@return fa.RailRouteRun[]
local function runs_of(route)
   ---@type fa.RailRouteRun[]
   local runs = {}
   local at = route.start
   for _, piece in ipairs(route.pieces) do
      local kind = kind_of(piece)
      local right = turns_right(at.direction, piece.direction)
      local last = runs[#runs]
      local continues = last
         and last.kind == kind
         and kind ~= "ramp"
         and (kind ~= "straight" or last.to.direction == piece.direction)
         and (kind ~= "curve" or last.right == right)
      if continues then
         last.to = piece
      else
         table.insert(runs, { kind = kind, from = at, to = piece, right = right })
      end
      at = piece
   end
   return runs
end

---What the route is, run by run; "no route" when the game found none.
---@param route fa.NativeRailRoute
---@return LocalisedString
function mod.describe(route)
   if not next(route.pieces) then return { "fa.native-rail-plan-no-route" } end
   local message = MessageBuilder.new()
   for _, run in ipairs(runs_of(route)) do
      local direction = FaUtils.localise_direction(run.to.direction --[[@as defines.direction]])
      local tiles = tiles_between(run.from, run.to)
      if run.kind == "curve" then
         message:list_item({ run.right and "fa.rail-route-curve-right" or "fa.rail-route-curve-left", direction })
      elseif run.kind == "ramp" then
         message:list_item({
            run.to.elevated and "fa.rail-route-ramp-up" or "fa.rail-route-ramp-down",
            direction,
            tiles,
         })
      else
         message:list_item({ "fa.rail-route-straight", direction, tiles })
      end
   end
   return message:build()
end

return mod
