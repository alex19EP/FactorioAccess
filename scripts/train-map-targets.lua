--[[
Train map targets: where a click on the map would send a train, said after what a map cell holds.

On the map, with a locomotive's window open or riding a train with no window open, the game draws
what its two clicks would add to the train's schedule (ChartRenderer::checkDrawLocomotivePaths):
holding Ctrl, a circle on the rail nearest the cursor within 20 tiles, where a temporary stop would
go, and holding Shift, one on the train stop nearest the cursor within 20 tiles. Each gets a preview
path when the train can reach it. The cell's reading ends with both, as the game picks them
(Locomotive::getRailTarget and getStationTarget: the nearest by position in a square 20 tiles each
way) and checks them (TrainGui::checkAddTemporaryTrainStation: either end of the rail, from either
end of the train).
]]
local mod = {}

local RANGE = 20

local RAIL_TYPES = {
   "straight-rail",
   "half-diagonal-rail",
   "curved-rail-a",
   "curved-rail-b",
   "elevated-straight-rail",
   "elevated-half-diagonal-rail",
   "elevated-curved-rail-a",
   "elevated-curved-rail-b",
   "rail-ramp",
   "legacy-straight-rail",
   "legacy-curved-rail",
}

---The train a click on the map adds a stop to, as getVehicleLocomotiveForAddTrainStation finds it:
---the open locomotive's, or with no window open, the one the player rides with a locomotive in it.
---@param player LuaPlayer
---@return LuaTrain?
local function clicked_train(player)
   local opened = player.opened
   if opened then
      if opened.object_name == "LuaEntity" and opened.type == "locomotive" then return opened.train end
      return nil
   end
   local vehicle = player.physical_vehicle
   if not vehicle or not vehicle.train then return nil end
   local train = vehicle.train
   if not next(train.locomotives.front_movers) and not next(train.locomotives.back_movers) then return nil end
   return train
end

---@param surface LuaSurface
---@param position MapPosition.struct
---@param filter EntitySearchFilters
---@return LuaEntity?
local function nearest(surface, position, filter)
   filter.area = {
      { x = position.x - RANGE, y = position.y - RANGE },
      { x = position.x + RANGE, y = position.y + RANGE },
   }
   local best, best_distance = nil, math.huge
   for _, entity in pairs(surface.find_entities_filtered(filter)) do
      local at = entity.position --[[@as MapPosition.struct]]
      local distance = (at.x - position.x) ^ 2 + (at.y - position.y) ^ 2
      if distance < best_distance then
         best, best_distance = entity, distance
      end
   end
   return best
end

---@param train LuaTrain
---@param goals TrainPathFinderGoal[]
---@return boolean
local function reachable(train, goals)
   local result = game.train_manager.request_train_path({
      type = "any-goal-accessible",
      train = train,
      goals = goals,
   }) --[[@as TrainPathFinderOneGoalResult]]
   return result.found_path
end

---Adds what Ctrl+click and Shift+click on the map at `position` would add to the schedule, when the
---game would add one there.
---@param message fa.MessageBuilder
---@param player LuaPlayer
---@param position MapPosition.struct
function mod.describe(message, player, position)
   local train = clicked_train(player)
   if not train then return end
   local surface = player.surface

   local rail = nearest(surface, position, { type = RAIL_TYPES })
   if rail then
      local goals = {
         { rail = rail, direction = defines.rail_direction.front },
         { rail = rail, direction = defines.rail_direction.back },
      }
      message:list_item({ reachable(train, goals) and "fa.map-train-rail-reachable" or "fa.map-train-rail-no-path" })
   end

   local stop = nearest(surface, position, { type = "train-stop" })
   if stop then
      local path = stop.connected_rail and reachable(train, { { train_stop = stop } })
      local name = assert(stop.backer_name)
      message:list_item({ path and "fa.map-train-stop-reachable" or "fa.map-train-stop-no-path", name })
   end
end

return mod
