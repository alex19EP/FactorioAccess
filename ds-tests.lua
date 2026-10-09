---@diagnostic disable
local lu = require("luaunit")

local tests = {
   { "fixed-ringbuffer", require("ds.tests.fixed-ringbuffer") },
}

local runner = lu.LuaUnit.new()
os.exit(runner:runSuiteByInstances(tests))
