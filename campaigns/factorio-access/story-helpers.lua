-- What the tutorial's levels share: the player and what they carry and build, and the story nodes
-- that show a hint or set a goal. A level requires the game's story first, whose functions these
-- call, then this file as __FactorioAccess__/campaigns/factorio-access/story-helpers.

local Helpers = {}

---@return LuaPlayer
function Helpers.player()
   return assert(game.get_player(1))
end

---@param name string
---@return integer
function Helpers.carried(name)
   return Helpers.player().get_item_count(name)
end

---Whether a condition is worth checking: on every event but the tick, and on every half second.
---@param event EventData
---@return boolean
function Helpers.due(event)
   return event.name ~= defines.events.on_tick or event.tick % 30 == 0
end

---@param name string
---@return LuaEntity[]
function Helpers.built(name)
   return game.surfaces.nauvis.find_entities_filtered({ name = name, force = "player" })
end

---@param name string
---@return LuaEntity?
function Helpers.find(name)
   return Helpers.built(name)[1]
end

---@return boolean
function Helpers.no_window()
   return Helpers.player().opened_gui_type == defines.gui_type.none
end

---@param entity LuaEntity
---@return boolean
function Helpers.fuelled(entity)
   return entity.energy > 0 or not assert(entity.burner).inventory.is_empty()
end

---@param text LocalisedString
function Helpers.hint(text)
   story_show_message_dialog({ text = text })
end

---@class (exact) fa.campaign.GoalOptions
---@field params (any[]|fun(): any[])? the goal's parameters, or a function giving them anew, which updates the goal as they change
---@field needs string? what the goal works on; while none stands, the goal asks for it again
---@field variant (fun(): string?, any[]?)? another goal while something is wrong, named as the goals are, with its parameters
---@field start fun()? called as the goal appears
---@field after fun(event: EventData, story: table)? called as the goal is done

---@class (exact) fa.campaign.Chapter
---@field hint_node fun(name: string, ...: any): fa.campaign.StoryNode
---@field goal_node fun(name: string, done: (fun(event: EventData): boolean), options?: fa.campaign.GoalOptions): fa.campaign.StoryNode

---The nodes of chapter `number`, whose texts are fa.campaign-NN-name and fa.campaign-NN-goal-name.
---@param number integer
---@return fa.campaign.Chapter
function Helpers.chapter(number)
   local prefix = string.format("fa.campaign-%02d-", number)

   ---A node that shows a hint once the last one is closed.
   ---@param name string
   ---@param ... any the hint's parameters
   ---@return fa.campaign.StoryNode
   local function hint_node(name, ...)
      local text = { prefix .. name, ... }
      return {
         name = name,
         condition = story_elapsed_check(1),
         action = function()
            Helpers.hint(text)
         end,
      }
   end

   ---A goal, done when `done` says so.
   ---@param name string
   ---@param done fun(event: EventData): boolean
   ---@param options fa.campaign.GoalOptions?
   ---@return fa.campaign.StoryNode
   local function goal_node(name, done, options)
      options = options or {}
      local params = options.params
      ---@return LocalisedString
      local function text()
         local values = type(params) == "function" and params() or params or {}
         return { prefix .. "goal-" .. name, table.unpack(values) }
      end
      return {
         name = name,
         init = function()
            -- What the goal starts may be what it counts.
            if options.start then options.start() end
            storage.goal_variant = nil
            storage.goal_shown = serpent.line(text())
            set_goal(text())
         end,
         update = function(event)
            if not Helpers.due(event) then return end
            ---@type string?, any[]?
            local variant, values = nil, nil
            if options.needs and not Helpers.find(options.needs) then
               variant, values = "rebuild", { { "entity-name." .. options.needs } }
            elseif options.variant then
               variant, values = options.variant()
            end
            ---@type LocalisedString
            local goal = variant and { prefix .. "goal-" .. variant, table.unpack(values or {}) } or text()
            local shown = serpent.line(goal)
            if shown == storage.goal_shown then return end
            -- A count going up updates the goal in place; a new goal is announced as one.
            local same = variant == storage.goal_variant
            storage.goal_variant = variant
            storage.goal_shown = shown
            set_goal(goal, same)
         end,
         condition = function(event)
            return storage.goal_variant == nil and Helpers.due(event) and done(event)
         end,
         action = function(event, story)
            set_goal("")
            if options.after then options.after(event, story) end
         end,
      }
   end

   return { hint_node = hint_node, goal_node = goal_node }
end

return Helpers
