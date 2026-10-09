-- Prototype lister UI
-- Lists all prototypes (items, fluids, virtual signals, entities, recipes, etc.)
-- Left click: Speak prototype name without modification
-- Right click: Spell out name with spaces between characters

local TreeChooser = require("scripts.ui.tree-chooser")
local KeyGraph = require("scripts.ui.key-graph")
local TabList = require("scripts.ui.tab-list")
local UiRouter = require("scripts.ui.router")
local SignalHelpers = require("scripts.ui.signal-helpers")
local Localising = require("scripts.localising")
local Speech = require("scripts.speech")

local mod = {}

---Convert a prototype name to spaced-out format for speaking
---@param name string
---@return string
local function name_to_spaced(name)
   local chars = {}
   for char in name:gmatch(".") do
      table.insert(chars, char)
   end
   return table.concat(chars, " ")
end

---Build vtable for prototype lister nodes
---@param name string Prototype name
---@param proto any Prototype object
---@return fa.ui.graph.NodeVtable
local function prototype_lister_vtable_builder(name, proto)
   return {
      label = function(ctx)
         ctx.message:fragment(Localising.get_localised_name_with_fallback(proto))
      end,
      on_click = function(click_ctx)
         Speech.speak(click_ctx.pindex, name)
      end,
      on_right_click = function(click_ctx)
         Speech.speak(click_ctx.pindex, name_to_spaced(name))
      end,
   }
end

---@param ctx fa.ui.graph.Ctx
local function build_prototype_tree(ctx)
   local builder = TreeChooser.TreeChooserBuilder.new()

   -- Prototype type categories with their signal-helpers add functions
   local proto_types = {
      {
         key = "type-item",
         label = { "fa.signal-type-item" },
         add_func = function()
            SignalHelpers.add_item_signals(builder, "type-item", false, nil, nil, nil, prototype_lister_vtable_builder)
         end,
      },
      {
         key = "type-fluid",
         label = { "fa.signal-type-fluid" },
         add_func = function()
            SignalHelpers.add_fluid_signals(builder, "type-fluid", prototype_lister_vtable_builder)
         end,
      },
      {
         key = "type-virtual",
         label = { "fa.signal-type-virtual" },
         add_func = function()
            SignalHelpers.add_virtual_signals(builder, "type-virtual", prototype_lister_vtable_builder)
         end,
      },
      {
         key = "type-entity",
         label = { "fa.signal-type-entity" },
         add_func = function()
            SignalHelpers.add_entity_signals(builder, "type-entity", prototype_lister_vtable_builder)
         end,
      },
      {
         key = "type-recipe",
         label = { "fa.signal-type-recipe" },
         add_func = function()
            SignalHelpers.add_recipe_signals(builder, "type-recipe", prototype_lister_vtable_builder)
         end,
      },
      {
         key = "type-space-location",
         label = { "fa.signal-type-space-location" },
         feature_flag = "space_travel",
         add_func = function()
            SignalHelpers.add_space_location_signals(builder, "type-space-location", prototype_lister_vtable_builder)
         end,
      },
      {
         key = "type-asteroid-chunk",
         label = { "fa.signal-type-asteroid-chunk" },
         feature_flag = "space_travel",
         add_func = function()
            SignalHelpers.add_asteroid_chunk_signals(builder, "type-asteroid-chunk", prototype_lister_vtable_builder)
         end,
      },
      {
         key = "type-quality",
         label = { "fa.signal-type-quality" },
         feature_flag = "quality",
         add_func = function()
            SignalHelpers.add_quality_signals(builder, "type-quality", prototype_lister_vtable_builder)
         end,
      },
   }

   -- Add top-level prototype type categories
   for _, proto_type in ipairs(proto_types) do
      if proto_type.feature_flag and not script.feature_flags[proto_type.feature_flag] then goto continue end

      builder:add_node(proto_type.key, TreeChooser.ROOT, {
         label = function(label_ctx)
            label_ctx.message:fragment(proto_type.label)
         end,
      })

      proto_type.add_func()

      ::continue::
   end

   return builder:build()
end

local prototype_lister_tab = KeyGraph.declare_graph({
   name = "prototype_lister",
   render_callback = build_prototype_tree,
   title = { "fa.prototype-lister-title" },
})

mod.prototype_lister_menu = TabList.declare_tablist({
   ui_name = UiRouter.UI_NAMES.PROTOTYPE_LISTER,
   resets_to_first_tab_on_open = true,
   tabs_callback = function()
      return {
         {
            name = "prototype_lister",
            tabs = { prototype_lister_tab },
         },
      }
   end,
})

UiRouter.register_ui(mod.prototype_lister_menu)

return mod
