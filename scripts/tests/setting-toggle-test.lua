--- Test the keys that toggle the per-player sonification settings

local EventManager = require("scripts.event-manager")
local SettingDecls = require("scripts.settings-decls")
local TestRegistry = require("scripts.test-registry")
local describe = TestRegistry.describe
local it = TestRegistry.it

local SETTING = SettingDecls.SETTING_NAMES.SONIFICATION_INSERTER

describe("Setting toggle keys", function()
   it("should toggle the player's own setting each press", function(ctx)
      local pindex = 1
      local mod_settings

      ctx:init(function()
         mod_settings = game.get_player(pindex).mod_settings
         mod_settings[SETTING] = { value = true }
      end)

      ctx:at_tick(1, function()
         EventManager.mock_event("fa-cas-i", { player_index = pindex })
         ctx:assert_equals(false, mod_settings[SETTING].value)
      end)

      ctx:at_tick(2, function()
         EventManager.mock_event("fa-cas-i", { player_index = pindex })
         ctx:assert_equals(true, mod_settings[SETTING].value)
      end)
   end)
end)
