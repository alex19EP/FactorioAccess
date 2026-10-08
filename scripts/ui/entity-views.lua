--[[
FA's own views of an entity, attached to the game's window for it: what the window does not show,
such as what a belt carries or where a pipe connects. Each view is a frame to the right of the
window (a relative GUI element), captioned with the view's title, holding its columns side by side
in a table under a header row of their titles. The native screen for the window reads each frame as
a stop after the window's own (native/src/screens/EntityWindowScreen.hpp).

Every player gets the views, running the DLL or not, so every peer changes the game's GUI the same
way. They are built as the window opens and destroyed as it closes; reopening it takes them again.
]]
local EventManager = require("scripts.event-manager")

local mod = {}

---@class fa.EntityViews.Column
---@field title LocalisedString? the column's header; columns without one get an empty header cell
---@field cells LocalisedString[]

---@class fa.EntityViews.View
---@field title LocalisedString
---@field columns fa.EntityViews.Column[]

---@class fa.EntityViews.Source
---@field gui defines.relative_gui_type the game's window for the entities
---@field views fun(entity: LuaEntity): fa.EntityViews.View[]

---@type table<string, fa.EntityViews.Source>
local sources = {}

local NAME_PREFIX = "fa-entity-view-"

-- Registers what to show beside the windows of entities of these types.
---@param types string[]
---@param gui defines.relative_gui_type
---@param views fun(entity: LuaEntity): fa.EntityViews.View[]
function mod.register(types, gui, views)
   for _, t in ipairs(types) do
      assert(not sources[t], "entity views for " .. t .. " are registered twice")
      sources[t] = { gui = gui, views = views }
   end
end

---@param player LuaPlayer
function mod.destroy(player)
   for _, element in ipairs(player.gui.relative.children) do
      if string.sub(element.name, 1, #NAME_PREFIX) == NAME_PREFIX then element.destroy() end
   end
end

---@param player LuaPlayer
---@param gui defines.relative_gui_type
---@param index integer
---@param view fa.EntityViews.View
local function add_view(player, gui, index, view)
   local frame = player.gui.relative.add({
      type = "frame",
      name = NAME_PREFIX .. index,
      caption = view.title,
      direction = "vertical",
      anchor = { gui = gui, position = defines.relative_gui_position.right },
   })
   local columns = {}
   local rows = 0
   local titled = false
   for _, column in ipairs(view.columns) do
      if column.cells[1] then
         table.insert(columns, column)
         rows = math.max(rows, #column.cells)
         titled = titled or column.title ~= nil
      end
   end
   local grid = frame.add({ type = "table", column_count = #columns })
   if titled then
      for _, column in ipairs(columns) do
         grid.add({ type = "label", caption = column.title or "" })
      end
   end
   for row = 1, rows do
      for _, column in ipairs(columns) do
         grid.add({ type = "label", caption = column.cells[row] or "" })
      end
   end
end

-- Replaces the player's views with those of the entity whose window just opened, if FA has any.
---@param player LuaPlayer
---@param entity LuaEntity
function mod.show(player, entity)
   mod.destroy(player)
   local source = sources[entity.type]
   if not source then return end
   for i, view in ipairs(source.views(entity)) do
      add_view(player, source.gui, i, view)
   end
end

EventManager.on_event(
   defines.events.on_gui_opened,
   ---@param event EventData.on_gui_opened
   ---@param pindex integer
   function(event, pindex)
      if event.gui_type == defines.gui_type.entity then mod.show(game.get_player(pindex), event.entity) end
   end,
   EventManager.EVENT_KIND.UI
)

EventManager.on_event(
   defines.events.on_gui_closed,
   ---@param event EventData.on_gui_closed
   ---@param pindex integer
   function(event, pindex)
      if event.gui_type == defines.gui_type.entity then mod.destroy(game.get_player(pindex)) end
   end,
   EventManager.EVENT_KIND.UI
)

return mod
