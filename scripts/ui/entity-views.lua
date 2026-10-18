--[[
FA's own views of an entity, attached to the game's window for it: what the window does not show,
such as what a belt carries or where a pipe connects. Each view is a frame to the right of the
window (a relative GUI element), captioned with the view's title, holding its columns side by side,
each a vertical flow headed by its title. The native screen for the window reads each frame as a
stop after the window's own (native/src/screens/EntityWindowScreen.hpp), and columns of text as
columns: Up and Down walk one, Left and Right go to the same line of the next.

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
---@param column LuaGuiElement
---@param caption LocalisedString
---@param width integer
---@param style string?
local function add_cell(column, caption, width, style)
   local label = column.add({ type = "label", caption = caption, style = style })
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
   local row = inside.add({ type = "flow", direction = "horizontal" })
   row.style.horizontal_spacing = 16
   for _, column in ipairs(view.columns) do
      if #column.cells > 0 then
         local flow = row.add({ type = "flow", direction = "vertical" })
         flow.style.vertical_spacing = 6
         if column.title then add_cell(flow, column.title, source.column_width, "caption_label") end
         for _, cell in ipairs(column.cells) do
            add_cell(flow, cell, source.column_width)
         end
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
   function(event)
      if event.gui_type == defines.gui_type.entity then
         mod.show(assert(game.get_player(event.player_index)), assert(event.entity))
      end
   end,
   EventManager.EVENT_KIND.UI
)

EventManager.on_event(
   defines.events.on_gui_closed,
   ---@param event EventData.on_gui_closed
   function(event)
      if event.gui_type == defines.gui_type.entity then mod.destroy(assert(game.get_player(event.player_index))) end
   end,
   EventManager.EVENT_KIND.UI
)

return mod
