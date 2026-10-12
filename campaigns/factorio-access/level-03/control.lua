-- Chapter 3, the scanner: categories, subcategories and entries, Home, remote view, coordinates, and
-- Kruise Kontrol sent to what the scanner found.
--
-- The map is the campaign's base map (helper-scripts/campaign-base-map): grass and sand, 400 by 400,
-- a pond of water from 16, -14 to 23, -9. This level adds rocks, a small forest, a patch each of
-- copper, coal and stone, an iron patch near the start and one far to the east, off the screen, and a
-- chest for the coordinates hint.

---@diagnostic disable-next-line: unresolved-require
require("story")

---@type fa.campaign.Patch[]
local PATCHES = {
   { name = "copper-ore", left = -24, top = 4, width = 5, height = 5 },
   { name = "iron-ore", left = 26, top = 3, width = 5, height = 5 },
   { name = "coal", left = 4, top = 11, width = 5, height = 4 },
   { name = "stone", left = -8, top = 10, width = 4, height = 4 },
   { name = "iron-ore", left = 90, top = -3, width = 6, height = 6 },
}
local COAL = PATCHES[3]
-- Iron east of this is the far patch.
local FAR_X = 60
local ORE_AMOUNT = 600

local ROCKS = {
   { name = "big-rock", x = -6, y = -7 },
   { name = "big-rock", x = 8, y = 8 },
   { name = "huge-rock", x = -14, y = -3 },
}
local TREES = {
   { -26, -13 },
   { -24, -14 },
   { -22, -12 },
   { -25, -11 },
   { -23, -10 },
   { -21, -13 },
   { -27, -10 },
   { -22, -9 },
}
local CHEST = { x = -12, y = -13 }

-- Kruise Kontrol is an optional dependency. Home, its key and Enter, which cancels it, carry the
-- cursor's position like FA's own keys.
local KK_KEY = "klient-alt-move-to"
local KK_CANCEL_KEY = "fa-kk-cancel"
local KEYS = { "fa-home", KK_CANCEL_KEY }
---@type LuaCustomInputPrototype?
local kk_input = prototypes.custom_input[KK_KEY]
if kk_input then KEYS[#KEYS + 1] = KK_KEY end

---@return boolean
local function kk_installed()
   return script.active_mods["Kruise_Kontrol"] ~= nil
end

---The key when `event` is one of the keys above, else nil.
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

---@return LuaPlayer
local function player()
   return assert(game.get_player(1))
end

---When `event` is Home: where it put the cursor and whether an entity matching `filter` stands there.
---@param event EventData
---@param filter EntitySearchFilters
---@return MapPosition.struct?
local function home_on(event, filter)
   if not pressed(event, "fa-home") then return nil end
   local position = (event --[[@as EventData.CustomInputEvent]]).cursor_position --[[@as MapPosition.struct]]
   filter.position = position
   filter.radius = 1
   if game.surfaces.nauvis.count_entities_filtered(filter) == 0 then return nil end
   return position
end

---How far the character is from a patch.
---@param patch fa.campaign.Patch
---@return number
local function patch_distance(patch)
   local position = assert(player().character).position --[[@as MapPosition.struct]]
   local dx = math.max(patch.left - position.x, 0, position.x - (patch.left + patch.width))
   local dy = math.max(patch.top - position.y, 0, position.y - (patch.top + patch.height))
   return math.sqrt(dx * dx + dy * dy)
end

---@param text LocalisedString
local function hint(text)
   story_show_message_dialog({ text = text })
end

---A node that shows a hint once the last one is closed.
---@param name string
---@param ... any the hint's parameters
---@return fa.campaign.StoryNode
local function hint_node(name, ...)
   local text = { "fa.campaign-03-" .. name, ... }
   return {
      name = name,
      condition = story_elapsed_check(1),
      action = function()
         hint(text)
      end,
   }
end

---A goal done when Home puts the cursor on an entity matching `filter`, and `check` agrees with
---where.
---@param goal LocalisedString
---@param filter fun(): EntitySearchFilters
---@param check (fun(position: MapPosition.struct): boolean)?
---@return fa.campaign.StoryNode
local function home_goal(goal, filter, check)
   return {
      init = function()
         set_goal(goal)
      end,
      condition = function(event)
         local position = home_on(event, filter())
         return position ~= nil and (check == nil or check(position))
      end,
      action = function()
         set_goal("")
      end,
   }
end

local function build_map()
   local surface = game.surfaces.nauvis
   surface.always_day = true
   for _, patch in ipairs(PATCHES) do
      for x = patch.left, patch.left + patch.width - 1 do
         for y = patch.top, patch.top + patch.height - 1 do
            surface.create_entity({ name = patch.name, position = { x + 0.5, y + 0.5 }, amount = ORE_AMOUNT })
         end
      end
   end
   for _, rock in ipairs(ROCKS) do
      surface.create_entity({ name = rock.name, position = { rock.x + 0.5, rock.y + 0.5 } })
   end
   for i, tree in ipairs(TREES) do
      surface.create_entity({ name = string.format("tree-0%d", (i - 1) % 9 + 1), position = { tree[1], tree[2] } })
   end
   surface.create_entity({ name = "wooden-chest", position = { CHEST.x + 0.5, CHEST.y + 0.5 }, force = "player" })
   game.forces.player.chart(surface, { { -200, -200 }, { 200, 200 } })
end

---@type fa.campaign.StoryNode[][]
local story_table = {
   {
      {
         init = build_map,
         condition = story_elapsed_check(1),
      },
      hint_node("welcome"),
      hint_node("levels"),
      hint_node("keys"),
      home_goal({ "fa.campaign-03-goal-rock" }, function()
         return { type = "simple-entity" }
      end),
      home_goal({ "fa.campaign-03-goal-copper" }, function()
         return { name = "copper-ore" }
      end),
      hint_node("entries"),
      home_goal({ "fa.campaign-03-goal-far-iron" }, function()
         return { name = "iron-ore" }
      end, function(position)
         return position.x >= FAR_X
      end),
      hint_node("remote"),
      {
         init = function()
            set_goal({ "fa.campaign-03-goal-return" })
         end,
         condition = function()
            return player().controller_type == defines.controllers.character
         end,
         action = function()
            set_goal("")
         end,
      },
      hint_node("coordinates", CHEST.x, CHEST.y),
      {
         condition = story_elapsed_check(1),
         action = function(event, story)
            if kk_installed() then return end
            hint({ "fa.campaign-03-kk-missing" })
            story_jump_to(story, "directional")
            -- The jump keeps the old start, so the next hint would follow this one at once.
            story.current_story_started_at = event.tick
         end,
      },
      {
         init = function()
            storage.kk_started = false
            set_goal({ "fa.campaign-03-goal-coal" })
         end,
         update = function(event)
            if pressed(event, KK_KEY) then storage.kk_started = true end
         end,
         condition = function(event)
            return pressed(event, KK_CANCEL_KEY)
               and storage.kk_started
               and patch_distance(COAL) <= 4
               and player().get_item_count("coal") > 0
         end,
         action = function()
            set_goal("")
         end,
      },
      hint_node("mined"),
      -- Without Kruise Kontrol the story jumps past this node, and a jump calls the action of the
      -- node before its target, so this one has none.
      {
         condition = story_elapsed_check(1),
      },
      hint_node("directional"),
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
   if not game.tick_paused then story_update(storage.story, event, "level-04") end
end)

script.on_event(KEYS, function(event)
   story_update(storage.story, event, "level-04")
end)
