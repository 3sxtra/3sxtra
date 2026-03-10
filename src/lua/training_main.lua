-- training_main.lua
-- Bootstrap script for the 3SX training mode Lua layer.
-- Sets up FBNeo globals, provides io/os stubs, loads the engine gamestate
-- adapter, and loads effie's full control tree (prediction + dummy_control).

-- ============================================================
-- 1. FBNeo Built-in Globals
-- ============================================================

--- Deep copy a table (FBNeo built-in, used extensively by effie modules)
function copytable(t)
   if type(t) ~= "table" then return t end
   local copy = {}
   for k, v in pairs(t) do
      copy[k] = copytable(v)
   end
   return setmetatable(copy, getmetatable(t))
end

--- Bitwise compat (Lua 5.4 uses native operators; FBNeo Lua uses bit library)
bit = bit or {}
bit.bxor = bit.bxor or function(a, b) return a ~ b end
bit.band = bit.band or function(a, b) return a & b end
bit.bor  = bit.bor  or function(a, b) return a | b end
bit.lshift = bit.lshift or function(a, n) return a << n end
bit.rshift = bit.rshift or function(a, n) return a >> n end

--- Queue_Command (FBNeo global used by recording.lua for deferred actions)
function Queue_Command(frame, func)
   -- No-op in 3SX: we don't support deferred command queuing yet
end

-- ============================================================
-- 2. IO / OS Stubs (effie uses io.open for JSON settings files)
-- ============================================================

-- Always override io.open — the standard Lua io library resolves paths
-- from CWD, but our files live relative to the exe directory.
-- engine.read_file_text uses SDL_GetBasePath() for correct resolution.
if not io then io = {} end
local _original_io_open = io.open
io.open = function(filename, mode)
   -- Block settings/theme files — we use hardcoded defaults
   if string.find(filename, "settings%.json") or string.find(filename, "theme") then
      return nil
   end

   -- Route through our C++ engine file reader (uses SDL_GetBasePath)
   local text = engine.read_file_text(filename)
   if text then
      return {
         read = function(self, format) return text end,
         close = function(self) end,
      }
   end
   return nil, "file not found: " .. tostring(filename)
end
io.close = io.close or function(...) end

if not os then
   os = {
      clock = function() return 0 end,
      remove = function() return nil, "not supported" end,
      execute = function() end,
   }
end

-- ============================================================
-- 3. Load Engine Gamestate Adapter
-- ============================================================

local gamestate = require("compat.engine_gamestate")
_G.gamestate = gamestate

-- Register per-frame update so gamestate stays current
emu.registerbefore(function()
   if engine.is_in_match() then
      gamestate.update()
   end
end)

-- ============================================================
-- 4. Add Effie's Source Tree to Package Path
-- ============================================================

-- Intercept require("src.gamestate") so effie gets our engine adapter.
package.preload["src.gamestate"] = function() return gamestate end

-- Stub src.ui.menu (frame_advantage.lua checks is_open)
package.preload["src.ui.menu"] = function()
   return { is_open = false, allow_update_while_open = false }
end

-- Stub src.ui.hud (frame_advantage.lua calls display_frame_advantage_numbers)
-- We intercept this call and push the value to RmlUI instead.
local _fa_results = {}  -- [player_id] = signed_advantage_number
package.preload["src.ui.hud"] = function()
   return {
      display_frame_advantage_numbers = function(player, num)
         if player and player.id then
            _fa_results[player.id] = num
         end
      end
   }
end

-- Preload src.modules stub (defers heavy init to init() which we don't call)
package.preload["src.modules"] = function()
   return {
      training_mode_names = {"defense", "jumpins", "footsies", "unblockables", "geneijin"},
      extra_module_names = {"extra_settings", "key_bindings"},
      training_modules = {},
      extra_modules = {},
      all_modules = {},
      init = function() end,
      update = function() end,
      toggle = function() end,
      after_images_loaded = function() end,
      after_framedata_loaded = function() end,
      after_menu_created = function() end,
      get_module = function() return nil end,
   }
end

-- Preload src.settings with hardcoded defaults (bypasses JSON file I/O)
package.preload["src.settings"] = function()
   local game_data = require("src.data.game_data")
   local training_settings = {
      version = game_data.script_version,  -- "1.2.0" — prevents upgrade path
      counter_attack_delay = 0,
      replay_mode = 1,
      current_recording_slot = 1,
      recording_player_positioning = false,
      recording_dummy_positioning = false,
      auto_crop_recording_start = false,
      auto_crop_recording_end = false,
      training_mode_index = 1,
      modules_index = 1,
      blocking_direction = 1,
      language = 1,
   }
   local modules_settings = {}
   local recordings_settings = {}

   local settings_module = {
      saved_path = "saved/",
      data_path = "lua/data/",
      training_path = "src/training/",
      modules_path = "src/modules/",
      training_require_path = "src.training",
      modules_require_path = "src.modules",
      framedata_path = "lua/data/" .. game_data.rom_name .. "/framedata/",
      framedata_file_ext = "_framedata.json",
      framedata_bin_file = "framedata.msgpack",
      load_first_bin_file = "load_first.msgpack",
      text_bin_file = "text.msgpack",
      images_bin_file = "images.msgpack",
      recordings_path = "saved/recordings/",
      themes_path = "data/themes.json",
      load_training_data = function() end,
      save_training_data = function() end,
   }

   setmetatable(settings_module, {
      __index = function(_, key)
         if key == "training" then return training_settings
         elseif key == "modules" then return modules_settings
         elseif key == "recordings" then return recordings_settings
         elseif key == "counter_attack" then return training_settings.counter_attack
         elseif key == "language" then return "en"
         end
      end,
      __newindex = function(_, key, value)
         if key == "training" then training_settings = value
         elseif key == "modules" then modules_settings = value
         elseif key == "recordings" then recordings_settings = value
         elseif key == "counter_attack" then training_settings.counter_attack = value
         else rawset(settings_module, key, value)
         end
      end,
   })

   return settings_module
end

-- Preload src.training stub (dummy_control references training.player/dummy)
package.preload["src.training"] = function()
   local training = {
      player = nil,  -- Set at runtime when match starts
      dummy = nil,   -- Set at runtime when match starts
      swap_controls = function() end,
   }
   return training
end

-- Preload src.control.managers stub (recording references managers.Screen_Scroll)
package.preload["src.control.managers"] = function()
   return {
      Screen_Scroll = {
         stop_scroll = function() end,
         scroll_to_player_position = function() end,
         scroll_to_screen_position = function() end,
         scroll_to_center = function() end,
      },
   }
end

-- Preload all dependencies that will be used by loading.lua
pcall(require, "src.data.prediction")
pcall(require, "src.control.dummy_control")
pcall(require, "src.control.inputs")

-- Deferred framedata loading — called lazily on first training match entry
-- instead of at boot. Uses msgpack (fast binary) instead of JSON (~1.4s saved).
local loading = require("src.loading")
local framedata_load_attempted = false

local function ensure_framedata_loaded()
   if framedata_load_attempted then return end
   framedata_load_attempted = true

   local ok, err = pcall(function()
      local fd = require("src.data.framedata")
      local settings = require("src.settings")

      -- Load from msgpack binary (much faster than 22 individual JSON files)
      local bin_path = settings.framedata_path .. settings.framedata_bin_file
      loading.load_binary(fd.frame_data, bin_path)

      -- Apply character-specific fixups
      fd.patch_frame_data()
      fd.is_loaded = true

      local count = 0
      for _ in pairs(fd.frame_data) do count = count + 1 end
      print(string.format("[training_main] Framedata loaded from msgpack (%d characters)", count))
   end)
   if not ok then
      print("[training_main] ERROR loading framedata: " .. tostring(err))
   end
end

-- ============================================================
-- 5. Load Full Effie Module Tree
-- ============================================================

-- Prediction module (2085 LOC — hitbox simulation, frame data)
local prediction_ok, prediction = pcall(require, "src.data.prediction")
if prediction_ok then
   _G.prediction = prediction
   print("[training_main] Prediction module loaded successfully")
else
   print("[training_main] Prediction module not loaded: " .. tostring(prediction))
   _G.prediction = nil
end

-- Dummy control module (1107 LOC — blocking, parrying, mashing, counters)
local dc_ok, dummy_control = pcall(require, "src.control.dummy_control")
if dc_ok then
   _G.dummy_control = dummy_control
   print("[training_main] Dummy control module loaded successfully")
else
   print("[training_main] Dummy control not loaded: " .. tostring(dummy_control))
   _G.dummy_control = nil
end

-- Inputs module (for process_pending_input_sequence)
local inputs_ok, inputs_mod = pcall(require, "src.control.inputs")
if inputs_ok then
   print("[training_main] Inputs module loaded successfully")
else
   print("[training_main] Inputs module not loaded: " .. tostring(inputs_mod))
   inputs_mod = nil
end

-- ============================================================
-- 6. Per-Frame Dummy Control Wiring
-- ============================================================

-- FBNeo button name mapping for the input table
local FBNEO_BUTTONS = {
   "Up", "Down", "Left", "Right",
   "Weak Punch", "Medium Punch", "Strong Punch",
   "Weak Kick", "Medium Kick", "Strong Kick",
   "Start", "Coin"
}

--- Build an empty FBNeo-format input table
local function make_fbneo_input()
   local input = {}
   for _, btn in ipairs(FBNEO_BUTTONS) do
      input["P1 " .. btn] = false
      input["P2 " .. btn] = false
   end
   return input
end

--- Convert the FBNeo input table back to joypad.set() format for a player
local function fbneo_to_joypad(input, prefix)
   local result = {}
   if input[prefix .. " Up"]            then result.up    = true end
   if input[prefix .. " Down"]          then result.down  = true end
   if input[prefix .. " Left"]          then result.left  = true end
   if input[prefix .. " Right"]         then result.right = true end
   if input[prefix .. " Weak Punch"]    then result.LP    = true end
   if input[prefix .. " Medium Punch"]  then result.MP    = true end
   if input[prefix .. " Strong Punch"]  then result.HP    = true end
   if input[prefix .. " Weak Kick"]     then result.LK    = true end
   if input[prefix .. " Medium Kick"]   then result.MK    = true end
   if input[prefix .. " Strong Kick"]   then result.HK    = true end
   if input[prefix .. " Start"]         then result.start = true end
   if input[prefix .. " Coin"]          then result.coin  = true end
   return result
end

--- Initialize dummy state fields that dummy_control expects
local function init_dummy_state(player)
   if not player.blocking then
      player.blocking = {
         is_blocking = false,
         is_blocking_this_frame = false,
         blocked_hit_count = 0,
         last_parry_index = 0,
         last_block_type = 4,  -- Blocking_Type.NONE
         received_hit_count = 0,
         parried_last_frame = false,
         is_pre_parrying = false,
         pre_parry_frame = 0,
         last_block = { frame_number = 0 },
         tracked_attacks = {},
         force_block_start_frame = 0,
         expected_attacks = {},
      }
   end
   if not player.counter then
      player.counter = {
         is_counterattacking = false,
         is_awaiting_queue = false,
         counter_type = "reversal",
         attack_frame = -1,
         offset = 0,
         stun_queued = false,
         sequence = {},
         air_recovery = false,
      }
   end
   if not player.input_info then
      player.input_info = {
         last_back_input = 0,
         last_forward_input = 0,
         last_down_input = 0,
         last_up_input = 0,
      }
   end
end

local dummy_initialized = false
local dummy_player = nil
local dummy_id = 2

--- Detect which side the human is playing from Operator_Status.
--- Operator_Status[p] == 1 means a human operator is on that side.
--- Returns 1 or 2 for the human's side, defaults to 1 if ambiguous.
local function detect_local_player()
   local globals = engine.read_globals()
   local op1 = globals.operator_p1 or 0
   local op2 = globals.operator_p2 or 0

   -- If only one side has a human operator, that's the player
   if op1 == 1 and op2 ~= 1 then return 1 end
   if op2 == 1 and op1 ~= 1 then return 2 end

   -- Both operators active (vs mode) or neither:
   -- Default to P1 as the human side
   return 1
end

--- Map C DummySettings to Lua blocking_options, mash mode, fast wakeup mode,
--- and tech throw mode.  Called each frame so menu changes take effect live.
---
--- C enum to Lua enum mapping:
---   block_type: C{NONE=0,ALWAYS=1,FIRST_HIT=2,AFTER_FIRST_HIT=3,RANDOM=4}
---     → Lua Blocking_Mode{OFF=1,ON=2,FIRST_HIT=3,AFTER_FIRST_HIT=4,RANDOM=5}
---   parry_type: C{NONE=0,HIGH=1,LOW=2,ALL=3,RED=4}
---     → style: BLOCK=1, PARRY=2, RED_PARRY=3
---     (NONE means blocking is the style, with mode OFF handled via block_type)
---   stun_mash / wakeup_mash: C{NONE=0,FAST=1,NORMAL=2,RANDOM=3,LP=4,MP=5,HP=6}
---     → Lua mode: 1=OFF, 2=normal, 3=serious, 4=fastest
---     Map: 0→1(OFF), 1→4(fastest), 2→2(normal), 3→3(serious), 4-6→4(fastest)
---   tech_throw_type: C{NONE=0,ALWAYS=1,RANDOM=2}
---     → Lua mode: 1=OFF, 2=always, 3=random(50%)
---   fast_wakeup: C{NONE=0,ALWAYS=1,RANDOM=2}
---     → Lua mode: 1=OFF, 2=always, 3=random(50%)
local function map_dummy_settings()
   local s = engine.get_dummy_settings()

   -- ---- Blocking mode ----
   -- C block_type → Lua Blocking_Mode (1-indexed)
   local BLOCK_MAP = {
      [0] = 1, -- NONE            → OFF
      [1] = 2, -- ALWAYS          → ON
      [2] = 3, -- FIRST_HIT       → FIRST_HIT
      [3] = 4, -- AFTER_FIRST_HIT → AFTER_FIRST_HIT
      [4] = 5, -- RANDOM          → RANDOM
   }

   -- ---- Blocking style from parry_type ----
   -- C parry_type → Lua Blocking_Style
   --   NONE(0)  → BLOCK(1)
   --   HIGH(1)  → PARRY(2) + prefer_parry_low=false
   --   LOW(2)   → PARRY(2) + prefer_parry_low=true
   --   ALL(3)   → PARRY(2)
   --   RED(4)   → RED_PARRY(3)
   local blocking_mode = BLOCK_MAP[s.block_type] or 1
   local blocking_style = 1         -- BLOCK
   local prefer_parry_low = false
   local prefer_block_low = s.guard_low_default

   if s.parry_type == 1 then        -- HIGH
      blocking_style = 2            -- PARRY
   elseif s.parry_type == 2 then    -- LOW
      blocking_style = 2            -- PARRY
      prefer_parry_low = true
   elseif s.parry_type == 3 then    -- ALL
      blocking_style = 2            -- PARRY
   elseif s.parry_type == 4 then    -- RED
      blocking_style = 3            -- RED_PARRY
   end

   -- If parry is active but block mode was OFF, promote to ON so update_blocking runs
   if blocking_style >= 2 and blocking_mode == 1 then
      blocking_mode = 2  -- ON
   end

   local blocking_options = {
      mode = blocking_mode,
      style = blocking_style,
      prefer_block_low = prefer_block_low,
      prefer_parry_low = prefer_parry_low,
      force_blocking_direction = 1,  -- OFF (no menu item yet)
      red_parry_hit_count = 1,
      parry_every_n_count = 1,
   }

   -- ---- Mash modes ----
   -- C: NONE=0, FAST=1, NORMAL=2, RANDOM=3, LP=4, MP=5, HP=6
   -- Lua: 1=OFF, 2=normal, 3=serious, 4=fastest
   local MASH_MAP = {
      [0] = 1, -- NONE   → OFF
      [1] = 4, -- FAST   → fastest
      [2] = 2, -- NORMAL → normal
      [3] = 3, -- RANDOM → serious (closest match to randomized mashing)
      [4] = 4, -- LP     → fastest (specific-button mash at max speed)
      [5] = 4, -- MP     → fastest
      [6] = 4, -- HP     → fastest
   }
   local mash_mode = MASH_MAP[s.stun_mash] or 1

   -- ---- Fast wakeup ----
   -- C: NONE=0, ALWAYS=1, RANDOM=2
   -- Lua: 1=OFF, 2=always, 3=random(50%)
   local FAST_WAKEUP_MAP = {
      [0] = 1, -- NONE   → OFF
      [1] = 2, -- ALWAYS → always
      [2] = 3, -- RANDOM → random
   }
   local fast_wakeup_mode = FAST_WAKEUP_MAP[s.fast_wakeup] or 1

   -- ---- Tech throws ----
   -- C: NONE=0, ALWAYS=1, RANDOM=2
   -- Lua: 1=OFF, 2=always, 3=random(50%)
   local TECH_THROW_MAP = {
      [0] = 1, -- NONE   → OFF
      [1] = 2, -- ALWAYS → always
      [2] = 3, -- RANDOM → random
   }
   local tech_throw_mode = TECH_THROW_MAP[s.tech_throw_type] or 1

   return blocking_options, mash_mode, fast_wakeup_mode, tech_throw_mode
end

-- Register the dummy control update in the per-frame callback
emu.registerbefore(function()
   if not engine.is_in_match() then
      dummy_initialized = false
      dummy_player = nil
      return
   end
   if not dummy_control or not inputs_mod then return end

   -- Lazy-load framedata on first match entry (deferred from boot)
   ensure_framedata_loaded()

   -- Detect which side the human is playing and assign dummy
   if not dummy_initialized then
      local local_id = detect_local_player()

      if local_id == 1 then
         dummy_id = 2
         dummy_player = gamestate.P2
      else
         dummy_id = 1
         dummy_player = gamestate.P1
      end

      init_dummy_state(dummy_player)
      dummy_initialized = true
      print(string.format("[training_main] Operator_Status: P1=%d P2=%d → local=P%d, dummy=P%d",
         engine.read_globals().operator_p1 or -1,
         engine.read_globals().operator_p2 or -1,
         local_id, dummy_id))
   end

   -- Read C menu settings and map to Lua enum values
   local blocking_options, mash_mode, fast_wakeup_mode, tech_throw_mode = map_dummy_settings()

   -- Build FBNeo input table (start with all false)
   local input = make_fbneo_input()

   -- Run dummy control subsystems with live menu settings
   local ok, err = pcall(function()
      dummy_control.update_blocking(input, dummy_player, blocking_options)
      dummy_control.update_pose(input, dummy_player, 1)                      -- 1 = STANDING (pose handled by C menu natively)
      dummy_control.update_mash_inputs(input, dummy_player, mash_mode)
      dummy_control.update_fast_wake_up(input, dummy_player, fast_wakeup_mode)
      dummy_control.update_tech_throws(input, dummy_player, tech_throw_mode)

      -- Process any queued input sequences
      inputs_mod.process_pending_input_sequence(dummy_player, input)
   end)

   if not ok then
      -- Print error once, don't spam
      if not _G._dummy_error_printed then
         print("[training_main] Dummy control error: " .. tostring(err))
         _G._dummy_error_printed = true
      end
      return
   end

   -- Convert FBNeo input table to joypad format and apply to dummy
   local prefix = "P" .. dummy_id
   local dummy_input = fbneo_to_joypad(input, prefix)
   joypad.set(dummy_input, dummy_id)
end)

print("[training_main] Bootstrap complete")

-- ============================================================
-- 7. Load Frame-Advantage Tracker
-- ============================================================
-- frame_advantage.lua is self-contained (127 LOC) and requires only
-- gamestate + tools (both available).  It calls our src.ui.hud stub
-- when advantage is computed, which stores the value in _fa_results.

local frame_advantage = require("src.data.frame_advantage")

-- ============================================================
-- 8. Per-Frame HUD Text Push → RmlUI
-- ============================================================
-- Push numeric HUD data each frame from gamestate to the always-on
-- effie_hud.rml overlay via engine.set_hud_text().
-- This bypasses effie's draw.lua (which depends on FBNeo's GD pixel
-- system) and feeds real game data directly to RmlUI text elements.

emu.registerafter(function()
   if not engine.is_in_match() then return end
   if not gamestate or not gamestate.P1 then return end

   local p1 = gamestate.P1
   local p2 = gamestate.P2

   -- Life: "HP/160"
   engine.set_hud_text("p1_life", string.format("%d/160", p1.life or 0))
   engine.set_hud_text("p2_life", string.format("%d/160", p2.life or 0))

   -- Meter: "gauge/max"
   local p1_gauge = p1.meter_gauge or 0
   local p2_gauge = p2.meter_gauge or 0
   if p1.meter_count == p1.max_meter_count then p1_gauge = p1.max_meter_gauge or 0 end
   if p2.meter_count == p2.max_meter_count then p2_gauge = p2.max_meter_gauge or 0 end
   engine.set_hud_text("p1_meter", string.format("%d/%d", p1_gauge, p1.max_meter_gauge or 0))
   engine.set_hud_text("p2_meter", string.format("%d/%d", p2_gauge, p2.max_meter_gauge or 0))

   -- Stun: "stun/max"
   engine.set_hud_text("p1_stun", string.format("%d/%d",
      math.floor(p1.stun_bar or 0), p1.stun_bar_max or 64))
   engine.set_hud_text("p2_stun", string.format("%d/%d",
      math.floor(p2.stun_bar or 0), p2.stun_bar_max or 64))

   -- Frame advantage: run tracker + push results
   local fa_ok, fa_err = pcall(frame_advantage.update)
   if not fa_ok and not _G._fa_error_printed then
      print("[training_main] Frame advantage error: " .. tostring(fa_err))
      _G._fa_error_printed = true
   end

   for _, pid in ipairs({1, 2}) do
      local prefix = (pid == 1) and "p1" or "p2"
      local adv = frame_advantage.advantage and frame_advantage.advantage[pid]
      if adv and adv.advantage then
         local num = adv.advantage
         local sign = num > 0 and "+" or ""
         engine.set_hud_text(prefix .. "_advantage", string.format("%s%d", sign, num))
         if num > 0 then
            engine.set_hud_text("advantage_class", "positive")
         elseif num < 0 then
            engine.set_hud_text("advantage_class", "negative")
         else
            engine.set_hud_text("advantage_class", "neutral")
         end
      end
   end
end)

return gamestate

