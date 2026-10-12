require("syntrax")

--Main file for mod runtime
local Logging = require("scripts.logging")
Logging.init()
-- Set logging level
--Logging.set_level("DEBUG")

-- Create logger for control.lua
local logger = Logging.Logger("control")

local util = require("util")

local AreaOperations = require("scripts.area-operations")
local AudioCues = require("scripts.audio-cues")
local Blueprints = require("scripts.blueprints")
local BuildingTools = require("scripts.building-tools")
local BuildDimensions = require("scripts.build-dimensions")
local BumpDetection = require("scripts.bump-detection")
local CircuitNetworks = require("scripts.circuit-network")
local Combat = require("scripts.combat")
local Consts = require("scripts.consts")
local SettingDecls = require("scripts.settings-decls")
local SETTING_NAMES = SettingDecls.SETTING_NAMES
local Cursor = require("scripts.cursor")
local CursorChanges = require("scripts.cursor-changes")
local Daytime = require("scripts.daytime")
local Driving = require("scripts.driving")
local HandMonitor = require("scripts.hand-monitor")
local Electrical = require("scripts.electrical")
local EventManager = require("scripts.event-manager")
require("scripts.fa-commands") -- registers FA's console commands
local FaInfo = require("scripts.fa-info")
local FaUtils = require("scripts.fa-utils")
local Hand = require("scripts.hand")
local F = require("scripts.field-ref")
local Filters = require("scripts.filters")
local GameNotices = require("scripts.game-notices")
local InventoryTransfers = require("scripts.inventory-transfers")
local InventoryUtils = require("scripts.inventory-utils")
local ItemInfo = require("scripts.item-info")
local KruiseKontrol = require("scripts.kruise-kontrol-wrapper")
local Localising = require("scripts.localising")
local MapCells = require("scripts.map-cells")
local NativeCursor = require("scripts.native-cursor")
local Speech = require("scripts.speech")
local MessageBuilder = Speech.MessageBuilder
local MovementHistory = require("scripts.movement-history")
local PlayerInit = require("scripts.player-init")
local PlayerMiningTools = require("scripts.player-mining-tools")
require("scripts.rich-text") -- registers rich text processor with speech.lua
local RailRoute = require("scripts.rails.rail-route")
local Rulers = require("scripts.rulers")
local ScannerEntrypoint = require("scripts.scanner.entrypoint")
local ScannerLegacy = require("scripts.scanner.legacy")
local SpidertronRemote = require("scripts.spidertron-remote")
local TH = require("scripts.table-helpers")
local TestFramework = require("scripts.test-framework")
local TransportBelts = require("scripts.transport-belts")
local UpgradePlanner = require("scripts.upgrade-planner")
local VanillaMode = require("scripts.vanilla-mode")
local VirtualTrainDriving = require("scripts.rails.virtual-train-driving")
local SonifierTickHandler = require("scripts.sonifiers.tick-handler")
local GridSonifier = require("scripts.sonifiers.grid-sonifier")
local CraftingBackend = require("scripts.sonifiers.grid-backends.crafting")
local BattleNotice = require("scripts.sonifiers.battle-notice")
local ForceGhostEnabler = require("scripts.force-ghost-enabler")
local AimAssist = require("scripts.combat.aim-assist")
local Capsules = require("scripts.combat.capsules")
local PlayerWeapon = require("scripts.combat.player-weapon")
local ViewLimit = require("scripts.view-limit")
local Zoom = require("scripts.zoom")

-- UI modules (required for registration with router)
require("scripts.ui.belt-analyzer")
require("scripts.ui.fluid-views")
require("scripts.ui.pole-views")
local EntityUI = require("scripts.ui.entity-ui")
require("scripts.ui.menus.gun-menu")
local WorldMenu = require("scripts.ui.menus.world-menu")
require("scripts.ui.menus.debug-menu")
require("scripts.ui.menus.rail-builder")
require("scripts.ui.menus.syntrax-program")
require("scripts.ui.tabs.item-chooser")
require("scripts.ui.tabs.signal-chooser")
require("scripts.ui.tabs.fluid-chooser")
require("scripts.ui.tabs.entity-chooser")
require("scripts.ui.tabs.prototype-lister")
require("scripts.ui.tabs.equipment-selector")
local Help = require("scripts.ui.help")
local MessageLists = require("scripts.message-lists")
require("scripts.ui.logistics-config")
require("scripts.ui.selectors.logistic-group-selector")
require("scripts.ui.selectors.train-group-selector")
require("scripts.ui.selectors.interrupt-selector")
require("scripts.ui.selectors.stop-selector")
require("scripts.ui.constant-combinator")
require("scripts.ui.decider-combinator")
require("scripts.ui.power-switch")
require("scripts.ui.schedule-editor")
require("scripts.ui.programmable-speaker")
require("scripts.ui.circuit-navigator")
require("scripts.ui.menus.warnings")
require("scripts.ui.generic-inventory")
require("scripts.ui.simple-textbox")
require("scripts.ui.internal.cursor-coordinate-input")
require("scripts.ui.internal.syntrax-input")
require("scripts.ui.help")
require("scripts.ui.menus.tutorial")
local GameGui = require("scripts.ui.game-gui")
local UiRouter = require("scripts.ui.router")
local VehicleCycler = require("scripts.vehicle-cycler")
local Viewpoint = require("scripts.viewpoint")
local Wires = require("scripts.wires")
local Walking = require("scripts.walking")
local Warnings = require("scripts.warnings")
local WorkerRobots = require("scripts.worker-robots")
local sounds = require("scripts.ui.sounds")

---@meta scripts.shared-types

-- Register grid sonifier backends
GridSonifier.register_backend_factory("crafting", CraftingBackend.new, CraftingBackend.ENTITY_TYPES)

entity_types = {}
production_types = {}
building_types = {}
local dirs = defines.direction

---Check if in combat mode and speak feedback if so
---@param pindex integer
---@return boolean true if in combat mode (caller should return early)
local function skip_in_combat_mode(pindex)
   if Combat.is_combat_mode(pindex) then
      Speech.speak(pindex, { "fa.not-available-in-combat-mode" })
      return true
   end
   return false
end

-- Initialize players as a deny-access table to catch any direct usage
players = TH.deny_access_table()

--Reads the item in hand, its facing direction if applicable, its count, and its total count including units in the main inventory.
---@param pindex integer
local function read_hand(pindex)
   if storage.players[pindex].skip_read_hand == true then
      storage.players[pindex].skip_read_hand = false
      return
   end
   -- Entering or leaving remote view changes the hand, and the view's announcement says it
   local hand = Hand.describe(pindex)
   if Zoom.hand_for_view(pindex, hand) then return end
   Speech.speak(pindex, hand or { "fa.empty_cursor" })
end

--Checks if the storage players table has been created, and if the table entry for this player exists. Otherwise it is initialized.
function check_for_player(index)
   if storage.players[index] == nil then
      local player = game.get_player(index)
      if player then PlayerInit.initialize(player) end
      return false
   else
      return true
   end
end

--Update the position info and cursor info during smooth walking.
EventManager.on_event(
   defines.events.on_player_changed_position,
   ---@param event EventData.on_player_changed_position
   ---@param pindex integer
   function(event, pindex)
      local p = game.get_player(pindex)

      -- Check for teleportation (large position jump)
      local reader = MovementHistory.get_movement_history_reader(pindex)
      local prev_entry = reader:get(0)
      local old_pos = prev_entry and prev_entry.position
      if old_pos and util.distance(old_pos, p.physical_position) > 10 then
         MovementHistory.reset_and_increment_generation(pindex)
      end
   end
)

--Tells the DLL what the movement keys do for a player: they move the cursor unless a mod menu has
--them or combat mode aims with them.
---@param player LuaPlayer
local function report_cursor_mode(player)
   local pindex = player.index
   if not player.connected or not storage.players[pindex] then return end
   Cursor.report_mode(
      pindex,
      VanillaMode.is_enabled(pindex),
      not UiRouter.get_router(pindex):is_ui_open() and not Combat.is_combat_mode(pindex),
      Viewpoint.get_viewpoint(pindex):get_cursor_anchored(),
      Rulers.ruler_tiles(pindex),
      player.mod_settings[SETTING_NAMES.NEXT_MACHINE_SOUND].value --[[@as boolean]]
   )
end

local function report_cursor_modes()
   for _, player in pairs(game.connected_players) do
      report_cursor_mode(player)
   end
end

-- A paused map runs no ticks, and the map editor pauses it in single player: the mode also goes to
-- the DLL after each of the player's events and when the player joins, before any key.
EventManager.observe_player_events(function(pindex)
   report_cursor_mode(assert(game.get_player(pindex)))
   ScannerEntrypoint.on_player_event(pindex)
end)

EventManager.observe_custom_inputs(function(_, pindex)
   HandMonitor.on_key(pindex)
   Rulers.on_key(pindex)
end)

EventManager.on_event(
   defines.events.on_player_joined_game,
   ---@param event EventData.on_player_joined_game
   ---@param pindex integer
   function(event, pindex)
      MessageLists.on_player_joined(pindex)
   end
)

--Called every tick.
function on_tick(event)
   MessageLists.try_translate_all_players()
   report_cursor_modes()
   ScannerEntrypoint.on_tick()
   MovementHistory.update_all_players()
   Rulers.update_all_players()
   SonifierTickHandler.on_tick()
   BattleNotice.on_tick()
   ForceGhostEnabler.on_tick()

   -- Check alerts via AudioCues
   AudioCues.check_cues(event.tick, players)

   -- Check bump detection every tick for all players
   for _, player in pairs(game.players) do
      if not player.connected then goto continue end
      if VanillaMode.is_enabled(player.index) then goto continue end

      BumpDetection.play_step_sounds(player.index, event.tick)
      -- Process walking announcements (anchored cursor or entity detection)
      Walking.process_walking_announcements(player.index)
      -- Check for pending logistics announcements
      WorkerRobots.on_tick(player.index)
      -- Handle shooting (combat mode aim assist or shooting_selected with safe mode)
      Combat.on_tick(player.index)

      ::continue::
   end

   if event.tick % 15 == 0 then
      for pindex, player in pairs(players) do
         -- Other periodic checks can go here
      end
      KruiseKontrol.read_remarks()
      GameNotices.on_tick()
   elseif event.tick % 450 == 14 then
      --Run regular reminders every 7.5 seconds
      for pindex, player in pairs(players) do
         if game.get_player(pindex).ticks_to_respawn ~= nil then
            Speech.speak(
               pindex,
               { "fa.respawn-countdown", tostring(math.floor(game.get_player(pindex).ticks_to_respawn / 60)) }
            )
         end
      end
   end
end

EventManager.on_event(
   defines.events.on_tick,
   ---@param event EventData.on_tick
   function(event)
      on_tick(event)
      TestFramework.on_tick(event)
      HandMonitor.on_tick()
   end
)

--Called when a player enters or exits a vehicle
EventManager.on_event(
   defines.events.on_player_driving_changed_state,
   ---@param event EventData.on_player_driving_changed_state
   ---@param pindex integer
   function(event, pindex)
      local router = UiRouter.get_router(pindex)

      MovementHistory.reset_and_increment_generation(pindex)
      game.get_player(pindex).clear_cursor()
      if game.get_player(pindex).driving then
         storage.players[pindex].last_vehicle = game.get_player(pindex).vehicle
         Speech.speak(
            pindex,
            { "fa.vehicle-entered", Localising.get_localised_name_with_fallback(game.get_player(pindex).vehicle) }
         )
      elseif storage.players[pindex].last_vehicle ~= nil then
         Speech.speak(
            pindex,
            { "fa.vehicle-exited", Localising.get_localised_name_with_fallback(storage.players[pindex].last_vehicle) }
         )
      else
         Speech.speak(pindex, { "fa.driving-state-changed" })
      end
   end
)

--Called when a player changes surface
EventManager.on_event(
   defines.events.on_player_changed_surface,
   ---@param event EventData.on_player_changed_surface
   ---@param pindex integer
   function(event, pindex)
      MovementHistory.reset_and_increment_generation(pindex)
   end
)

--Save info about last item pickup and draw radius
EventManager.on_event(
   defines.events.on_picked_up_item,
   ---@param event EventData.on_picked_up_item
   ---@param pindex integer
   function(event, pindex)
      local p = game.get_player(pindex)
      --Draw the pickup range
      rendering.draw_circle({
         color = { 0.3, 1, 0.3 },
         radius = 1.25,
         width = 1,
         target = p.position,
         surface = p.surface,
         time_to_live = 10,
         draw_on_ground = true,
      })
      storage.players[pindex].last_pickup_tick = event.tick
      storage.players[pindex].last_item_picked_up = event.item_stack.name
   end
)

function swap_weapon_forward(pindex, write_to_character)
   local p = game.get_player(pindex)
   if p.character == nil then
      return 0 --This is an intentionally selected error code
   end
   local gun_index = p.character.selected_gun_index
   if gun_index == nil then
      return 0 --This is an intentionally selected error code
   end
   local guns_inv = p.character.get_inventory(defines.inventory.character_guns)
   local ammo_inv = game.get_player(pindex).character.get_inventory(defines.inventory.character_ammo)

   --Simple index increment (not needed)
   gun_index = gun_index + 1
   if gun_index > 3 then gun_index = 1 end
   --game.print("start " .. gun_index)--
   assert(ammo_inv)

   --Increment again if the new index has no guns or no ammo
   local ammo_stack = ammo_inv[gun_index]
   local gun_stack = guns_inv[gun_index]
   local tries = 0
   while
      tries < 4
      and (
         ammo_stack == nil
         or not ammo_stack.valid_for_read
         or not ammo_stack.valid
         or gun_stack == nil
         or not gun_stack.valid_for_read
         or not gun_stack.valid
      )
   do
      gun_index = gun_index + 1
      if gun_index > 3 then gun_index = 1 end
      ammo_stack = ammo_inv[gun_index]
      gun_stack = guns_inv[gun_index]
      tries = tries + 1
   end

   if tries > 3 then
      --game.print("error " .. gun_index)--
      return -1
   end

   if write_to_character then p.character.selected_gun_index = gun_index end
   --game.print("end " .. gun_index)--
   return gun_index
end

function swap_weapon_backward(pindex, write_to_character)
   local p = game.get_player(pindex)
   if p.character == nil then
      return 0 --This is an intentionally selected error code
   end
   local gun_index = p.character.selected_gun_index
   if gun_index == nil then
      return 0 --This is an intentionally selected error code
   end
   local guns_inv = p.get_inventory(defines.inventory.character_guns)
   local ammo_inv = game.get_player(pindex).get_inventory(defines.inventory.character_ammo)

   --Simple index increment (not needed)
   gun_index = gun_index - 1
   if gun_index < 1 then gun_index = 3 end

   --Increment again if the new index has no guns or no ammo
   local ammo_stack = ammo_inv[gun_index]
   local gun_stack = guns_inv[gun_index]
   local tries = 0
   while
      tries < 4
      and (
         ammo_stack == nil
         or not ammo_stack.valid_for_read
         or not ammo_stack.valid
         or gun_stack == nil
         or not gun_stack.valid_for_read
         or not gun_stack.valid
      )
   do
      gun_index = gun_index - 1
      if gun_index < 1 then gun_index = 3 end
      ammo_stack = ammo_inv[gun_index]
      gun_stack = guns_inv[gun_index]
      tries = tries + 1
   end

   if tries > 3 then return -1 end

   if write_to_character then p.character.selected_gun_index = gun_index end
   return gun_index
end

--[[Manages inventory transfers that are bigger than one stack.
* Has checks and speech output!
]]

--When the item in hand changes
EventManager.on_event(
   defines.events.on_player_cursor_stack_changed,
   ---@param event EventData.on_player_cursor_stack_changed
   ---@param pindex integer
   function(event, pindex)
      -- Skip if suppressed (e.g., during rail building hand swaps)
      if not HandMonitor.is_enabled(pindex) then return end
      if Rulers.tool_away(pindex) then return end

      CursorChanges.on_cursor_stack_changed(event, pindex, read_hand)
      VirtualTrainDriving.on_cursor_stack_changed(event)

      -- Close UIs that are bound to hand contents
      UiRouter.on_hand_contents_changed(pindex)
   end
)

EventManager.on_event(
   defines.events.on_player_mined_item,
   ---@param event EventData.on_player_mined_item
   ---@param pindex integer
   function(event, pindex)
      --Play item pickup sound
      sounds.play_picked_up_item(pindex)
      sounds.play_close_inventory(pindex)
   end
)

function ensure_storage_structures_are_up_to_date()
   ScannerLegacy.drop_old_state()
   storage.forces = storage.forces or {}
   storage.players = storage.players or {}
   for pindex, player in pairs(game.players) do
      PlayerInit.initialize(player)
   end

   storage.entity_types = {}
   entity_types = storage.entity_types

   local types = {}

   for _, ent in pairs(prototypes.entity) do
      if
         types[ent.type] == nil
         and ent.weight == nil
         and (
            ent.burner_prototype ~= nil
            or ent.electric_energy_source_prototype ~= nil
            or ent.automated_ammo_count ~= nil
         )
      then
         types[ent.type] = true
      end
   end

   for i, type in pairs(types) do
      table.insert(entity_types, i)
   end
   table.insert(entity_types, "container")

   storage.production_types = {}
   production_types = storage.production_types

   -- TODO: reimplement production types. Seems only to be the warnings menu
   -- using it.

   storage.building_types = {}
   building_types = storage.building_types

   local ents = prototypes.entity
   local types = {}
   for i, ent in pairs(ents) do
      if ent.is_building then types[ent.type] = true end
   end
   types["transport-belt"] = nil
   for i, type in pairs(types) do
      table.insert(building_types, i)
   end
   table.insert(building_types, "character")
end

EventManager.on_load(function()
   entity_types = storage.entity_types
   production_types = storage.production_types
   building_types = storage.building_types
end)

EventManager.on_configuration_changed(ensure_storage_structures_are_up_to_date)

EventManager.on_init(function()
   ensure_storage_structures_are_up_to_date()
   TestFramework.on_init()
   AudioCues.on_init()
end)

EventManager.on_event(
   defines.events.on_cutscene_finished,
   ---@param event EventData.on_cutscene_finished
   ---@param pindex integer
   function(event, pindex)
      --Speech.speak(pindex, "Press TAB to continue")
   end
)

EventManager.on_event(
   defines.events.on_cutscene_started,
   ---@param event EventData.on_cutscene_started
   ---@param pindex integer
   function(event, pindex)
      --Speech.speak(pindex, "Press TAB to continue")
   end
)

EventManager.on_event(
   defines.events.on_player_created,
   ---@param event EventData.on_player_created
   function(event)
      PlayerInit.initialize(game.players[event.player_index]) --if not game.is_multiplayer() then Speech.speak(pindex, "Press 'TAB' to continue") end
   end
)

EventManager.on_event(
   defines.events.on_gui_closed,
   ---@param event EventData.on_gui_closed
   function(event, pindex)
      local router = UiRouter.get_router(pindex)
      print(serpent.line(event, { nocode = true }))
      router:close_ui()
   end
)

EventManager.on_event(
   defines.events.on_gui_opened,
   ---@param event EventData.on_gui_opened
   ---@param pindex integer
   function(event, pindex)
      local router = UiRouter.get_router(pindex)
      local p = game.get_player(pindex)
   end
)

EventManager.on_event(
   defines.events.on_object_destroyed,
   ---@param event EventData.on_object_destroyed
   function(event) --DOES NOT HAVE THE KEY PLAYER_INDEX
      -- Close UIs that are bound to this entity
      UiRouter.on_entity_destroyed(event.registration_number)
   end
)

--If a filter inserter is selected, the item in hand is set as its output filter item.
function set_inserter_filter_by_hand(pindex, ent)
   local stack = game.get_player(pindex).cursor_stack
   if ent.filter_slot_count == 0 then return "This inserter has no filters to set" end
   if stack == nil or stack.valid_for_read == false then
      --Delete last filter
      for i = ent.filter_slot_count, 1, -1 do
         local filt = Filters.get_filter_prototype(ent, i)
         if filt ~= nil then
            Filters.set_filter(ent, i, nil)
            return "Last filter cleared"
         end
      end
      return "All filters cleared"
   else
      --Add item in hand as next filter
      for i = 1, ent.filter_slot_count, 1 do
         local filt = Filters.get_filter_prototype(ent, i)
         if filt == nil then
            Filters.set_filter(ent, i, stack.name)
            if Filters.get_filter_prototype(ent, i) == stack.name then
               return "Added filter"
            else
               return "Filter setting failed"
            end
         end
      end
      return "All filters full"
   end
end

EventManager.on_event(
   defines.events.on_player_display_resolution_changed,
   ---@param event EventData.on_player_display_resolution_changed
   ---@param pindex integer
   function(event, pindex)
      local new_res = game.get_player(pindex).display_resolution
      if players and storage.players[pindex] then storage.players[pindex].display_resolution = new_res end
   end
)

EventManager.on_event(
   defines.events.on_player_display_scale_changed,
   ---@param event EventData.on_player_display_scale_changed
   ---@param pindex integer
   function(event, pindex)
      local new_sc = game.get_player(pindex).display_scale
      if players and storage.players[pindex] then storage.players[pindex].display_resolution = new_sc end
   end
)

EventManager.on_event(defines.events.on_string_translated, Localising.handler)

EventManager.on_event(
   defines.events.on_player_respawned,
   ---@param event EventData.on_player_respawned
   ---@param pindex integer
   function(event, pindex)
      Cursor.jump(pindex, game.get_player(pindex).position)
      MovementHistory.reset_and_increment_generation(pindex)
   end
)

--If the player has unexpected lateral movement while smooth running in a cardinal direction, like from bumping into an entity or being at the edge of water, play a sound.

function all_ents_are_walkable(pos)
   local ents = game.surfaces[1].find_entities_filtered({
      position = FaUtils.center_of_tile(pos),
      radius = 0.4,
      invert = true,
      type = Consts.ENT_TYPES_YOU_CAN_WALK_OVER,
   })
   for i, ent in ipairs(ents) do
      return false
   end
   return true
end

function general_mod_menu_up(pindex, menu, lower_limit_in) --todo*** use
   local lower_limit = lower_limit_in or 0
   menu.index = menu.index - 1
   if menu.index < lower_limit then
      menu.index = lower_limit
      sounds.play_ui_edge(pindex)
   else
      --Play sound
      sounds.play_menu_move(pindex)
   end
end

function general_mod_menu_down(pindex, menu, upper_limit)
   menu.index = menu.index + 1
   if menu.index > upper_limit then
      menu.index = upper_limit
      sounds.play_ui_edge(pindex)
   else
      --Play sound
      sounds.play_menu_move(pindex)
   end
end

EventManager.on_event(defines.events.on_player_selected_area, SpidertronRemote.on_selected_area)
EventManager.on_event(defines.events.on_player_alt_selected_area, SpidertronRemote.on_selected_area)
EventManager.on_event(defines.events.on_player_reverse_selected_area, SpidertronRemote.on_selected_area)
EventManager.on_event(defines.events.on_player_used_spidertron_remote, SpidertronRemote.on_used_remote)
EventManager.on_event(
   defines.events.on_player_toggled_alt_mode,
   ---@param event EventData.on_player_toggled_alt_mode
   ---@param pindex integer
   function(event, pindex)
      Speech.speak(pindex, { "fa.alt-mode-toggled", event.alt_mode and 1 or 0 })
   end
)
EventManager.on_event(defines.events.on_achievement_gained, GameNotices.on_achievement_gained)
EventManager.on_event(defines.events.on_chart_tag_added, GameNotices.on_chart_tag_added)
EventManager.on_event(defines.events.on_player_controller_changed, function(event)
   Zoom.on_controller_changed(event)
end)
-- New input event definitions

---Plays the sound of a step onto the next entity of a kind bigger than a tile: in a row of machines
---alike the reader says the same name on each, and this marks where one ends.
---@param pindex integer
local function play_next_of_kind(pindex)
   if game.get_player(pindex).mod_settings[SETTING_NAMES.NEXT_MACHINE_SOUND].value then
      sounds.play_next_machine(pindex)
   end
end

---Says where a movement key took the cursor, when the DLL did not read it: the DLL moves it as the
---game reads the key, so the key's event already carries the new tile, or on the full map the new
---cell, and the DLL reads the tile itself. A step has no sound of its own.
---@param pindex integer
---@param event EventData.CustomInputEvent
local function cursor_mode_move(pindex, event)
   local step, _, next_of_kind = Cursor.step(pindex, event)
   if not step then return end
   if step == "edge" then
      ViewLimit.say_edge(pindex)
      return
   end

   if step == "moved" then
      -- Else a step of a drag build, which the game builds on after it: its own preview says what
      -- blocks the build
      local cell_size = Zoom.get_map_cell_size(pindex)
      if cell_size then MapCells.read(pindex, cell_size) end
      return
   end
   if next_of_kind then play_next_of_kind(pindex) end
end

--Chooses the function to call after a movement keypress, according to the current mode.
---@param direction defines.direction
---@param event EventData.CustomInputEvent
local function move_key(direction, event)
   local pindex = event.player_index

   -- Combat mode: Set aim direction instead of moving cursor
   if Combat.is_combat_mode(pindex) then
      AimAssist.set_direction(pindex, direction)
      return
   end

   cursor_mode_move(pindex, event)
end

EventManager.on_event(
   "fa-w",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      move_key(defines.direction.north, event)
   end
)

EventManager.on_event(
   "fa-a",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      move_key(defines.direction.west, event)
   end
)

EventManager.on_event(
   "fa-s",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      move_key(defines.direction.south, event)
   end
)

EventManager.on_event(
   "fa-d",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      move_key(defines.direction.east, event)
   end
)

---Sounds where a skip key took the cursor (Shift with a movement key, or Ctrl, which moves by the
---size of the building in hand): the DLL finds the tile as the game reads the key and reads it
---itself (native/src/skip.h).
---@param pindex integer
---@param event EventData.CustomInputEvent
local function cursor_skip(pindex, event)
   local step, tiles, next_of_kind = Cursor.step(pindex, event)
   if not step then return end
   if step == "edge" then
      ViewLimit.say_edge(pindex)
      return
   end
   if next_of_kind then play_next_of_kind(pindex) end

   local p = game.get_player(pindex)
   local cursor_pos = Cursor.tile(pindex)
   if step == "skipped" then
      -- A skip of more than a tile, or one that found no change, rolls; one of a tile is a step
      if tiles == 1 then return end
      if storage.players[pindex].remote_view then
         sounds.play_sound_at_position({ path = "inventory-wrap-around", volume_modifier = 1 }, cursor_pos)
      else
         p.play_sound({ path = "inventory-wrap-around", position = p.position, volume_modifier = 1 })
      end
   elseif storage.players[pindex].remote_view then
      sounds.play_building_placement(p.index, cursor_pos)
   else
      p.play_sound({ path = "Close-Inventory-Sound", position = p.position, volume_modifier = 1 })
   end
end

---Handle shift+direction key press
---In combat mode with capsule: use capsule in direction
---Outside combat mode: cursor skip
---@param direction defines.direction
---@param event EventData.CustomInputEvent
local function handle_shift_direction(direction, event)
   local pindex = event.player_index
   -- In combat mode, shift+wasd throws capsules if one is held
   if Combat.is_combat_mode(pindex) then
      if Capsules.get_held_capsule_data(pindex) then
         Capsules.use_capsule_in_direction(pindex, direction, false)
      else
         Speech.speak(pindex, { "fa.no-capsule-in-hand" })
      end
      return
   end
   cursor_skip(pindex, event)
end

---Handle ctrl+direction key press: the cursor moves by the size of the building in hand.
---@param event EventData.CustomInputEvent
local function handle_ctrl_direction(event)
   local pindex = event.player_index
   if skip_in_combat_mode(pindex) then return end
   cursor_skip(pindex, event)
end

EventManager.on_event(
   "fa-s-w",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      handle_shift_direction(defines.direction.north, event)
   end
)

EventManager.on_event(
   "fa-s-a",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      handle_shift_direction(defines.direction.west, event)
   end
)

EventManager.on_event(
   "fa-s-s",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      handle_shift_direction(defines.direction.south, event)
   end
)

EventManager.on_event(
   "fa-s-d",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      handle_shift_direction(defines.direction.east, event)
   end
)

EventManager.on_event("fa-c-w", handle_ctrl_direction)
EventManager.on_event("fa-c-a", handle_ctrl_direction)
EventManager.on_event("fa-c-s", handle_ctrl_direction)
EventManager.on_event("fa-c-d", handle_ctrl_direction)

-- Ctrl+shift+wasd force-fires capsules even in safe mode
EventManager.on_event(
   "fa-cs-w",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      if Combat.is_combat_mode(pindex) and Capsules.get_held_capsule_data(pindex) then
         Capsules.use_capsule_in_direction(pindex, defines.direction.north, true)
      end
   end
)

EventManager.on_event(
   "fa-cs-a",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      if Combat.is_combat_mode(pindex) and Capsules.get_held_capsule_data(pindex) then
         Capsules.use_capsule_in_direction(pindex, defines.direction.west, true)
      end
   end
)

EventManager.on_event(
   "fa-cs-s",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      if Combat.is_combat_mode(pindex) and Capsules.get_held_capsule_data(pindex) then
         Capsules.use_capsule_in_direction(pindex, defines.direction.south, true)
      end
   end
)

EventManager.on_event(
   "fa-cs-d",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      if Combat.is_combat_mode(pindex) and Capsules.get_held_capsule_data(pindex) then
         Capsules.use_capsule_in_direction(pindex, defines.direction.east, true)
      end
   end
)

--Read the current co-ordinates of the cursor on the map or in a menu. For crafting recipe and technology menus, it reads the ingredients / requirements instead.
--Todo: split this function by menu.
local function read_coords(pindex, start_phrase)
   start_phrase = start_phrase or ""
   local result = start_phrase
   local ent = storage.players[pindex].building.ent
   local offset = 0

   local router = UiRouter.get_router(pindex)

   local position = Cursor.tile(pindex)
   local marked_pos = { x = position.x, y = position.y }

   if game.get_player(pindex).driving then
      --Give vehicle coords and orientation and speed --laterdo find exact speed coefficient
      local vehicle = game.get_player(pindex).vehicle
      assert(vehicle ~= nil) -- When driving is true, vehicle is guaranteed to exist
      local speed = vehicle.speed * 215
      local message = MessageBuilder.new()

      if start_phrase then message:fragment(start_phrase) end

      if vehicle.type ~= "spider-vehicle" then
         if speed > 0 then
            message:fragment({
               "fa.vehicle-heading",
               Localising.get_localised_name_with_fallback(vehicle),
               FaUtils.get_heading_info(vehicle),
               tostring(math.floor(speed)),
            })
         elseif speed < 0 then
            message:fragment({
               "fa.vehicle-reversing",
               Localising.get_localised_name_with_fallback(vehicle),
               FaUtils.get_heading_info(vehicle),
               tostring(math.floor(-speed)),
            })
         else
            message:fragment({
               "fa.vehicle-parked",
               Localising.get_localised_name_with_fallback(vehicle),
               FaUtils.get_heading_info(vehicle),
            })
         end
      else
         message:fragment({
            "fa.vehicle-spider-moving",
            Localising.get_localised_name_with_fallback(vehicle),
            tostring(math.floor(speed)),
         })
      end

      message:fragment({
         "fa.vehicle-position-in",
         Localising.get_localised_name_with_fallback(vehicle),
         tostring(math.floor(vehicle.position.x)),
         tostring(math.floor(vehicle.position.y)),
      })

      Speech.speak(pindex, message:build())
   else
      --Simply give coords
      local location = FaUtils.get_entity_part_at_cursor(pindex)
      local message = MessageBuilder.new()

      if start_phrase then message:fragment(start_phrase) end

      if location and location ~= " " then
         message:fragment({
            "fa.coordinates-at-with-location",
            "",
            location,
            tostring(math.floor(marked_pos.x)),
            tostring(math.floor(marked_pos.y)),
         })
      else
         message:fragment({
            "fa.coordinates-at",
            "",
            tostring(math.floor(marked_pos.x)),
            tostring(math.floor(marked_pos.y)),
         })
      end

      --Draw the point
      rendering.draw_circle({
         color = { 1.0, 0.2, 0.0 },
         radius = 0.1,
         width = 5,
         target = marked_pos,
         surface = game.get_player(pindex).surface,
         time_to_live = 180,
      })

      --If there is a build preview, give its dimensions and which way they extend
      local stack = game.get_player(pindex).cursor_stack
      local p_width, p_height = BuildDimensions.get_stack_build_dimensions(stack, NativeCursor.build_direction(pindex))

      --While planning rails, the route the game draws to the cursor
      local rail_route = NativeCursor.rail_route(pindex)
      if rail_route then
         message:list_item(RailRoute.describe(rail_route))
      -- Tiles are always 1x1, so tile case still triggers.
      elseif p_width and p_height and (p_width > 1 or p_height > 1) then
         message:fragment({ "fa.build-preview-dimensions", tostring(p_width), tostring(p_height) })
      end
      Speech.speak(pindex, message:build())
   end
end

--Read coordinates of the cursor. Extra info as well such as entity part if an entity is selected, and heading and speed info for vehicles.
EventManager.on_event(
   "fa-k",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      -- Check for virtual train driving (its status command reads no tile)
      if VirtualTrainDriving.on_kb_descriptive_action_name(event) then return end

      read_coords(pindex)
   end
)

--Get distance and direction of cursor from player.
---@param event EventData.CustomInputEvent
local function kb_read_cursor_distance_and_direction(event)
   local pindex = event.player_index
   local p = game.get_player(pindex)
   local cursor_pos = Cursor.tile(pindex)
   --Read where the cursor is with respect to the player, e.g. "at 5 west"
   local dir_dist = FaUtils.dir_dist_locale(p.position, cursor_pos)
   local cursor_location_description = { "fa.at" }
   local cursor_production = " "
   local cursor_description_of = " "
   local result = { "fa.thing-producing-listpos-dirdist", cursor_location_description }
   table.insert(result, cursor_production) --no production
   table.insert(result, cursor_description_of) --listpos
   table.insert(result, dir_dist)
   -- Spoken by the native console reader
   p.print(result, { volume_modifier = 0 })
   --Draw the point
   rendering.draw_circle({
      color = { 1, 0.2, 0 },
      radius = 0.1,
      width = 5,
      target = cursor_pos,
      surface = p.surface,
      time_to_live = 180,
   })
end

EventManager.on_event(
   "fa-s-k",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      kb_read_cursor_distance_and_direction(event)
   end
)

--Get distance and direction of cursor from player as a vector with a horizontal component and vertical component.
---@param event EventData.CustomInputEvent
local function kb_read_cursor_distance_vector(event)
   local pindex = event.player_index
   local p = game.get_player(pindex)
   local c_pos = Cursor.tile(pindex)
   local p_pos = p.position
   local diff_x = math.floor(c_pos.x) - math.floor(p_pos.x)
   local diff_y = math.floor(c_pos.y) - math.floor(p_pos.y)

   ---@type defines.direction
   local dir_x = dirs.east

   if diff_x < 0 then dir_x = dirs.west end

   ---@type defines.direction
   local dir_y = dirs.south

   if diff_y < 0 then dir_y = dirs.north end
   local result = "At "
      .. math.abs(diff_x)
      .. " "
      .. FaUtils.direction_lookup(dir_x)
      .. " and "
      .. math.abs(diff_y)
      .. " "
      .. FaUtils.direction_lookup(dir_y)
   -- Spoken by the native console reader
   p.print(result, { volume_modifier = 0 })
   --Show cursor position
   rendering.draw_circle({
      color = { 1, 0.2, 0 },
      radius = 0.1,
      width = 5,
      target = c_pos,
      surface = p.surface,
      time_to_live = 180,
   })
end

EventManager.on_event(
   "fa-a-k",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      local router = UiRouter.get_router(pindex)

      kb_read_cursor_distance_vector(event)
   end
)

local function kb_read_character_coords(event)
   local pindex = event.player_index
   local pos = game.get_player(pindex).position
   local result = "Character at " .. math.floor(pos.x) .. ", " .. math.floor(pos.y)
   --Floored, then with extra precision; spoken by the native console reader
   game.get_player(pindex).print(
      result .. "\n (" .. math.floor(pos.x * 10) / 10 .. ", " .. math.floor(pos.y * 10) / 10 .. ")",
      { volume_modifier = 0 }
   )
end

EventManager.on_event(
   "fa-a-j",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      kb_read_character_coords(event)
   end
)

--Teleports the cursor to the player character
---@param event EventData.CustomInputEvent
local function kb_jump_to_player(event)
   local pindex = event.player_index
   -- The character, wherever remote view's camera is
   Cursor.jump(pindex, game.get_player(pindex).physical_position)
   read_coords(pindex, "Cursor returned ")
end

---@param event EventData.CustomInputEvent
local function kb_read_driving_structure_ahead(event)
   local pindex = event.player_index
   local p = game.get_player(pindex)
   local ent = storage.players[pindex].last_driving_alert_ent
   if ent and ent.valid then
      local dir = FaUtils.get_heading_value(p.vehicle)
      local dir_ent = FaUtils.get_direction_biased(ent.position, p.vehicle.position)
      if p.vehicle.speed >= 0 and (dir_ent == dir or math.abs(dir_ent - dir) == 1 or math.abs(dir_ent - dir) == 15) then
         local dist = math.floor(util.distance(p.vehicle.position, ent.position))
         Speech.speak(
            pindex,
            { "fa.driving-structure-ahead", Localising.get_localised_name_with_fallback(ent), tostring(dist) }
         )
      elseif p.vehicle.speed <= 0 and dir_ent == FaUtils.rotate_180(dir) then
         local dist = math.floor(util.distance(p.vehicle.position, ent.position))
         Speech.speak(
            pindex,
            { "fa.driving-structure-behind", Localising.get_localised_name_with_fallback(ent), tostring(dist) }
         )
      end
   end
end

EventManager.on_event(
   "fa-j",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      local router = UiRouter.get_router(pindex)
      local p = game.get_player(pindex)

      if Combat.is_combat_mode(pindex) then
         Speech.speak(pindex, { "fa.not-available-in-combat-mode" })
      else
         kb_jump_to_player(event)
      end
   end
)

-- The virtual train's bookmark stack: Shift+B pushes, Alt+B pops. B is the game's blueprint library.
EventManager.on_event(
   "fa-s-b",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      VirtualTrainDriving.on_kb_descriptive_action_name(event)
   end
)

EventManager.on_event(
   "fa-a-b",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      if not VirtualTrainDriving.is_locked(pindex) then return end
      if VirtualTrainDriving.return_to_bookmark(pindex) then Cursor.read(pindex) end
   end
)

EventManager.on_event(
   "fa-ca-b",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      Rulers.give_tool(pindex)
   end
)

EventManager.on_event(
   defines.events.on_lua_shortcut,
   ---@param event EventData.on_lua_shortcut
   function(event, pindex)
      if event.prototype_name == Rulers.TOOL then Rulers.give_tool(pindex) end
   end
)

EventManager.on_event(
   "fa-audio-ruler-place",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      Rulers.place(pindex)
   end
)

EventManager.on_event(
   "fa-audio-ruler-remove",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      Rulers.remove(pindex)
   end
)

EventManager.on_event(
   "fa-cas-b",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      local p = game.get_player(pindex)
      if p.is_cursor_empty then p.cursor_stack.set_stack("blueprint-book") end
   end
)

EventManager.on_event(
   "fa-s-c",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      UiRouter.get_router(pindex):open_ui(UiRouter.UI_NAMES.CURSOR_COORDINATE_INPUT)
   end
)

---Toggles cursor mode on or off. Appropriately affects other modes such as build lock or remote view.
---@param pindex integer
---@param muted boolean
local function toggle_cursor_mode(pindex, muted)
   local vp = Viewpoint.get_viewpoint(pindex)
   if not vp:get_cursor_anchored() then
      --Enable
      vp:set_cursor_anchored(true)

      --Finally, read the new tile
      Speech.speak(pindex, { "fa.anchored-cursor" })
   else
      --Finally, read the new tile
      vp:set_cursor_anchored(false)
      -- For the unanchored case it's worth reading the tile the cursor ended up on.
      if muted ~= true then Cursor.read(pindex, { "fa.unanchored-cursor" }) end
   end
end

EventManager.on_event(
   "fa-i",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      if skip_in_combat_mode(pindex) then return end
      if Hand.holds_build(pindex) then
         NativeCursor.toggle_detach(pindex)
         return
      end
      toggle_cursor_mode(pindex, false)
   end
)

EventManager.on_event(
   "fa-a-i",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      -- For remote view toggle, but remote view is currently not working in Factorio 2.0.
   end
)

EventManager.on_event(
   "fa-pageup",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      local router = UiRouter.get_router(pindex)
      local p = game.get_player(pindex)
      local ent = p.opened

      if ent and ent.type == "inserter" then
         -- TODO: Move to capability-based UI
         -- Temporarily inline the functionality
         ent.inserter_stack_size_override = ent.inserter_stack_size_override + 1
         local result = ent.inserter_stack_size_override .. " set for hand stack size"
         Speech.speak(pindex, result)
      else
         ScannerEntrypoint.move(pindex, event)
      end
   end
)

EventManager.on_event(
   "fa-s-pageup",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      ScannerEntrypoint.move(pindex, event)
   end
)

EventManager.on_event(
   "fa-c-pageup",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      ScannerEntrypoint.move_category(pindex)
   end
)

EventManager.on_event(
   "fa-pagedown",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      local router = UiRouter.get_router(pindex)
      local p = game.get_player(pindex)
      local ent = p.opened

      if ent and ent.type == "inserter" then
         -- TODO: Move to capability-based UI
         -- Temporarily inline the functionality
         local result = ""
         if ent.inserter_stack_size_override > 1 then
            ent.inserter_stack_size_override = ent.inserter_stack_size_override - 1
            result = ent.inserter_stack_size_override .. " set for hand stack size"
         else
            ent.inserter_stack_size_override = 0
            local cap = ent.force.inserter_stack_size_bonus + 1
            if ent.name == "stack-inserter" or ent.name == "stack-filter-inserter" then
               cap = ent.force.stack_inserter_capacity_bonus + 1
            end
            result = "restored " .. cap .. " as default hand stack size "
         end
         Speech.speak(pindex, result)
      else
         ScannerEntrypoint.move(pindex, event)
      end
   end
)

EventManager.on_event(
   "fa-s-pagedown",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      ScannerEntrypoint.move(pindex, event)
   end
)

EventManager.on_event(
   "fa-c-pagedown",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      ScannerEntrypoint.move_category(pindex)
   end
)

EventManager.on_event(
   "fa-home",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      ScannerEntrypoint.jump(pindex, event)
   end
)

EventManager.on_event(
   "fa-end",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      ScannerEntrypoint.do_refresh(pindex)
   end
)

EventManager.on_event("fa-s-end", function(event)
   local player = game.get_player(event.player_index)
   local char = player.character
   if not char then return end
   ScannerEntrypoint.do_refresh(event.player_index, char.direction)
end)

EventManager.on_event("fa-s-slash", function(event)
   local pindex = event.player_index
   local player = game.get_player(pindex)

   local help_items = {}

   -- Add hand status message
   local cursor_stack = player.cursor_stack
   if cursor_stack and cursor_stack.valid_for_read then
      local item_name = Localising.get_localised_name_with_fallback(cursor_stack.prototype)
      local msg = MessageBuilder.new():fragment({ "fa.hand-contains", item_name }):build()
      table.insert(help_items, Help.message(msg))
   else
      table.insert(help_items, Help.message({ "fa.hand-empty" }))
   end

   -- If hand has item, check for prototype-specific help
   if cursor_stack and cursor_stack.valid_for_read then
      local prototype_type = cursor_stack.prototype.type
      local help_list_name = prototype_type .. "-protohelp"
      if MessageLists.has_list(help_list_name) then table.insert(help_items, Help.message_list(help_list_name)) end
   end

   -- Add map help
   table.insert(help_items, Help.message_list("map-help"))

   -- Open help UI
   local router = UiRouter.get_router(pindex)
   local help_params = Help.create_parameters(help_items)
   router:open_ui(UiRouter.UI_NAMES.HELP, help_params)
end)

EventManager.on_event(
   "fa-t",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      Speech.speak(pindex, Daytime.describe(game.get_player(pindex).surface))
   end
)

-- Prototype lister (Alt+P)
EventManager.on_event("fa-a-p", function(event)
   local pindex = event.player_index
   local router = UiRouter.get_router(pindex)
   router:open_ui(UiRouter.UI_NAMES.PROTOTYPE_LISTER, {})
end)

-- Circuit/copper network neighbors (N key)
EventManager.on_event(
   "fa-n",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      local player = game.get_player(pindex)
      local ent = player.selected

      if ent and ent.valid then
         -- Check for combinators (multi-connection circuit entities)
         if
            ent.type == "arithmetic-combinator"
            or ent.type == "decider-combinator"
            or ent.type == "selector-combinator"
         then
            local msg = CircuitNetworks.get_combinator_neighbors_info(ent, pindex)
            Speech.speak(pindex, msg)
            return
         end

         -- Check for power switch (multi-connection entity with both copper and circuit)
         if ent.type == "power-switch" then
            local msg = CircuitNetworks.get_power_switch_neighbors_info(ent, pindex)
            Speech.speak(pindex, msg)
            return
         end

         -- Check for electric pole (copper wires)
         if ent.type == "electric-pole" then
            local msg = CircuitNetworks.get_copper_wire_neighbors_info(ent, pindex)
            Speech.speak(pindex, msg)
            return
         end

         -- Check if entity has circuit network capability
         local cb = ent.get_control_behavior()
         if cb then
            local msg = CircuitNetworks.get_circuit_neighbors_info(ent, pindex)
            Speech.speak(pindex, msg)
            return
         end
      end

      Speech.speak(pindex, { "", "Not in a network" })
   end
)

-- Remove wires (Alt+N key)
EventManager.on_event(
   "fa-a-n",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      CircuitNetworks.remove_wires(pindex)
   end
)

-- Circuit network navigator (Ctrl+Alt+N key)
EventManager.on_event(
   "fa-c-n",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      local player = game.get_player(pindex)
      local ent = player.selected

      -- Check if entity has circuit network capability
      if ent and ent.valid then
         local cb = ent.get_control_behavior()
         if cb then
            UiRouter.get_router(pindex):open_ui(UiRouter.UI_NAMES.CIRCUIT_NAVIGATOR, { entity = ent })
            return
         end
      end

      Speech.speak(pindex, { "", "No circuit network capable entity selected" })
   end
)

-- In FA's own menus the UI router takes Ctrl+Tab for their sections. Otherwise it moves to the parts
-- of the screen the native DLL reads, such as the quickbar.
---@param direction integer
local function next_part(direction)
   ---@param pindex integer
   return function(_, pindex)
      if UiRouter.get_router(pindex):is_ui_open() then return end
      NativeCursor.next_part(pindex, direction)
   end
end

EventManager.on_event("fa-c-tab", next_part(1))
EventManager.on_event("fa-cs-tab", next_part(-1))

EventManager.on_event(
   "fa-a-w",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      WorldMenu.open_world_menu(pindex)
   end
)

-- Now handled by router.lua
---@param event EventData
local function kb_read_menu_name(event)
   ---@cast event EventData.CustomInputEvent
   local pindex = event.player_index

   Speech.speak(pindex, { "fa.not-in-menu" })
end

EventManager.on_event("fa-s-e", function(event) --read_menu_name
   local pindex = event.player_index
   kb_read_menu_name(event)
end)

---@param event EventData.CustomInputEvent
local function kb_delete(event)
   local pindex = event.player_index
   local router = UiRouter.get_router(pindex)
   local p = game.get_player(pindex)
   local hand = p.cursor_stack

   if hand and hand.valid_for_read then
      local is_planner = hand.is_blueprint
         or hand.is_blueprint_book
         or hand.is_deconstruction_item
         or hand.is_upgrade_item
      if is_planner then
         if FaUtils.confirm_action(pindex, hand.export_stack(), "Press again to delete the planner in hand.") then
            p.cursor_stack_temporary = true
            p.clear_cursor()
         end
      end
   end
end
EventManager.on_event(
   "fa-delete",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      kb_delete(event)
   end
)

---@param event EventData.CustomInputEvent
local function kb_mine_tiles(event)
   local pindex = event.player_index
   local p = game.get_player(pindex)
   local stack = p.cursor_stack
   if not (stack and stack.valid_for_read and stack.valid and stack.prototype.place_as_tile_result) then return end

   local c_pos = Cursor.tile(pindex)
   local left_top = { x = c_pos.x, y = c_pos.y }
   local right_bottom = { x = c_pos.x + 1, y = c_pos.y + 1 }
   local tiles = p.surface.find_tiles_filtered({ area = { left_top, right_bottom } })

   for _, tile in ipairs(tiles) do
      if p.mine_tile(tile) then sounds.play_entity_mined(p.index, "stone-furnace") end
   end
end

---@param event EventData.CustomInputEvent
local function kb_mine_access_sounds(event)
   local pindex = event.player_index
   local p = game.get_player(pindex)
   local ent = p.selected
   if ent and ent.valid and ent.prototype.mineable_properties.products and ent.type ~= "resource" then
      sounds.play_mine(p.index)
   elseif ent and ent.valid and ent.name == "character-corpse" then
      Speech.speak(pindex, { "fa.collecting-items" })
   end
end

EventManager.on_event(
   "fa-x",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      local router = UiRouter.get_router(pindex)

      local p = game.get_player(pindex)
      local stack = p.cursor_stack
      if stack and stack.valid_for_read and stack.valid and stack.prototype.place_as_tile_result then
         kb_mine_tiles(event)
      end
      kb_mine_access_sounds(event)
   end
)

--Area mining for obstacles, trees, rocks, etc.
EventManager.on_event(
   "fa-s-x",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      local router = UiRouter.get_router(pindex)

      AreaOperations.mine_area(pindex)
   end
)

--Mines groups of entities depending on the name or type. Includes trees and rocks.

EventManager.on_event(
   "fa-cs-x",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      local router = UiRouter.get_router(pindex)

      local ent = game.get_player(pindex).selected
      if ent and ent.valid then AreaOperations.super_mine_area(pindex) end
   end
)

EventManager.on_event(
   "fa-a-l",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      WorkerRobots.logistics_request_toggle_handler(pindex)
   end
)

EventManager.on_event(
   "fa-a-f",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      WorkerRobots.announce_robot_dispatch_status(pindex)
   end
)

---@param event EventData.CustomInputEvent
---@param ent LuaEntity
local function kb_launch_rocket(event, ent)
   local pindex = event.player_index
   local cargo_inventory = ent.get_inventory(defines.inventory.rocket_silo_rocket)

   local destination
   if cargo_inventory and not cargo_inventory.is_empty() then
      -- Cargo present, need a landing pad
      local landing_pads = ent.surface.find_entities_filtered({ type = "cargo-landing-pad" })
      if #landing_pads == 0 then
         Speech.speak(pindex, { "fa.no-cargo-landing-pad" })
         return
      end
      destination = {
         type = defines.cargo_destination.station,
         station = landing_pads[1],
         transform_launch_products = true,
      }
   else
      -- No cargo, launch to orbit
      destination = {
         type = defines.cargo_destination.orbit,
      }
   end

   local try_launch = ent.launch_rocket(destination)
   if try_launch then
      if destination.type == defines.cargo_destination.orbit then
         Speech.speak(pindex, { "fa.launch-to-orbit-successful" })
      else
         Speech.speak(pindex, { "fa.launch-successful" })
      end
   else
      Speech.speak(pindex, { "fa.not-ready-to-launch" })
   end
end

EventManager.on_event(
   "fa-s-enter",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      local p = game.get_player(pindex)
      local ent = p.selected

      if not ent or not ent.valid then return end

      if ent.type == "rocket-silo" then
         kb_launch_rocket(event, ent)
      elseif ent.type == "constant-combinator" then
         local cb = ent.get_control_behavior()
         ---@cast cb LuaConstantCombinatorControlBehavior?
         if cb then
            cb.enabled = not cb.enabled
            if cb.enabled then
               Speech.speak(pindex, { "fa.switched-on" })
            else
               Speech.speak(pindex, { "fa.switched-off" })
            end
         end
         return
      end

      if ent.type == "power-switch" then
         local cb = ent.get_control_behavior()
         ---@cast cb LuaGenericOnOffControlBehavior
         if cb and (cb.circuit_enable_disable or cb.connect_to_logistic_network) then
            Speech.speak(pindex, { "fa.power-switch-circuit-controlled" })
         else
            ent.power_switch_state = not ent.power_switch_state
            if ent.power_switch_state then
               Speech.speak(pindex, { "fa.switched-on" })
            else
               Speech.speak(pindex, { "fa.switched-off" })
            end
         end
         return
      end
   end
)

--Calls function to notify if items are being picked up via vanilla F key.
---@param event EventData.CustomInputEvent
local function kb_read_item_pickup_state(event)
   local pindex = event.player_index
   local router = UiRouter.get_router(pindex)

   local p = game.get_player(pindex)
   local result = ""
   local check_last_pickup = false
   local nearby_belts =
      p.surface.find_entities_filtered({ position = p.position, radius = 1.25, type = "transport-belt" })
   local nearby_ground_items =
      p.surface.find_entities_filtered({ position = p.position, radius = 1.25, name = "item-on-ground" })
   --Draw the pickup range
   rendering.draw_circle({
      color = { 0.3, 1, 0.3 },
      radius = 1.25,
      width = 1,
      target = p.position,
      surface = p.surface,
      time_to_live = 60,
      draw_on_ground = true,
   })
   --Check if there is a belt within n tiles
   if #nearby_belts > 0 then
      result = "Picking up "
      --Check contents being picked up
      local ent = nearby_belts[1]
      if ent == nil or not ent.valid then
         result = result .. " from nearby belts"
         Speech.speak(pindex, result)
         return
      end
      -- Line indices are exactly the integers defines.transport_line is defined as;
      -- carry the type without changing the value.
      ---@param index integer
      ---@return defines.transport_line
      local function tl(index)
         return index --[[@as defines.transport_line]]
      end
      local left = TH.nqc_to_sorted_descending(
         TH.rollup2(ent.get_transport_line(tl(1)).get_contents(), F.name().get, F.quality().get, F.count().get)
      )
      local right = TH.nqc_to_sorted_descending(
         TH.rollup2(ent.get_transport_line(tl(2)).get_contents(), F.name().get, F.quality().get, F.count().get)
      )
      local all = {}
      TH.concat_arrays(left, right)
      -- Rename it, for clarity.
      local all = left
      table.sort(all, function(a, b)
         return a.count > b.count
      end)

      if all[1] then result = result .. all[1].name end
      if all[2] then result = result .. " " .. string.format("and %s", all[2].name) end
      if all[3] then result = result .. " and others" end

      result = result .. " from nearby belts"
      --Check if there are ground items within n tiles
   elseif #nearby_ground_items > 0 then
      result = "Picking up "
      if nearby_ground_items[1] and nearby_ground_items[1].valid then
         result = result .. nearby_ground_items[1].stack.name
      end
      result = result .. " from ground, and possibly more items "
   else
      result = "No items within range to pick up"
   end
   Speech.speak(pindex, result)
end

EventManager.on_event(
   "fa-h",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      BuildingTools.flip_item_in_hand(event)
   end
)

EventManager.on_event(
   "fa-f",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      kb_read_item_pickup_state(event)
   end
)

EventManager.on_event(
   "fa-v",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      BuildingTools.flip_item_in_hand(event)
   end
)

EventManager.on_event(
   "fa-s-v",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      VehicleCycler.cycle_to_next_vehicle(pindex)
   end
)

EventManager.on_event(
   "fa-r",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      BuildingTools.rotate_item_in_hand(event)
   end
)

-- Shift+R: Counterclockwise rotation, or toggle spawners first in combat mode
EventManager.on_event(
   "fa-s-r",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      if Combat.is_combat_mode(pindex) then
         AimAssist.toggle_spawners_first(pindex)
      else
         BuildingTools.rotate_item_in_hand(event)
      end
   end
)

-- Ctrl+R: Toggle strongest/closest (aim assist)
EventManager.on_event(
   "fa-c-r",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      AimAssist.toggle_healthiest_first(pindex)
   end
)

-- Ctrl+Shift+R: Toggle safe mode (aim assist)
EventManager.on_event(
   "fa-cs-r",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      AimAssist.toggle_safe_mode(pindex)
   end
)

--Called when player rotates an entity on the map
EventManager.on_event(
   defines.events.on_player_rotated_entity,
   ---@param event EventData.on_player_rotated_entity
   function(event)
      BuildingTools.on_entity_rotated(event)
   end
)

--Called when player flips an entity on the map
EventManager.on_event(
   defines.events.on_player_flipped_entity,
   ---@param event EventData.on_player_flipped_entity
   function(event)
      BuildingTools.on_entity_flipped(event)
   end
)

--The game's own info panel for what the cursor points at, read by the DLL (the router handles
--FA's menus via on_read_info)
EventManager.on_event(
   "fa-y",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      if UiRouter.get_router(pindex):is_ui_open() then return end
      NativeCursor.open_selected_info(pindex)
   end
)

--Reads detailed item info for the item in hand
EventManager.on_event(
   "fa-s-y",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      ItemInfo.read_item_in_hand(pindex)
   end
)

--Read production statistics info for the selected item, in the hand or else selected in the inventory menu
EventManager.on_event(
   "fa-u",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      if game.get_player(pindex).driving then return end
      local str = FaInfo.selected_item_production_stats_info(pindex)
      Speech.speak(pindex, str)
   end
)

EventManager.on_event(
   "fa-s-u",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      FaInfo.read_pollution_level_at_position(Cursor.tile(pindex), pindex)
   end
)

EventManager.on_event("fa-cas-v", function(event, pindex)
   local router = UiRouter.get_router(pindex)
   local enabling = not VanillaMode.is_enabled(pindex)

   if enabling then
      -- Speak before toggle so speech still works
      Speech.speak(pindex, { "fa.vanilla-mode-enabled" })
      sounds.play_confirm(pindex)

      VanillaMode.toggle(pindex)

      -- Close any open UI
      router:close_ui()

      -- Exit combat mode if active
      if Combat.is_combat_mode(pindex) then Combat.toggle_combat_mode(pindex) end
   else
      VanillaMode.toggle(pindex)

      sounds.play_confirm(pindex)
      Speech.speak(pindex, { "fa.vanilla-mode-disabled" })
   end
end)

-- Q: Pipette tool
EventManager.on_event(
   "fa-q",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      CursorChanges.kb_pipette_tool(event)
   end
)

EventManager.on_event(
   "fa-s-q",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      read_hand(pindex)
   end
)

EventManager.on_event(
   "fa-p",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      local router = UiRouter.get_router(pindex)
      router:open_ui(UiRouter.UI_NAMES.WARNINGS)
   end
)

EventManager.on_event(
   "fa-s-p",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      FaInfo.read_nearest_damaged_ent_info(Cursor.tile(pindex), pindex)
   end
)

--Toggle whether rockets are launched automatically when they have cargo
EventManager.on_event(
   "fa-c-space",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      Speech.speak(pindex, { "fa.not-implemented-factorio-2" })
   end
)

--Help key and tutorial system DISABLED (tutorial content nonfunctional)
-- Tutorial system module and data left in place as dead code for future work

EventManager.on_event(
   "fa-l",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      local p = game.get_player(pindex)
      if p.character == nil then return end
      if p.driving == false then
         WorkerRobots.logistics_info_key_handler(pindex)
      else
         Driving.pda_read_assistant_toggled_info(pindex)
      end
   end
)

-- Reload mod scripts (control stage only)
-- NOTE: This does NOT reload:
-- - Localizations (locale/*.cfg files)
-- - Prototypes (data.lua, data-updates.lua, data-final-fixes.lua)
-- - Graphics/sounds
-- For a full reload including translations, Factorio must be restarted.
EventManager.on_event("fa-cas-r", function(event, pindex)
   Speech.speak(
      pindex,
      "Reloading mod scripts (control.lua). Note: This does not reload localizations or prototypes. For full reload including translations, restart Factorio."
   )
   game.reload_script()
end)

EventManager.on_event("fa-c-o", function(event)
   Speech.speak(
      event.player_index,
      "Type in the new cruise control speed and press 'ENTER' and then 'E' to confirm, or press 'ESC' to exit"
   )
end)

EventManager.on_event("fa-cas-d", function(event)
   local pindex = event.player_index
   local router = UiRouter.get_router(pindex)
   router:open_ui(UiRouter.UI_NAMES.DEBUG)
end)

-- Keys that toggle a per-player setting, the same one the game's Mod settings window shows
for key, setting in pairs({
   ["fa-cas-i"] = SETTING_NAMES.SONIFICATION_INSERTER,
   ["fa-cas-c"] = SETTING_NAMES.SONIFICATION_CRAFTING,
   ["fa-cas-e"] = SETTING_NAMES.SONIFICATION_COMBAT_ENEMIES,
   ["fa-cas-s"] = SETTING_NAMES.SONIFICATION_COMBAT_SPAWNERS,
}) do
   EventManager.on_event(key, function(event, pindex)
      local mod_settings = game.get_player(pindex).mod_settings
      local enabled = not mod_settings[setting].value
      mod_settings[setting] = { value = enabled }
      Speech.speak(
         pindex,
         { enabled and "fa.setting-enabled" or "fa.setting-disabled", { "mod-setting-name." .. setting } }
      )
   end)
end

-- Toggle combat mode (changes sound reference point from cursor to character)
EventManager.on_event("fa-cs-i", function(event)
   Combat.toggle_combat_mode(event.player_index)
end)

-- Blueprint book navigation at world level (when not in a menu)
---@param pindex integer
---@param offset integer 1 for next, -1 for previous
local function cycle_blueprint_book(pindex, offset)
   local player = game.get_player(pindex)
   if not player then return end

   local cursor_stack = player.cursor_stack
   if not cursor_stack or not cursor_stack.valid_for_read or not cursor_stack.is_blueprint_book then return end

   local book_inv = cursor_stack.get_inventory(defines.inventory.item_main)
   if not book_inv or #book_inv == 0 then
      Speech.speak(pindex, { "fa.blueprint-book-empty" })
      return
   end

   local current = cursor_stack.active_index or 1
   local new_idx = current + offset
   if new_idx < 1 then
      new_idx = #book_inv
   elseif new_idx > #book_inv then
      new_idx = 1
   end

   cursor_stack.active_index = new_idx
   local active_item = book_inv[new_idx]
   if active_item and active_item.valid_for_read then
      Speech.speak(pindex, { "", { "fa.blueprint-book-switched" }, " ", ItemInfo.item_info(active_item) })
   end
end

EventManager.on_event("fa-comma", function(event)
   -- Check for virtual train driving
   local handled, should_read = VirtualTrainDriving.on_kb_descriptive_action_name(event)
   if handled then
      if should_read then Cursor.read(event.player_index) end
      return
   end

   local pindex = event.player_index
   local player = game.get_player(pindex)
   if not player then return end

   local cursor_stack = player.cursor_stack
   if not cursor_stack or not cursor_stack.valid_for_read or not cursor_stack.is_blueprint_book then return end

   local book_inv = cursor_stack.get_inventory(defines.inventory.item_main)
   if not book_inv or #book_inv == 0 then
      Speech.speak(pindex, { "fa.blueprint-book-empty" })
      return
   end

   if not cursor_stack.active_index then
      Speech.speak(pindex, { "fa.blueprint-book-no-active" })
      return
   end

   local active_item = book_inv[cursor_stack.active_index]
   if active_item and active_item.valid_for_read then
      Speech.speak(pindex, { "", { "fa.blueprint-book-active" }, " ", ItemInfo.item_info(active_item) })
   else
      Speech.speak(pindex, { "fa.blueprint-book-empty-active" })
   end
end, EventManager.EVENT_KIND.WORLD)

EventManager.on_event("fa-m", function(event)
   -- Check for virtual train driving
   local handled, should_read = VirtualTrainDriving.on_kb_descriptive_action_name(event)
   if handled then
      if should_read then Cursor.read(event.player_index) end
      return
   end

   local pindex = event.player_index
   local player = game.get_player(pindex)
   local stack = player.cursor_stack

   -- Check if wire is in hand
   if stack and stack.valid_for_read then
      local wire_type = stack.name
      if wire_type == "red-wire" or wire_type == "green-wire" or wire_type == "copper-wire" then
         -- Wire in hand - select input/left side
         CircuitNetworks.drag_wire_and_read(pindex, "input")
         return
      end
   end

   cycle_blueprint_book(pindex, -1)
end, EventManager.EVENT_KIND.WORLD)

EventManager.on_event("fa-dot", function(event)
   -- Check for virtual train driving
   local handled, should_read = VirtualTrainDriving.on_kb_descriptive_action_name(event)
   if handled then
      if should_read then Cursor.read(event.player_index) end
      return
   end

   local pindex = event.player_index
   local player = game.get_player(pindex)
   local stack = player.cursor_stack

   -- Check if wire is in hand
   if stack and stack.valid_for_read then
      local wire_type = stack.name
      if wire_type == "red-wire" or wire_type == "green-wire" or wire_type == "copper-wire" then
         -- Wire in hand - select output/right side
         CircuitNetworks.drag_wire_and_read(pindex, "output")
         return
      end
   end

   cycle_blueprint_book(pindex, 1)
end, EventManager.EVENT_KIND.WORLD)

EventManager.on_event("fa-a-comma", function(event)
   -- Check for virtual train driving
   if VirtualTrainDriving.on_kb_descriptive_action_name(event) then return end
end, EventManager.EVENT_KIND.WORLD)

EventManager.on_event("fa-slash", function(event)
   -- Check for virtual train driving first
   if VirtualTrainDriving.on_kb_descriptive_action_name(event) then return end

   -- Otherwise, fall through to UI handling (handled by router)
end, EventManager.EVENT_KIND.WORLD)

-- Virtual train signal placement keybindings
EventManager.on_event("fa-c-m", function(event)
   local handled, should_read = VirtualTrainDriving.on_kb_descriptive_action_name(event)
   if handled then
      if should_read then Cursor.read(event.player_index) end
      return
   end
end, EventManager.EVENT_KIND.WORLD)

EventManager.on_event("fa-s-m", function(event)
   local handled, should_read = VirtualTrainDriving.on_kb_descriptive_action_name(event)
   if handled then
      if should_read then Cursor.read(event.player_index) end
      return
   end
end, EventManager.EVENT_KIND.WORLD)

EventManager.on_event("fa-c-dot", function(event)
   local handled, should_read = VirtualTrainDriving.on_kb_descriptive_action_name(event)
   if handled then
      if should_read then Cursor.read(event.player_index) end
      return
   end
end, EventManager.EVENT_KIND.WORLD)

EventManager.on_event("fa-s-dot", function(event)
   local handled, should_read = VirtualTrainDriving.on_kb_descriptive_action_name(event)
   if handled then
      if should_read then Cursor.read(event.player_index) end
      return
   end
end, EventManager.EVENT_KIND.WORLD)

-- Dangerous delete: Delete blueprint/decon/upgrade planner from hand
EventManager.on_event("fa-c-backspace", function(event)
   local pindex = event.player_index
   local player = game.get_player(pindex)
   if not player then return end

   local cursor_stack = player.cursor_stack
   if not cursor_stack or not cursor_stack.valid_for_read then
      Speech.speak(pindex, { "fa.dangerous-delete-nothing-to-delete" })
      return
   end

   -- Check if it's a planner item using API flags
   if
      cursor_stack.is_blueprint
      or cursor_stack.is_blueprint_book
      or cursor_stack.is_deconstruction_item
      or cursor_stack.is_upgrade_item
   then
      local item_description = ItemInfo.item_info({
         name = cursor_stack.name,
         count = cursor_stack.count,
         quality = cursor_stack.quality and cursor_stack.quality.name or nil,
      })

      -- Clear the cursor
      cursor_stack.clear()

      Speech.speak(pindex, { "fa.dangerous-delete-deleted", item_description })
   else
      Speech.speak(pindex, { "fa.dangerous-delete-not-planner" })
   end
end, EventManager.EVENT_KIND.WORLD)

EventManager.on_event("fa-backspace", function(event)
   -- Check for virtual train driving
   local handled, should_read = VirtualTrainDriving.on_kb_descriptive_action_name(event)
   if handled then
      if should_read then Cursor.read(event.player_index) end
      return
   end
end, EventManager.EVENT_KIND.WORLD)

EventManager.on_event("fa-c-l", function(event)
   local pindex = event.player_index
   local player = game.get_player(pindex)
   if not player then return end

   local entity = player.selected
   if not entity or not entity.valid then
      Speech.speak(pindex, { "fa.entity-invalid" })
      return
   end

   -- Check if entity has any logistics support
   local points = entity.get_logistic_point()
   if not points then
      Speech.speak(pindex, { "fa.entity-no-logistics" })
      return
   end

   local router = UiRouter.get_router(pindex)
   router:open_ui(UiRouter.UI_NAMES.LOGISTICS_CONFIG, { entity = entity })
end)

EventManager.on_event("fa-cs-l", function(event)
   local pindex = event.player_index
   local player = game.get_player(pindex)
   if not player or not player.character then return end

   local router = UiRouter.get_router(pindex)
   router:open_ui(UiRouter.UI_NAMES.LOGISTICS_CONFIG, { entity = player.character })
end)

EventManager.on_event(
   "fa-kk-cancel",
   ---@param event EventData.CustomInputEvent
   function(event, pindex)
      KruiseKontrol.on_toggle_driving(pindex)
   end
)

-- Send hand contents to trash: O key
EventManager.on_event("fa-o", function(event)
   local pindex = event.player_index
   local player = game.get_player(pindex)
   if not player or not player.character then return end

   local cursor_stack = player.cursor_stack
   if not cursor_stack or not cursor_stack.valid_for_read then
      Speech.speak(pindex, { "fa.trash-nothing-in-hand" })
      return
   end

   -- Get character's trash inventory
   local trash_inventory = InventoryUtils.find_trash_inventory(player.character)
   if not trash_inventory then
      Speech.speak(pindex, { "fa.trash-not-available" })
      return
   end

   -- Try to insert into trash
   local item_name = cursor_stack.name
   local item_count = cursor_stack.count
   local item_quality = cursor_stack.quality and cursor_stack.quality.name or nil

   local inserted = trash_inventory.insert({ name = item_name, count = item_count, quality = item_quality })

   if inserted > 0 then
      -- Remove from hand
      cursor_stack.count = cursor_stack.count - inserted

      -- Announce success
      local item_description = ItemInfo.item_info({
         name = item_name,
         count = inserted,
         quality = item_quality,
      })
      Speech.speak(pindex, { "fa.trash-sent-to-trash", item_description })

      if inserted < item_count then Speech.speak(pindex, { "fa.trash-full", tostring(item_count - inserted) }) end
   else
      Speech.speak(pindex, { "fa.trash-full-none-inserted" })
   end
end, EventManager.EVENT_KIND.WORLD)

-- Virtual train driving keys
-- TODO: Add these key definitions to data.lua
-- EventManager.on_event("fa-slash", function(event)
--    VirtualTrainDriving.on_kb_descriptive_action_name(event)
-- end, EventManager.EVENT_KIND.WORLD)

-- EventManager.on_event("fa-alt-comma", function(event)
--    VirtualTrainDriving.on_kb_descriptive_action_name(event)
-- end, EventManager.EVENT_KIND.WORLD)

-- EventManager.on_event("fa-shift-b", function(event)
--    VirtualTrainDriving.on_kb_descriptive_action_name(event)
-- end, EventManager.EVENT_KIND.WORLD)

-- EventManager.on_event("fa-ctrl-m", function(event)
--    VirtualTrainDriving.on_kb_descriptive_action_name(event)
-- end, EventManager.EVENT_KIND.WORLD)

-- EventManager.on_event("fa-ctrl-dot", function(event)
--    VirtualTrainDriving.on_kb_descriptive_action_name(event)
-- end, EventManager.EVENT_KIND.WORLD)

-- EventManager.on_event("fa-shift-m", function(event)
--    VirtualTrainDriving.on_kb_descriptive_action_name(event)
-- end, EventManager.EVENT_KIND.WORLD)

-- EventManager.on_event("fa-shift-dot", function(event)
--    VirtualTrainDriving.on_kb_descriptive_action_name(event)
-- end, EventManager.EVENT_KIND.WORLD)

EventManager.on_event("fa-a-z", function(event, pindex)
   local mb = MessageBuilder.new()

   -- Add zoom info
   Zoom.append_zoom_info(pindex, mb)

   -- Add weapon info
   local gun_name, gun_proto = PlayerWeapon.get_selected_gun(pindex)
   if gun_name and gun_proto then
      mb:list_item()
      mb:fragment(gun_proto.localised_name)

      local ammo_name, ammo_proto = PlayerWeapon.get_selected_ammo(pindex)
      if ammo_name and ammo_proto then mb:fragment({ "fa.zoom-weapon-with-ammo", ammo_proto.localised_name }) end

      local max_range = PlayerWeapon.get_max_range(pindex)
      local soft_min = PlayerWeapon.get_soft_min_range(pindex)

      if soft_min and max_range then
         mb:fragment({ "fa.zoom-weapon-range-with-min", soft_min, max_range })
      elseif max_range then
         mb:fragment({ "fa.zoom-weapon-range", max_range })
      end
   else
      mb:list_item({ "fa.zoom-no-weapon" })
   end

   Speech.speak(pindex, mb:build())
end, EventManager.EVENT_KIND.WORLD)
