--[[
Saves from before the scanner moved into the DLL hold the old Lua scanner's state in storage: per
surface (storage.surfaces[i].scanner), per player (storage.players[i].scanner) and its work queues
(storage.work_queues). Those tables carry metatables registered under the names below. The names stay
registered, with empty metatables, so that such saves still load; drop_old_state then removes the
state itself.
]]
local mod = {}

script.register_metatable("fa.WorkQueue", {})
script.register_metatable("fa.ds.Deque", {})
script.register_metatable("fa.ds.SparseBitset", {})
script.register_metatable("fa.ds.TileClusterer", {})
script.register_metatable("fac1", {})
script.register_metatable("fa.scanner.ChartTagsBackend", {})
script.register_metatable("fa.scanner.IcebergBackend", {})
script.register_metatable("fa.scanner.PinsBackend", {})
script.register_metatable("fa.scanner.ResourceClusterer", {})
script.register_metatable("fa.scanner.ResourcePatches", {})
script.register_metatable("fa.scanner.WaterBackend", {})
script.register_metatable("fa.scanner.backends.TreeBackenmd", {})
script.register_metatable("fa.scanner.backends.CraftingMachine", {})
script.register_metatable("fa.scanner.backends.MiningDrill", {})
script.register_metatable("fa.scanner.backends.Furnace", {})
script.register_metatable("fa.scanner.backends.Vehicle", {})
script.register_metatable("fa.scanner.backends.Spidertron", {})
script.register_metatable("fa.scanner.backends.TrainsSimple", {})
script.register_metatable("fa.scanner.backends.TrainsNamed", {})
script.register_metatable("fa.scanner.backends.Ghosts", {})
script.register_metatable("fa.scanner.backends.Character", {})
script.register_metatable("fa.scanner.backends.Unit", {})
script.register_metatable("fa.scanner.backends.Spawner", {})
script.register_metatable("fa.scanner.backends.LogisticsAndPower", {})
script.register_metatable("fa.scanner.backends.Production", {})
script.register_metatable("fa.scanner.backends.Military", {})
script.register_metatable("fa.scanner.backends.Other", {})
script.register_metatable("fa.scanner.backends.Terrain", {})
script.register_metatable("fa.scanner.backends.Remnants", {})
script.register_metatable("fa.scanner.backends.Containers", {})
script.register_metatable("fa.scanner.backends.Corpses", {})
script.register_metatable("fa.scanner.backends.ResourceSingle", {})
script.register_metatable("fa.scanner.backends.LogisticsWithFluid", {})
script.register_metatable("fa.scanner.backends.Roboport", {})
script.register_metatable("fa.scanner.backends.Pipe", {})

-- Removes the old Lua scanner's state from storage.
function mod.drop_old_state()
   storage.work_queues = nil
   for _, root in ipairs({ "surfaces", "players" }) do
      for _, entry in pairs(storage[root] or {}) do
         entry.scanner = nil
      end
   end
   local versions = storage.storage_manager and storage.storage_manager.versions
   if versions and versions.surfaces then versions.surfaces.scanner = nil end
end

return mod
