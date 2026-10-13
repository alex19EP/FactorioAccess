--[[
The low-level entrypoint to the UI infrastructure for external code.

One does: `Router.get_router(pindex):method()` or alternatively caches that in a variable.  This module is the UI
entrypoint: you can request UIs to open and close, and ask what is open, etc.

Routers cannot be stored in storage.  It is cheap to get a router because the instances are cached; each
get_router(pindex) call returns the same object  for a given pindex for the run of this process.  Those objects are thin
wrappers over storage manipulation, and so this is still deterministic.

This is WIP in the sense that not all UIs are ported to the new system.  So, for now, it just holds and changes the UI's
name, and the routing is handled in control.lua. If you are familiar with the old way:

```
players[pindex].menu = bla
players[pindex].in_menu = true
```

Then, now, it's:

```
Router.get_router(pindex):open_ui(UiRouter.UI_NAMES.WHATEVER)
local is_open = Router.get_router(pindex):is_ui_open()
```

Internally, routers will pass themselves down to child UIs so that said UIs may manipulate the top-level UI state. While
getting off the ground however, everything imports this module and uses it as a raw replacement for
storage[].menu[_name].  Once we are further along, that will be inverted and this router will get a chance to intercept
UI-destined events.
]]
local StorageManager = require("scripts.storage-manager")
local EventManager = require("scripts.event-manager")
local GameGui = require("scripts.ui.game-gui")
local UiSounds = require("scripts.ui.sounds")
local Speech = require("scripts.speech")
local MessageBuilder = Speech.MessageBuilder
local RichText = require("scripts.rich-text")

local mod = {}

---Storage for registered UI instances
---@type table<string, fa.ui.UiPanelBase?>
local registered_uis = {}

---Register a UI with the event routing system
---@param ui fa.ui.UiPanelBase
function mod.register_ui(ui)
   assert(type(ui.ui_name) == "string")
   registered_uis[ui.ui_name] = ui
end

---Get a registered UI by name
---@param ui_name string
---@return fa.ui.UiPanelBase?
function mod.get_registered_ui(ui_name)
   return registered_uis[ui_name]
end

---Get all registered UIs
---@return table<string, fa.ui.UiPanelBase?>
function mod.get_registered_uis()
   return registered_uis
end

---@alias fa.ui.Modifiers { control: boolean?, alt: boolean?, shift: boolean? }

---@class fa.ui.UiPanelBase
---@field ui_name fa.ui.UiName
---@field open fun(self, pindex: integer, parameters: table, controller: fa.ui.RouterController)
---@field close? fun(self, pindex: integer)
---@field is_overlay? fun(self): boolean Return true if this UI is an overlay on the map
---@field on_click? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_right_click? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_up? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_down? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_left? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_right? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_top? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_bottom? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_leftmost? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_rightmost? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_read_coords? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_read_info? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_next_tab? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_previous_tab? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_next_section? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_previous_section? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_child_result? fun(self, pindex: number, result: any, context: any, controller: fa.ui.RouterController) Result is string (plain textbox), fa.ui.TextboxRichTextResult (rich text textbox), or child UI specific
---@field on_accelerator? fun(self, pindex: number, accelerator_name: fa.ui.Accelerator, modifiers: table?, controller: fa.ui.RouterController)
---@field on_clear? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_dangerous_delete? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_announce_title? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_bar_min? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_bar_max? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_bar_up_small? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_bar_down_small? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_bar_up_large? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_bar_down_large? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_action1? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_action2? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_action3? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_drag_up? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_drag_down? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_drag_left? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_drag_right? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_conjunction_modification? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_add_to_row? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_set_filter? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field on_clear_filter? fun(self, pindex: number, modifiers: table?, controller: fa.ui.RouterController)
---@field get_help_metadata? fun(self, pindex: number): fa.ui.help.HelpItem[]?
---@field get_binds? fun(self, pindex: number, parameters: table): fa.ui.Bind[]? Return binds for this UI, nil closes immediately

---@enum fa.ui.UiName
mod.UI_NAMES = {
   -- New inventory, but we need the old name for a little bit while we roll over.
   GENERIC_INVENTORY = "inventory2",
   -- BUILDING and VEHICLE removed - migrating to capability-based UI
   -- Generic entity UI that adapts to entity capabilities
   ENTITY = "entity",
   WARNINGS = "warnings",
   SIGNAL_SELECTOR = "signal_selector",
   DEBUG = "debug",
   ITEM_CHOOSER = "item_chooser",
   SIGNAL_CHOOSER = "signal_chooser",
   FLUID_CHOOSER = "fluid_chooser",
   LOGISTICS_CONFIG = "logistics_config",
   LOGISTIC_GROUP_SELECTOR = "logistic_group_selector",
   CONSTANT_COMBINATOR = "constant_combinator",
   DECIDER_COMBINATOR = "decider_combinator",
   POWER_SWITCH = "power_switch",
   PROGRAMMABLE_SPEAKER = "programmable_speaker",
   BOX_SELECTOR = "box_selector",
   SIMPLE_TEXTBOX = "simple_textbox",
   CURSOR_COORDINATE_INPUT = "cursor_coordinate_input",
   SYNTRAX_INPUT = "syntrax_input",
   RAIL_BUILDER = "rail_builder",
   SYNTRAX_PROGRAM = "syntrax_program",
   CIRCUIT_NAVIGATOR = "circuit_navigator",
   CIRCUIT_NAVIGATOR_ENTITIES = "circuit_navigator_entities",
   EQUIPMENT_SELECTOR = "equipment_selector",
   WEAPON_SELECTOR = "weapon_selector",
   AMMO_SELECTOR = "ammo_selector",
   ENTITY_CHOOSER = "entity_chooser",
   HELP = "help",
   TUTORIAL = "tutorial",
   PROTOTYPE_LISTER = "prototype_lister",
   -- Test UI names (used in automated tests)
   TEST_ASSEMBLING_MACHINE = "test-assembling-machine",
   TEST_ASSEMBLING_MACHINE_FOCUS = "test-assembling-machine-focus",
   INVENTORY = "inventory",
}

---@enum fa.ui.Accelerator
mod.ACCELERATORS = {
   ENTER_CONSTANT = "enter_constant",
   SELECT_SIGNAL = "select_signal",
   UNLOAD_GUNS = "unload_guns",
   UNLOAD_EQUIPMENT = "unload_equipment",
   PREVIEW_SPEAKER = "preview_speaker",
}

---@enum fa.ui.BindKind
mod.BIND_KIND = {
   NONE = "none", -- Default, no binding
   ENTITY = "entity", -- Close if entity is destroyed
   HAND_CONTENTS = "hand", -- Close if player's hand contents change
}

---@class fa.ui.Bind
---@field kind fa.ui.BindKind
---@field entity LuaEntity? Only for ENTITY kind

---@class fa.ui.StackEntry
---@field name fa.ui.UiName
---@field context any?
---@field binds fa.ui.Bind[]?
---@field entity_registration_numbers uint64[]?

---@class fa.ui.RouterState
---@field ui_stack fa.ui.StackEntry[] Stack of open UIs with contexts (top is last)

---@type table<integer, fa.ui.RouterState>
local router_state = StorageManager.declare_storage_module("ui_router", {
   ui_stack = {},
}, {
   ephemeral_state_version = 3,
})

---@class fa.ui.Router
---@field pindex integer
local Router = {}
local Router_meta = { __index = Router }

---@class fa.ui.RouterController
---@field router fa.ui.Router
---@field pindex integer
---@field message fa.MessageBuilder Temporary message accumulator during event processing
local RouterController = {}
local RouterController_meta = { __index = RouterController }

---Create a controller for handling a UI event
---@param router fa.ui.Router
---@return fa.ui.RouterController
local function create_controller_for_event(router)
   return setmetatable({
      router = router,
      pindex = router.pindex,
      message = MessageBuilder.new(),
   }, RouterController_meta)
end

function RouterController:close()
   self.router:close_ui()
end

---Finalize the event by speaking accumulated message
function RouterController:finalize()
   local msg = self.message:build_or_nil()
   if msg then Speech.speak(self.pindex, msg) end
end

---Close the current UI and return a result to the parent
---@param result any The result to pass to the parent UI
function RouterController:close_with_result(result)
   self.router:close_with_result(result)
end

---Open a child UI on top of the current one
---@param name fa.ui.UiName
---@param params? table Optional parameters to pass to the child UI
---@param context? any Optional context to help identify which node/item opened the child UI
function RouterController:open_child_ui(name, params, context)
   self.router:open_child_ui(name, params, context)
end

---Open a textbox for user input
---@param initial_text string
---@param context any Any value to help identify which node/item opened the textbox
---@param options fa.ui.TextboxOptions? Options for the textbox (intro_message, rich_text)
function RouterController:open_textbox(initial_text, context, options)
   GameGui.open_textbox(self.pindex, initial_text, context, options)
end

---@param name fa.ui.UiName
---@param params? table Optional parameters to pass to the UI
function Router:open_ui(name, params)
   -- If the top UI is an overlay, treat this as opening a child UI instead of clearing
   if self:is_in_overlay(true) then
      local stack = router_state[self.pindex].ui_stack
      -- Check for cycles - don't allow opening a UI that's already in the stack
      for _, entry in ipairs(stack) do
         if entry.name == name then
            Speech.speak(self.pindex, { "fa.ui-already-open" })
            return
         end
      end
      self:open_child_ui(name, params)
   else
      -- Clear stack then push new UI
      self:_clear_ui_stack()
      self:_push_ui(name, params)
   end
end

function Router:close_ui()
   self:_pop_ui()
end

-- Core stack operations

---Push a UI onto the stack (open it)
---@param name fa.ui.UiName
---@param params? table
---@param context? any Context to store with this UI entry
function Router:_push_ui(name, params, context)
   assert(
      registered_uis[name],
      "UI '" .. tostring(name) .. "' is not registered. Did you forget to require the module?"
   )

   local ui = registered_uis[name]
   local stack = router_state[self.pindex].ui_stack

   -- Process binds if the UI defines get_binds
   local binds = nil
   local entity_reg_numbers = nil
   if ui.get_binds then
      binds = ui:get_binds(self.pindex, params or {})
      if binds == nil then
         -- Bind callback returned nil, UI should not open
         return
      end

      -- Register entities for destruction notification
      entity_reg_numbers = {}
      for _, bind in ipairs(binds) do
         if bind.kind == mod.BIND_KIND.ENTITY and bind.entity and bind.entity.valid then
            local reg_number = script.register_on_object_destroyed(bind.entity)
            table.insert(entity_reg_numbers, reg_number)
         end
      end
   end

   -- Add to stack with context and binds
   ---@type fa.ui.StackEntry
   local entry = {
      name = name,
      context = context,
      binds = binds,
      entity_registration_numbers = entity_reg_numbers,
   }
   table.insert(stack, entry)

   -- Update GUI to show new top
   GameGui.set_active_ui(self.pindex, name)

   -- Play open sound
   UiSounds.play_open_inventory(self.pindex)

   -- Call the UI's open method
   local controller = create_controller_for_event(self)
   ui:open(self.pindex, params or {}, controller)
   controller:finalize()
end

---Pop the top UI from the stack (close it)
---@return fa.ui.UiName? The name of the UI that was closed
function Router:_pop_ui()
   local stack = router_state[self.pindex].ui_stack
   if #stack == 0 then return nil end

   local top_entry = stack[#stack]
   local ui_name = top_entry.name

   -- Call the UI's close method
   if registered_uis[ui_name] then
      local ui = registered_uis[ui_name]
      if ui.close then ui:close(self.pindex) end
   end

   -- Remove from stack
   table.remove(stack)

   -- Play close sound
   UiSounds.play_close_inventory(self.pindex)

   -- Update GUI to show new top (or clear if empty)
   if #stack > 0 then
      GameGui.set_active_ui(self.pindex, stack[#stack].name)
   else
      GameGui.clear_active_ui(self.pindex)
   end

   return ui_name
end

---Clear all UIs from the stack
function Router:_clear_ui_stack()
   local stack = router_state[self.pindex].ui_stack
   if #stack == 0 then return end

   -- Close all UIs in reverse order (top to bottom)
   for i = #stack, 1, -1 do
      local entry = stack[i]
      local ui_name = entry.name
      if registered_uis[ui_name] then
         local ui = registered_uis[ui_name]
         if ui.close then ui:close(self.pindex) end
      end
   end

   -- Clear the stack
   router_state[self.pindex].ui_stack = {}

   -- Play single close sound
   UiSounds.play_close_inventory(self.pindex)

   -- Clear the GUI
   GameGui.clear_active_ui(self.pindex)
end

---Check if any UI in the stack has a HAND_CONTENTS bind
---@return boolean
function Router:_has_hand_bind()
   local stack = router_state[self.pindex].ui_stack
   for _, entry in ipairs(stack) do
      if entry.binds then
         for _, bind in ipairs(entry.binds) do
            if bind.kind == mod.BIND_KIND.HAND_CONTENTS then return true end
         end
      end
   end
   return false
end

---Get all entity registration numbers tracked by UIs in the stack
---@return uint64[]
function Router:_get_all_registration_numbers()
   local result = {}
   local stack = router_state[self.pindex].ui_stack
   for _, entry in ipairs(stack) do
      if entry.entity_registration_numbers then
         for _, reg_num in ipairs(entry.entity_registration_numbers) do
            table.insert(result, reg_num)
         end
      end
   end
   return result
end

--[[
Ask if a UI is open.

This function primarily exists for the test framework.  It may be tempting to use it in other code, but you should
instead go through the UI event management at the bottom of this file, by overriding events in the ways others are
currently.
]]
---@param name fa.ui.UiName? If null, return true if any UI is open.
---@return boolean
function Router:is_ui_open(name)
   local stack = router_state[self.pindex].ui_stack
   if not name then return #stack > 0 end
   -- Check if name exists anywhere in the stack
   for _, entry in ipairs(stack) do
      if entry.name == name then return true end
   end
   return false
end

---@return fa.ui.UiName?
function Router:get_open_ui_name()
   local stack = router_state[self.pindex].ui_stack
   if #stack == 0 then return nil end
   return stack[#stack].name -- Return name from top of stack
end

---Check if there is an overlay in the UI stack
---@param top_only? boolean If true, only check if the top UI is an overlay. If false/nil, check entire stack.
---@return boolean
function Router:is_in_overlay(top_only)
   local stack = router_state[self.pindex].ui_stack
   if #stack == 0 then return false end

   if top_only then
      -- Only check the top UI
      local top_entry = stack[#stack]
      local ui = registered_uis[top_entry.name]
      if ui and ui.is_overlay and ui:is_overlay() then return true end
   else
      -- Check if any UI in the stack is an overlay
      for _, entry in ipairs(stack) do
         local ui = registered_uis[entry.name]
         if ui and ui.is_overlay and ui:is_overlay() then return true end
      end
   end

   return false
end

---Open a child UI on top of the current one without closing it
---@param name fa.ui.UiName
---@param params? table Optional parameters to pass to the child UI
---@param context? any Optional context to store with this UI
function Router:open_child_ui(name, params, context)
   local stack = router_state[self.pindex].ui_stack

   -- If we're in an overlay, check if this UI is already in the stack
   if self:is_in_overlay() then
      for _, entry in ipairs(stack) do
         if entry.name == name then
            error("Cannot enter UI " .. name .. " while already in it during overlay selection")
         end
      end
   end

   -- Just push without clearing
   self:_push_ui(name, params, context)
end

---Close the current UI and return a result to the parent
---@param result any The result to pass to the parent UI
function Router:close_with_result(result)
   local stack = router_state[self.pindex].ui_stack
   if #stack == 0 then return end

   -- Get the context of the UI being closed
   local closing_ui = stack[#stack]
   local context = closing_ui.context

   -- Get parent before popping
   local parent_ui = #stack > 1 and stack[#stack - 1] or nil

   -- Pop the current UI
   self:_pop_ui()

   -- Send result to parent if there is one
   if parent_ui and registered_uis[parent_ui.name] then
      local controller = create_controller_for_event(self)
      registered_uis[parent_ui.name]:on_child_result(self.pindex, result, context, controller)
      controller:finalize()
   end
end

-- Routers are stateless save for the pindex; we cache them to avoid the overhead of making new ones.
local router_cache = {}

---@param pindex integer
---@return fa.ui.Router
function mod.get_router(pindex)
   assert(pindex ~= nil)
   if router_cache[pindex] then return router_cache[pindex] end
   local new_router = setmetatable({ pindex = pindex }, Router_meta)
   router_cache[pindex] = new_router
   return new_router
end

---Register a UI event that only fires when a UI is open
---@param event_name string|defines.events The event name or ID to register
---@param handler fun(event: EventData, pindex: integer): any? The handler function
local function register_ui_event(event_name, handler)
   EventManager.on_event(event_name, function(event, pindex)
      -- UI events are all custom inputs, which always come from a player
      assert(pindex)
      local router = mod.get_router(pindex)
      -- Only process if a UI is open
      if router:is_ui_open() then return handler(event, pindex) end
      -- No UI open, let it fall through
      return nil
   end, EventManager.EVENT_KIND.UI)
end

---Create a handler that routes to the appropriate TabList method
---@param method_name string The method name to call on the TabList
---@param modifiers? {control?: boolean, shift?: boolean, alt?: boolean} Optional modifier keys state
---@param is_enabled_during_overlay? boolean If true, this handler works even when an overlay UI is open (default false)
---@return fun(event: EventData, pindex: integer): any
local function create_ui_handler(method_name, modifiers, is_enabled_during_overlay)
   -- Default to no modifiers if not provided
   modifiers = modifiers or { control = false, shift = false, alt = false }
   is_enabled_during_overlay = is_enabled_during_overlay or false

   return function(event, pindex)
      local router = mod.get_router(pindex)
      local stack = router_state[pindex].ui_stack

      -- Only check the top UI (the active one)
      if #stack > 0 then
         local top_entry = stack[#stack]
         local ui_name = top_entry.name

         if registered_uis[ui_name] then
            local ui = registered_uis[ui_name]

            -- If we're in an overlay and this handler is not enabled during overlay, skip
            local in_overlay = ui.is_overlay and ui:is_overlay()
            if in_overlay and not is_enabled_during_overlay then return nil end

            -- Check if this UI has the method we're looking for
            if ui[method_name] then
               local controller = create_controller_for_event(router)
               ui[method_name](ui, pindex, modifiers, controller)
               controller:finalize()
               return EventManager.FINISHED
            end
         end
      end

      -- No UI handled it or no UI open, let event pass through to world handlers
      return nil
   end
end

register_ui_event("fa-w", create_ui_handler("on_up"))
register_ui_event("fa-s", create_ui_handler("on_down"))
register_ui_event("fa-a", create_ui_handler("on_left"))
register_ui_event("fa-d", create_ui_handler("on_right"))

-- Drag keys (Shift+WASD for reordering/moving)
register_ui_event("fa-s-w", create_ui_handler("on_drag_up"))
register_ui_event("fa-s-s", create_ui_handler("on_drag_down"))
register_ui_event("fa-s-a", create_ui_handler("on_drag_left"))
register_ui_event("fa-s-d", create_ui_handler("on_drag_right"))

-- Edge navigation (Ctrl+WASD) - enabled during overlay for cursor navigation
register_ui_event("fa-c-w", create_ui_handler("on_top"))
register_ui_event("fa-c-s", create_ui_handler("on_bottom"))
register_ui_event("fa-c-a", create_ui_handler("on_leftmost"))
register_ui_event("fa-c-d", create_ui_handler("on_rightmost"))

-- Section navigation (Ctrl+TAB and Ctrl+Shift+TAB)
register_ui_event("fa-c-tab", create_ui_handler("on_next_section"))
register_ui_event("fa-cs-tab", create_ui_handler("on_previous_section"))

-- K key reads coordinates - enabled during overlay for cursor navigation
register_ui_event("fa-k", create_ui_handler("on_read_coords", nil, true))

-- Y key reads custom info - enabled during overlay for cursor navigation
register_ui_event("fa-y", create_ui_handler("on_read_info", nil, true))

-- Handle textbox confirmation - this one needs special handling since it checks textbox validity
---@param event EventData.on_gui_confirmed
EventManager.on_event(defines.events.on_gui_confirmed, function(event)
   local pindex = event.player_index
   -- Check if this is our textbox
   if event.element and event.element.valid and event.element.name == "fa-text-input" then
      local router = mod.get_router(pindex)
      local top_ui_name = router:get_open_ui_name()

      if top_ui_name then
         local context = GameGui.get_textbox_context(pindex)
         local is_rich_text = GameGui.is_textbox_rich_text(pindex)
         local result

         -- Process rich text if enabled
         if is_rich_text then
            local text = event.element.text
            local expanded, errors = RichText.parse_rich_text_shorthand(text)
            result = { value = expanded, errors = errors }
         else
            result = event.element.text
         end

         -- Send result to the top UI on the stack
         local ui = registered_uis[top_ui_name]
         if ui and ui.on_child_result then
            local controller = create_controller_for_event(router)
            ui:on_child_result(pindex, result, context, controller)
            controller:finalize()
         end
         -- Close the textbox
         GameGui.close_textbox(pindex)
         return EventManager.FINISHED
      end
   end
   -- Not our textbox or no UI open
   return nil
end, EventManager.EVENT_KIND.UI)

-- Equipment/inventory accelerators (only work when UI is open)
-- CTRL+SHIFT+R: Unload guns/ammo
register_ui_event("fa-cs-r", function(event, pindex)
   local router = mod.get_router(pindex)
   local stack = router_state[pindex].ui_stack

   if #stack > 0 then
      local top_entry = stack[#stack]
      local ui_name = top_entry.name
      if registered_uis[ui_name] then
         local ui = registered_uis[ui_name]
         if ui.on_accelerator then
            local controller = create_controller_for_event(router)
            ui:on_accelerator(pindex, mod.ACCELERATORS.UNLOAD_GUNS, nil, controller)
            controller:finalize()
            return EventManager.FINISHED
         end
      end
   end
   return nil
end)

-- Bind invalidation handlers (called from control.lua)

---Called when player's hand contents change. Closes all UIs if any have HAND_CONTENTS bind.
---@param pindex integer
function mod.on_hand_contents_changed(pindex)
   local router = mod.get_router(pindex)
   if router:_has_hand_bind() then router:_clear_ui_stack() end
end

---Called when an entity is destroyed. Closes all UIs if the registration number is tracked.
---@param registration_number uint64
function mod.on_entity_destroyed(registration_number)
   -- Must check all players since on_object_destroyed has no player_index
   for pindex, _ in pairs(router_state) do
      local router = mod.get_router(pindex)
      local reg_numbers = router:_get_all_registration_numbers()
      for _, reg_num in ipairs(reg_numbers) do
         if reg_num == registration_number then
            router:_clear_ui_stack()
            break
         end
      end
   end
end

return mod
