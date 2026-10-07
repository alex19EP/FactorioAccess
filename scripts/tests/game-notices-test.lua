local TestRegistry = require("scripts.test-registry")
local describe, it = TestRegistry.describe, TestRegistry.it
local GameNotices = require("scripts.game-notices")

describe("Game notices", function()
   it("speaks the goal window once for each new text, and not when it closes", function(ctx)
      local player
      local first, repeated, changed, closed, reopened

      ctx:init(function()
         player = game.get_player(1)
         player.set_goal_description("", true)
         GameNotices.check_goal(1)
      end)

      ctx:at_tick(1, function()
         player.set_goal_description("Build a furnace", true)
         first = GameNotices.check_goal(1)
         repeated = GameNotices.check_goal(1)
         player.set_goal_description("Smelt iron", true)
         changed = GameNotices.check_goal(1)
         player.set_goal_description("", true)
         closed = GameNotices.check_goal(1)
         player.set_goal_description("Smelt iron", true)
         reopened = GameNotices.check_goal(1)
      end)

      ctx:at_tick(2, function()
         ctx:assert_table_equals({ "fa.notice-goal", "Build a furnace" }, first)
         ctx:assert_nil(repeated)
         ctx:assert_table_equals({ "fa.notice-goal", "Smelt iron" }, changed)
         ctx:assert_nil(closed)
         ctx:assert_table_equals({ "fa.notice-goal", "Smelt iron" }, reopened)
         player.set_goal_description("", true)
      end)
   end)
end)
