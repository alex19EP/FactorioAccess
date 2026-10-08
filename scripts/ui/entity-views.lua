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
---@field column_width integer

---@type table<string, fa.EntityViews.Source>
local sources = {}

local NAME_PREFIX = "fa-entity-view-"

-- How wide a column's text runs before it wraps, in GUI pixels, unless a registration says.
local COLUMN_WIDTH = 240

-- Registers what to show beside the windows of entities of these types. `column_width` narrows the
-- columns beside a window that leaves little of the screen free.
---@param types string[]
---@param gui defines.relative_gui_type
---@param views fun(entity: LuaEntity): fa.EntityViews.View[]
---@param column_width integer?
function mod.register(types, gui, views, column_width)
   for _, t in ipairs(types) do
      assert(not sources[t], "entity views for " .. t .. " are registered twice")
      sources[t] = { gui = gui, views = views, column_width = column_width or COLUMN_WIDTH }
   end
end

---@param player LuaPlayer
function mod.destroy(player)
   for _, element in ipairs(player.gui.relative.children) do
      if string.sub(element.name, 1, #NAME_PREFIX) == NAME_PREFIX then element.destroy() end
   end
end

-- A cell's label, wrapping its text at the column's width: a line that runs on would leave the
-- screen beside a wide window.
---@param grid LuaGuiElement
---@param caption LocalisedString
---@param width integer
---@param style string?
local function add_cell(grid, caption, width, style)
   local label = grid.add({ type = "label", caption = caption, style = style })
   label.style.single_line = false
   label.style.maximal_width = width
end

---@param player LuaPlayer
---@param source fa.EntityViews.Source
---@param index integer
---@param view fa.EntityViews.View
local function add_view(player, source, index, view)
   local frame = player.gui.relative.add({
      type = "frame",
      name = NAME_PREFIX .. index,
      caption = view.title,
      direction = "vertical",
      anchor = { gui = source.gui, position = defines.relative_gui_position.right },
   })
   local inside = frame.add({ type = "frame", style = "inside_shallow_frame_with_padding", direction = "vertical" })
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
   local grid = inside.add({ type = "table", column_count = #columns })
   grid.style.horizontal_spacing = 16
   grid.style.vertical_spacing = 6
   if titled then
      for _, column in ipairs(columns) do
         add_cell(grid, column.title or "", source.column_width, "caption_label")
      end
   end
   for row = 1, rows do
      for _, column in ipairs(columns) do
         add_cell(grid, column.cells[row] or "", source.column_width)
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
      add_view(player, source, i, view)
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
