--Here: Functions relating to circuit networks: pure functions for signal processing and aggregation.

local Localising = require("scripts.localising")
local ItemInfo = require("scripts.item-info")

local mod = {}

-- ============================================================================
-- Signal Processing and Aggregation
-- ============================================================================

---Get the prototype dictionary for a signal type
---@param signal_type string
---@return table<string, any>
local function get_prototype_dict(signal_type)
   if signal_type == "item" or not signal_type then
      return prototypes.item
   elseif signal_type == "fluid" then
      return prototypes.fluid
   elseif signal_type == "virtual" then
      return prototypes.virtual_signal
   elseif signal_type == "entity" then
      return prototypes.entity
   elseif signal_type == "recipe" then
      return prototypes.recipe
   elseif signal_type == "space-location" then
      return prototypes.space_location
   elseif signal_type == "asteroid-chunk" then
      return prototypes.asteroid_chunk
   elseif signal_type == "quality" then
      return prototypes.quality
   else
      return {}
   end
end

---Localize a SignalID to a LocalisedString (without count)
---@param signal_id SignalID
---@return LocalisedString
function mod.localise_signal(signal_id)
   if not signal_id or not signal_id.name then return { "", "empty" } end

   local signal_type = signal_id.type or "item"
   local proto_dict = get_prototype_dict(signal_type)
   local proto = proto_dict[signal_id.name]

   if not proto then return { "", signal_id.name } end

   local base_name
   -- For items and fluids, use the quality-aware localisation
   if signal_type == "item" or signal_type == "fluid" then
      base_name = ItemInfo.item_or_fluid_info({
         name = proto,
         quality = signal_id.quality,
      }, proto_dict)
   else
      -- For other signals, just get the localised name
      base_name = Localising.get_localised_name_with_fallback(proto)
   end

   -- Add type suffix for entity and recipe signals to disambiguate
   if signal_type == "entity" then
      return { "", base_name, " ", { "fa.signal-suffix-entity" } }
   elseif signal_type == "recipe" then
      return { "", base_name, " ", { "fa.signal-suffix-recipe" } }
   else
      return base_name
   end
end

---Localize a SignalID with count to a LocalisedString
---@param signal_id SignalID
---@param count number
---@return LocalisedString
function mod.localise_signal_with_count(signal_id, count)
   if not signal_id or not signal_id.name then return { "", "empty" } end

   local signal_type = signal_id.type or "item"
   local proto_dict = get_prototype_dict(signal_type)
   local proto = proto_dict[signal_id.name]

   if not proto then return { "", signal_id.name, " x ", tostring(count) } end

   -- For items and fluids, use the quality-aware localisation
   if signal_type == "item" or signal_type == "fluid" then
      return ItemInfo.item_or_fluid_info({
         name = proto,
         quality = signal_id.quality,
         count = count,
      }, proto_dict)
   else
      -- For other signals, just get name and append count
      local name = Localising.get_localised_name_with_fallback(proto)
      return { "", name, " x ", tostring(count) }
   end
end

---Localize a comparator symbol to a LocalisedString
---@param comparator ComparatorString? Comparator symbol (<, <=, =, !=, >=, >)
---@return LocalisedString
function mod.localise_comparator(comparator)
   local comp = comparator or "<"
   local key_map = {
      ["<"] = "lt",
      ["≤"] = "lte",
      ["<="] = "lte",
      ["="] = "eq",
      ["≠"] = "ne",
      ["!="] = "ne",
      ["≥"] = "gte",
      [">="] = "gte",
      [">"] = "gt",
   }
   local key = "fa.comparator-" .. (key_map[comp] or "lt")
   return { key }
end

---Get next comparator (cycle forward)
---@param current ComparatorString? Current comparator
---@return ComparatorString
function mod.get_next_comparison_operator(current)
   ---@type ComparatorString[]
   local comparators = { "<", "≤", "=", "≥", ">", "≠" }
   local curr = current or "<"
   for i, comp in ipairs(comparators) do
      if comp == curr then return comparators[(i % #comparators) + 1] end
   end
   return "<"
end

---Get previous comparator (cycle backward)
---@param current ComparatorString? Current comparator
---@return ComparatorString
function mod.get_prev_comparison_operator(current)
   ---@type ComparatorString[]
   local comparators = { "<", "≤", "=", "≥", ">", "≠" }
   local curr = current or "<"
   for i, comp in ipairs(comparators) do
      if comp == curr then
         local prev_idx = i - 1
         if prev_idx < 1 then prev_idx = #comparators end
         return comparators[prev_idx]
      end
   end
   return "<"
end

---@class fa.CircuitConditionReadOptions
---@field empty_message LocalisedString? Message to show when condition is unconfigured (defaults to generic message)

---Helper to get network text for display (red, green, or empty for both)
---@param networks CircuitNetworkSelection?
---@return LocalisedString
local function get_network_text(networks)
   if not networks then return "" end
   local red = networks.red ~= false
   local green = networks.green ~= false
   if red and not green then
      return { "fa.decider-red-network" }
   elseif green and not red then
      return { "fa.decider-green-network" }
   else
      return ""
   end
end

---Read a circuit condition into a MessageBuilder
---Unified function for reading CircuitCondition or CircuitConditionDefinition
---Network fields (first_signal_networks, second_signal_networks) are only present on DeciderCombinatorCondition
---@param mb fa.MessageBuilder MessageBuilder to write to
---@param condition CircuitCondition|CircuitConditionDefinition|DeciderCombinatorCondition|nil The condition to read
---@param options fa.CircuitConditionReadOptions? Optional configuration
function mod.read_condition(mb, condition, options)
   options = options or {}

   -- Handle unconfigured condition
   if not condition or not condition.first_signal or not condition.first_signal.name then
      mb:fragment(options.empty_message or { "fa.condition-unconfigured" })
      return
   end

   -- First signal with optional network (only on DeciderCombinatorCondition)
   mb:fragment(mod.localise_signal(condition.first_signal))
   if condition.first_signal_networks then mb:fragment(get_network_text(condition.first_signal_networks)) end

   -- Comparator
   mb:fragment(mod.localise_comparator(condition.comparator))

   -- Second signal or constant
   if condition.second_signal and condition.second_signal.name then
      mb:fragment(mod.localise_signal(condition.second_signal))
      if condition.second_signal_networks then mb:fragment(get_network_text(condition.second_signal_networks)) end
   else
      local constant = condition.constant or 0
      mb:fragment(tostring(constant))
   end
end

---Aggregate signals from a circuit network, summing across qualities
---Returns a nested table: type -> name -> quality -> count
---@param signals Signal[]?
---@return table<string, table<string, table<string, integer>>>
function mod.aggregate_signals(signals)
   if not signals then return {} end

   local aggregated = {}

   for _, signal in ipairs(signals) do
      local signal_type = signal.signal.type or "item"
      local signal_name = signal.signal.name
      local quality = signal.signal.quality or "normal"

      aggregated[signal_type] = aggregated[signal_type] or {}
      aggregated[signal_type][signal_name] = aggregated[signal_type][signal_name] or {}
      aggregated[signal_type][signal_name][quality] = (aggregated[signal_type][signal_name][quality] or 0)
         + signal.count
   end

   return aggregated
end

---Sort aggregated signals by total count (descending) then by name
---Unrolls the nested table into a flat sorted array
---@param aggregated table<string, table<string, table<string, integer>>>
---@return {signal_id: SignalID, total_count: integer, by_quality: table<string, integer>}[]
function mod.sort_aggregated_signals(aggregated)
   local sorted = {}

   for signal_type, names in pairs(aggregated) do
      for signal_name, qualities in pairs(names) do
         local total_count = 0
         for _, count in pairs(qualities) do
            total_count = total_count + count
         end

         table.insert(sorted, {
            signal_id = {
               name = signal_name,
               type = signal_type,
               quality = "normal",
            },
            total_count = total_count,
            by_quality = qualities,
         })
      end
   end

   table.sort(sorted, function(a, b)
      if a.total_count ~= b.total_count then return a.total_count > b.total_count end
      return a.signal_id.name < b.signal_id.name
   end)

   return sorted
end

---Get sorted quality names from a by_quality table
---@param by_quality table<string, number>
---@return string[]
function mod.get_sorted_quality_names(by_quality)
   local qualities = {}
   for quality_name, _ in pairs(by_quality) do
      table.insert(qualities, quality_name)
   end

   -- Sort by quality level
   table.sort(qualities, function(a, b)
      local qa = prototypes.quality[a]
      local qb = prototypes.quality[b]
      if not qa then return false end
      if not qb then return true end
      return qa.level < qb.level
   end)

   return qualities
end

---Get all signals from an entity's circuit networks
---Combines red and green if both_wires is true, otherwise uses specified wire
---@param entity LuaEntity
---@param both_wires boolean If true, combine red and green networks
---@param wire_connector_id defines.wire_connector_id? If both_wires is false, which wire to use
---@return Signal[]?
function mod.get_signals_from_entity(entity, both_wires, wire_connector_id)
   if not entity or not entity.valid then return nil end

   local all_signals = {}

   if both_wires then
      -- Combine signals from both red and green
      local red_network = entity.get_circuit_network(defines.wire_connector_id.circuit_red)
      local green_network = entity.get_circuit_network(defines.wire_connector_id.circuit_green)

      if red_network and red_network.signals then
         for _, signal in ipairs(red_network.signals) do
            table.insert(all_signals, signal)
         end
      end

      if green_network and green_network.signals then
         for _, signal in ipairs(green_network.signals) do
            table.insert(all_signals, signal)
         end
      end
   else
      -- Get signals from specified wire
      local network = entity.get_circuit_network(assert(wire_connector_id))

      if network and network.signals then all_signals = network.signals end
   end

   return #all_signals > 0 and all_signals or nil
end

---Get aggregated signals from an entity's circuit networks
---Returns signals already aggregated and sorted, ready for display
---@param entity LuaEntity
---@param both_wires boolean If true, combine red and green networks
---@param wire_connector_id defines.wire_connector_id? If both_wires is false, which wire to use
---@return {signal_id: SignalID, total_count: integer, by_quality: table<string, integer>}[]?
function mod.get_aggregated_signals_from_entity(entity, both_wires, wire_connector_id)
   local signals = mod.get_signals_from_entity(entity, both_wires, wire_connector_id)
   if not signals then return nil end

   local aggregated = mod.aggregate_signals(signals)
   return mod.sort_aggregated_signals(aggregated)
end

---Get network ID for display purposes
---@param entity LuaEntity
---@param wire_connector_id defines.wire_connector_id
---@return string
function mod.get_network_id_string(entity, wire_connector_id)
   if not entity or not entity.valid then return "nil" end

   local network = entity.get_circuit_network(wire_connector_id)

   if not network or not network.valid then return "nil" end

   return tostring(network.network_id)
end

---Get copper wire neighbor count for fa-info display (electric poles)
---@param entity LuaEntity
---@param msg fa.MessageBuilder Message builder to add info to
function mod.add_copper_wire_info(entity, msg)
   -- Count unique neighbors via copper wires
   local neighbor_count = 0
   local seen = {}

   local connectors = entity.get_wire_connectors(false)
   for _, connector in pairs(connectors) do
      if connector.wire_type == defines.wire_type.copper then
         for _, connection in pairs(connector.connections) do
            local other = connection.target.owner
            if other.valid and not seen[other.unit_number] then
               seen[other.unit_number] = true
               neighbor_count = neighbor_count + 1
            end
         end
      end
   end

   if neighbor_count > 0 then
      msg:list_item()
      msg:fragment({ "fa.copper-wire-neighbors", tostring(neighbor_count) })
   end
end

---Get all signals from a constant combinator
---Returns table of {signal=SignalID, count=number}
---@param entity LuaEntity
---@return table[] Array of signal info
function mod.get_constant_combinator_filters(entity)
   local cb = entity.get_control_behavior()
   ---@cast cb LuaConstantCombinatorControlBehavior
   local all_signals = {}

   for _, section in ipairs(cb.sections) do
      for i = 1, section.filters_count do
         local slot = section.get_slot(i)
         if slot.value then
            table.insert(all_signals, {
               signal = slot.value,
               count = slot.min,
            })
         end
      end
   end

   return all_signals
end

---Find all entities in a circuit network using BFS, sorted by hop distance
---@param start_entity LuaEntity The entity to start from
---@param wire_connector_id defines.wire_connector_id Which wire connector to follow
---@return table[] Array of {entity: LuaEntity, hops: number} sorted by hops
function mod.find_entities_in_network_by_hops(start_entity, wire_connector_id)
   local network = start_entity.get_circuit_network(wire_connector_id)
   if not network then return {} end

   local target_network_id = network.network_id
   local visited = {}
   local queue = { { entity = start_entity, hops = 0 } }
   local head = 1
   local results = {}

   while head <= #queue do
      local current = queue[head]
      head = head + 1

      local unit_number = current.entity.unit_number
      if not visited[unit_number] then
         visited[unit_number] = true
         table.insert(results, current)

         -- Find all neighbors via circuit wires
         local connectors = current.entity.get_wire_connectors(false)
         for _, connector in pairs(connectors) do
            if connector.wire_type == defines.wire_type.red or connector.wire_type == defines.wire_type.green then
               -- Check if this connector is in our target network
               if connector.network_id == target_network_id then
                  -- Add all connected entities to queue
                  for _, connection in pairs(connector.connections) do
                     local neighbor = connection.target.owner
                     if neighbor.valid and not visited[neighbor.unit_number] then
                        table.insert(queue, { entity = neighbor, hops = current.hops + 1 })
                     end
                  end
               end
            end
         end
      end
   end

   -- Sort by hops (already mostly sorted due to BFS, but ensure it)
   table.sort(results, function(a, b)
      if a.hops ~= b.hops then return a.hops < b.hops end
      return a.entity.unit_number < b.entity.unit_number
   end)

   return results
end

return mod
