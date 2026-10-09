--- Test the time of day read by T, from the surface's own dusk, evening, morning and dawn

local Daytime = require("scripts.daytime")
local TestRegistry = require("scripts.test-registry")
local describe = TestRegistry.describe
local it = TestRegistry.it

describe("Time of day", function()
   it("should name the part of the day the surface's daytime is in", function(ctx)
      local surface
      local saved

      ctx:init(function()
         surface = game.surfaces[1]
         saved = { freeze = surface.freeze_daytime, daytime = surface.daytime }
         surface.freeze_daytime = true
      end)

      ctx:at_tick(1, function()
         local function phase_at(daytime)
            surface.daytime = daytime
            return Daytime.describe(surface)[1]
         end

         ctx:assert_equals("fa.daytime-day", phase_at(0))
         ctx:assert_equals("fa.daytime-dusk", phase_at((surface.dusk + surface.evening) / 2))
         ctx:assert_equals("fa.daytime-night", phase_at(0.5))
         ctx:assert_equals("fa.daytime-dawn", phase_at((surface.morning + surface.dawn) / 2))
         ctx:assert_equals("fa.daytime-day", phase_at((surface.dawn + 1) / 2))

         surface.daytime = saved.daytime
         surface.freeze_daytime = saved.freeze
      end)
   end)
end)
