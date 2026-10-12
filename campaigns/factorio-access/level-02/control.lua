-- Chapter 2, walking: the character as the body, walking with the arrow keys, the anchored cursor and
-- Kruise Kontrol.
--
-- The map is the campaign's base map (helper-scripts/campaign-base-map): grass and sand, 400 by 400,
-- a pond of water from 16, -14 to 23, -9. This level adds a wooden chest west of the start and one
-- east of the pond, so that the pond stands between the pond's west side and the second chest.

---@diagnostic disable-next-line: unresolved-require
require("story")

local WEST_CHEST = { x = -14, y = 0 }
local POND_CHEST = { x = 27, y = -12 }
-- The pond's water, as an area of the map.
local POND = { left = 16, top = -14, right = 24, bottom = -8 }
-- How near the character must come to a chest, from the chest's centre.
local ARRIVED = 2.5
-- How far away the cancelled Kruise Kontrol walk must go.
local FAR_WALK = 10

-- Kruise Kontrol is an optional dependency. Its key and Enter, which cancels it, carry the cursor's
-- position like FA's own keys.
local KK_KEY = "klient-alt-move-to"
local KK_CANCEL_KEY = "fa-kk-cancel"
local KEYS = { "fa-i", KK_CANCEL_KEY }
---@type LuaCustomInputPrototype?
local kk_input = prototypes.custom_input[KK_KEY]
if kk_input then KEYS[#KEYS + 1] = KK_KEY end

---@return boolean
local function kk_installed()
   return script.active_mods["Kruise_Kontrol"] ~= nil
end

---The key when `event` is one of the keys above, else nil (a tick or a step).
---@param event EventData
---@return string?
local function key(event)
   return (event --[[@as EventData.CustomInputEvent]]).input_name
end

---@param event EventData
---@param name string
---@return boolean
local function pressed(event, name)
   return key(event) == name
end

---@return MapPosition.struct
local function character_position()
   local character = assert(assert(game.get_player(1)).character)
   return character.position --[[@as MapPosition.struct]]
end

---@param a fa.Point
---@param b fa.Point
---@return number
local function distance(a, b)
   return math.sqrt((a.x - b.x) ^ 2 + (a.y - b.y) ^ 2)
end

---@param tile fa.Tile
---@return fa.Point
local function centre(tile)
   return { x = tile.x + 0.5, y = tile.y + 0.5 }
end

---@param tile fa.Tile
---@return boolean
local function next_to(tile)
   return distance(character_position(), centre(tile)) <= ARRIVED
end

---How far the character is from the pond's water.
---@return number
local function pond_distance()
   local position = character_position()
   local dx = math.max(POND.left - position.x, 0, position.x - POND.right)
   local dy = math.max(POND.top - position.y, 0, position.y - POND.bottom)
   return math.sqrt(dx * dx + dy * dy)
end

---@param text LocalisedString
local function hint(text)
   story_show_message_dialog({ text = text })
end

---A node that shows a hint once the last one is closed.
---@param name string
---@return fa.campaign.StoryNode
local function hint_node(name)
   return {
      name = name,
      condition = story_elapsed_check(1),
      action = function()
         hint({ "fa.campaign-02-" .. name })
      end,
   }
end

---@param tile fa.Tile
local function create_chest(tile)
   game.surfaces.nauvis.create_entity({
      name = "wooden-chest",
      position = centre(tile),
      force = "player",
   })
end

---@type fa.campaign.StoryNode[][]
local story_table = {
   {
      {
         init = function()
            game.surfaces.nauvis.always_day = true
            create_chest(WEST_CHEST)
            create_chest(POND_CHEST)
            storage.anchored = false
         end,
         condition = story_elapsed_check(1),
      },
      hint_node("welcome"),
      hint_node("body"),
      hint_node("arrows"),
      {
         init = function()
            set_goal({ "fa.campaign-02-goal-west-chest", WEST_CHEST.x, WEST_CHEST.y })
         end,
         condition = function(event)
            return event.name == defines.events.on_player_changed_position and next_to(WEST_CHEST)
         end,
         action = function()
            set_goal("")
         end,
      },
      hint_node("anchor"),
      -- The level counts the presses of I from the start, unanchored, since FA's cursor mode is not
      -- the level's to read.
      {
         init = function()
            set_goal({ "fa.campaign-02-goal-anchor" })
         end,
         condition = function(event)
            return storage.anchored
               and event.name == defines.events.on_player_changed_position
               and pond_distance() <= 1.5
         end,
      },
      {
         init = function()
            set_goal({ "fa.campaign-02-goal-unanchor" })
         end,
         condition = function(event)
            return pressed(event, "fa-i") and not storage.anchored
         end,
         action = function()
            set_goal("")
         end,
      },
      {
         condition = story_elapsed_check(1),
         action = function(event, story)
            if kk_installed() then
               hint({ "fa.campaign-02-kk" })
            else
               hint({ "fa.campaign-02-kk-missing" })
               story_jump_to(story, "edge")
               -- The jump keeps the old start, so the next hint would follow this one at once.
               story.current_story_started_at = event.tick
            end
         end,
      },
      hint_node("kk-more"),
      {
         init = function()
            storage.kk_started = false
            set_goal({ "fa.campaign-02-goal-kk", POND_CHEST.x, POND_CHEST.y })
         end,
         update = function(event)
            if pressed(event, KK_KEY) then storage.kk_started = true end
         end,
         condition = function(event)
            return storage.kk_started
               and event.name == defines.events.on_player_changed_position
               and next_to(POND_CHEST)
         end,
      },
      -- Enter counts once a walk of at least FAR_WALK tiles is on its way and not yet there.
      {
         init = function()
            storage.kk_target = nil
            set_goal({ "fa.campaign-02-goal-cancel", FAR_WALK })
         end,
         update = function(event)
            if not pressed(event, KK_KEY) then return end
            local target = (event --[[@as EventData.CustomInputEvent]]).cursor_position --[[@as MapPosition.struct]]
            if distance(character_position(), target) >= FAR_WALK then
               storage.kk_target = { x = target.x, y = target.y }
            else
               storage.kk_target = nil
            end
         end,
         condition = function(event)
            local target = storage.kk_target --[[@as fa.Point?]]
            return pressed(event, KK_CANCEL_KEY) and target ~= nil and distance(character_position(), target) > ARRIVED
         end,
         action = function()
            set_goal("")
         end,
      },
      hint_node("edge"),
      hint_node("end"),
      {
         condition = story_elapsed_check(1),
      },
   },
}

story_init_helpers(story_table)

script.on_init(function()
   storage.story = story_init()
end)

script.on_event(defines.events.on_tick, function(event)
   if not game.tick_paused then story_update(storage.story, event, "level-03") end
end)

script.on_event(defines.events.on_player_changed_position, function(event)
   story_update(storage.story, event, "level-03")
end)

script.on_event(KEYS, function(event)
   if pressed(event, "fa-i") then storage.anchored = not storage.anchored end
   story_update(storage.story, event, "level-03")
end)
