-- Chapter 1, getting started: how the tutorial talks, the cursor, coordinates and distances.
--
-- The map is the campaign's base map (helper-scripts/campaign-base-map): grass and sand, 400 by 400,
-- a pond of water from 16, -14 to 23, -9. This level adds a wooden chest.

---@diagnostic disable-next-line: unresolved-require
require("story")

local CHEST = { x = -10, y = 12 }
local MOVES_NEEDED = 6

-- FA's keys the level listens to. Every one carries the cursor's position.
local MOVE_KEYS = { ["fa-w"] = true, ["fa-a"] = true, ["fa-s"] = true, ["fa-d"] = true }
local SKIP_KEYS = { ["fa-s-w"] = true, ["fa-s-a"] = true, ["fa-s-s"] = true, ["fa-s-d"] = true }
local KEYS = { "fa-c-tab", "fa-cs-tab", "fa-k", "fa-j", "fa-s-k", "fa-a-k" }
for name in pairs(MOVE_KEYS) do
   KEYS[#KEYS + 1] = name
end
for name in pairs(SKIP_KEYS) do
   KEYS[#KEYS + 1] = name
end

---The key when `event` is one of FA's keys, else nil (a tick).
---@param event EventData
---@return EventData.CustomInputEvent?
local function key_event(event)
   if event.name == defines.events.on_tick then return nil end
   return event --[[@as EventData.CustomInputEvent]]
end

---@param event EventData
---@return string?
local function key(event)
   local input = key_event(event)
   return input and input.input_name
end

---@param event EventData
---@param name string
---@return boolean
local function pressed(event, name)
   return key(event) == name
end

---The tile under the cursor when `event` is one of FA's keys, else nil.
---@param event EventData
---@return fa.Tile?
local function cursor_tile(event)
   local input = key_event(event)
   if not input then return nil end
   local position = input.cursor_position --[[@as MapPosition.struct]]
   return { x = math.floor(position.x), y = math.floor(position.y) }
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
      condition = story_elapsed_check(1),
      action = function()
         hint({ "fa.campaign-01-" .. name })
      end,
   }
end

---@type fa.campaign.StoryNode[][]
local story_table = {
   {
      {
         init = function()
            local surface = game.surfaces.nauvis
            surface.always_day = true
            surface.create_entity({
               name = "wooden-chest",
               position = { CHEST.x + 0.5, CHEST.y + 0.5 },
               force = "player",
            })
         end,
         condition = story_elapsed_check(1),
      },
      hint_node("welcome"),
      hint_node("windows"),
      hint_node("cursor"),
      -- Done once a part was opened with Ctrl+Tab and the cursor moves again, back in the game. Keys
      -- inside a part (Tab, Escape) are the DLL's and never reach the level. No window opens over the
      -- status, since Tab there would move in the status rather than close the window.
      {
         init = function()
            storage.parts_opened = false
            set_goal({ "fa.campaign-01-goal-status" })
         end,
         update = function(event)
            if pressed(event, "fa-c-tab") or pressed(event, "fa-cs-tab") then storage.parts_opened = true end
         end,
         condition = function(event)
            return storage.parts_opened and MOVE_KEYS[key(event) or ""] ~= nil
         end,
      },
      {
         init = function()
            storage.moves = 0
            set_goal({ "fa.campaign-01-goal-move" })
         end,
         update = function(event)
            if MOVE_KEYS[key(event) or ""] then storage.moves = storage.moves + 1 end
         end,
         condition = function()
            return storage.moves >= MOVES_NEEDED
         end,
      },
      {
         init = function()
            set_goal({ "fa.campaign-01-goal-pond" })
         end,
         condition = function(event)
            local name = key(event) or ""
            if not MOVE_KEYS[name] and not SKIP_KEYS[name] then return false end
            local tile = assert(cursor_tile(event))
            return game.surfaces.nauvis.get_tile(tile.x, tile.y).name == "water"
         end,
         action = function()
            set_goal("")
         end,
      },
      hint_node("coordinates"),
      {
         init = function()
            set_goal({ "fa.campaign-01-goal-coordinates" })
         end,
         condition = function(event)
            return pressed(event, "fa-k")
         end,
      },
      {
         init = function()
            set_goal({ "fa.campaign-01-goal-back" })
         end,
         condition = function(event)
            return pressed(event, "fa-j")
         end,
         action = function()
            set_goal("")
         end,
      },
      {
         init = function()
            set_goal({ "fa.campaign-01-goal-chest", CHEST.x, CHEST.y })
         end,
         condition = function(event)
            local name = key(event) or ""
            if not MOVE_KEYS[name] and not SKIP_KEYS[name] then return false end
            local tile = assert(cursor_tile(event))
            return tile.x == CHEST.x and tile.y == CHEST.y
         end,
         action = function()
            set_goal("")
         end,
      },
      hint_node("distance"),
      {
         init = function()
            set_goal({ "fa.campaign-01-goal-distance" })
         end,
         condition = function(event)
            return pressed(event, "fa-s-k") or pressed(event, "fa-a-k")
         end,
         action = function()
            set_goal("")
         end,
      },
      hint_node("factorio"),
      hint_node("organized"),
      hint_node("saving"),
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
   if not game.tick_paused then story_update(storage.story, event) end
end)

script.on_event(KEYS, function(event)
   story_update(storage.story, event)
end)
