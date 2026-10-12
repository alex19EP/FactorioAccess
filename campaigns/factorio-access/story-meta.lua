---@meta
-- Types for the game's core/lualib/story.lua, which campaign levels load with require("story").
-- The lint's API library has no lualib, so the functions the levels use are declared here.

---A rectangle of ore a level creates, in tiles.
---@class (exact) fa.campaign.Patch
---@field name string
---@field left integer
---@field top integer
---@field width integer
---@field height integer

---@class fa.campaign.StoryNode
---@field name string?
---@field init (fun(event: EventData, story: table): boolean?)?
---@field update (fun(event: EventData, story: table))?
---@field condition (fun(event: EventData, story: table): boolean)?
---@field action (fun(event: EventData, story: table))?

---@param story_table fa.campaign.StoryNode[][]
function story_init_helpers(story_table) end

---@return table
function story_init() end

---@param story table
---@param event EventData
---@param next_level string?
---@param onwin (fun(): boolean)?
function story_update(story, event, next_level, onwin) end

---@param story table
---@param name string
function story_jump_to(story, name) end

---@param seconds number
---@return fun(event: EventData, story: table): boolean
function story_elapsed_check(seconds) end

---@param param LuaGameScript.show_message_dialog_param
---@param player LuaPlayer?
function story_show_message_dialog(param, player) end

---@param goal_string LocalisedString
---@param update_only boolean?
function set_goal(goal_string, update_only) end

---@param info table?
function set_info(info) end
