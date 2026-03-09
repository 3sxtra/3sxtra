-- engine_gamestate.lua
-- Drop-in replacement for effie's gamestate.lua using engine.read_player()
-- instead of FBNeo memory.read*() calls. Provides the same P1/P2 player
-- object interface that prediction.lua and other modules expect.

-- Character name table (matches game_data.characters, 0-indexed char_id + 1)
local characters = {
   "gill", "alex", "ryu", "yun", "dudley", "necro", "hugo", "ibuki",
   "elena", "oro", "yang", "ken", "sean", "urien", "gouki", "shingouki",
   "chunli", "makoto", "q", "twelve", "remy"
}

-- Module-level state
local frame_number = 0
local stage = 0
local screen_x, screen_y = 0, 0
local is_in_match = false
local is_before_curtain = false
local has_match_just_started = false
local has_match_just_ended = false
local match_state = 0

local P1, P2

-- Input bitfield decode
local INPUT_BITS = {
   up = 0x0001, down = 0x0002, left = 0x0004, right = 0x0008,
   LP = 0x0010, MP = 0x0020, HP = 0x0040,
   LK = 0x0100, MK = 0x0200, HK = 0x0400,
   start = 0x0800, coin = 0x1000,
}

local function make_input_set(value)
   return {
      up = value, down = value, left = value, right = value,
      LP = value, MP = value, HP = value,
      LK = value, MK = value, HK = value,
      start = value, coin = value
   }
end

local function decode_input(raw)
   local t = {}
   for name, mask in pairs(INPUT_BITS) do
      t[name] = (raw & mask) ~= 0
   end
   return t
end

local function make_player(id)
   return {
      id = id,
      prefix = id == 1 and "P1" or "P2",
      type = "player",
      -- Position
      pos_x = 0, pos_y = 0,
      previous_pos_x = 0, previous_pos_y = 0,
      -- Character
      char_id = 2, char_str = "ryu",
      -- Posture
      posture = 0, posture_ext = 0,
      previous_posture = 0,
      is_standing = false, is_crouching = false,
      is_jumping = false, is_airborne = false, is_grounded = false,
      -- Standing state (raw byte from bridge, tracks ground/air FSM)
      standing_state = 0,
      previous_standing_state = 0,
      -- Combat
      is_attacking = false, is_blocking = false,
      is_being_thrown = false, is_throwing = false,
      -- Guard
      guard_flag = 0, guard_chuu = 0, blocking_id = 0,
      -- Animation
      animation = "0000", animation_frame = 0,
      -- Stun
      is_stunned = false, stun_timer = 0,
      stun_just_began = false,
      -- Combo
      combo = 0,
      -- Input
      input = {
         pressed = make_input_set(false),
         released = make_input_set(false),
         down = make_input_set(false),
         state_time = make_input_set(0),
         last_state_time = make_input_set(0),
      },
      -- Meter
      meter_gauge = 0, meter_count = 0,
      -- Side
      side = 1,
      -- State
      flip_x = 0, direction = 0,
      velocity_x = 0, velocity_y = 0,

      -- Derived fields (frame-to-frame transitions)
      is_waking_up = false,
      previous_is_wakingup = false,
      has_just_woke_up = false,
      is_fast_wakingup = false,
      has_just_been_thrown = false,
      idle_time = 0,
      is_idle = false,
      just_received_connection = false,
      can_fast_wakeup = 0,
      previous_can_fast_wakeup = 0,
      is_past_fast_wakeup_frame = false,
      has_just_landed = false,

      -- Prediction & Frame Data Tracking
      cooldown = 0,
      throw_invulnerability_cooldown = 0,
      is_in_pushback = false,
      pushback_start_frame = 0,
      previous_recovery_time = 0,
      ends_recovery_next_frame = false,
      remaining_wakeup_time = 0,

      -- Raw engine data (filled per-frame)
      _raw = nil,
   }
end

local jump_postures = {[20]=true, [22]=true, [24]=true, [26]=true, [28]=true, [30]=true}
local movement_postures = {[6]=true, [8]=true, [10]=true, [12]=true}

local function update_player(player, raw, other)
   if not raw then return end

   player._raw = raw
   player.previous_pos_x = player.pos_x
   player.previous_pos_y = player.pos_y

   -- Snapshot previous-frame values for transition detection
   local prev_is_being_thrown = player.is_being_thrown
   local prev_is_stunned = player.is_stunned

   -- Position
   player.pos_x = raw.pos_x or 0
   player.pos_y = raw.pos_y or 0
   player.pos_x_char = raw.pos_x_char or 0
   player.pos_y_char = raw.pos_y_char or 0

   -- Velocity
   player.velocity_x = raw.velocity_x or 0
   player.velocity_y = raw.velocity_y or 0

   -- Direction
   player.flip_x = raw.flip_x or 0
   player.direction = raw.direction or 0

   -- Character
   player.char_id = raw.char_id or 2
   player.char_str = characters[(raw.char_id or 0) + 1] or "ryu"

   -- Posture (track previous before overwrite)
   player.previous_posture = player.posture
   player.posture = raw.posture or 0
   player.posture_ext = raw.posture_ext or 0

   player.is_standing = (player.posture == 0x0 and player.pos_y == 0) or
                         player.posture == 0x2 or player.posture == 0x6
   player.is_crouching = player.posture == 0x20 or player.posture == 0x21
   player.is_jumping = jump_postures[player.posture] or false
   player.is_airborne = player.is_jumping or (player.posture == 0 and player.pos_y ~= 0)
   player.is_grounded = player.is_standing or player.is_crouching or
                         movement_postures[player.posture] or
                         (player.posture == 0 and player.pos_y == 0)

   -- Standing state (raw bridge field, tracks ground/air FSM)
   player.previous_standing_state = player.standing_state
   player.standing_state = raw.standing_state or 0

   -- Guard/blocking
   player.guard_flag = raw.guard_flag or 0
   player.guard_chuu = raw.guard_chuu or 0
   player.is_blocking = (raw.guard_chuu or 0) ~= 0
   player.kind_of_blocking = raw.kind_of_blocking or 0

   -- Attack state
   player.is_attacking = (raw.routine_no_1 or 0) == 4
   player.kind_of_waza = raw.kind_of_waza or 0
   player.pat_status = raw.pat_status or 0
   player.current_attack = raw.current_attack or 0
   player.attpow = raw.attpow or 0
   player.defpow = raw.defpow or 0

   -- Throws (detect being-thrown transition)
   player.is_being_thrown = raw.is_being_thrown or false
   player.is_throwing = raw.is_throwing or false
   player.tsukami_num = raw.tsukami_num or 0
   player.has_just_been_thrown = player.is_being_thrown and not prev_is_being_thrown

   -- Combat tracking
   player.combo = raw.combo_total or 0
   player.hit_flag = raw.hit_flag or 0
   player.attack_num = raw.attack_num or 0
   player.dm_guard_success = raw.dm_guard_success or 0

   -- Stun (detect onset transition)
   player.is_stunned = (raw.stun_flag or 0) ~= 0
   player.stun_timer = raw.stun_time or 0
   player.stun_bar = raw.stun_now or 0
   player.stun_bar_max = raw.stun_genkai or 64
   player.stun_just_began = player.is_stunned and not prev_is_stunned

   -- Animation
   player.animation_frame_id = raw.animation_frame_id or 0
   player.cg_number = raw.cg_number or 0
   -- animation as 4-hex-digit string for frame data lookup
   player.animation = string.format("%04x", raw.cg_number or 0)
   player.old_cgnum = raw.old_cgnum or 0
   player.has_animation_just_changed = (raw.cg_number ~= raw.old_cgnum)

   -- Health
   player.life = raw.life or 0
   player.vitality = raw.vitality or 0

   -- Gauge
   player.meter_gauge = raw.gauge or 0
   player.meter_count = raw.store or 0
   player.max_meter_gauge = raw.gauge_len or 0
   player.max_meter_count = raw.store_max or 0
   player.selected_sa = raw.selected_sa or 0

   -- Recovery / freeze
   player.remaining_freeze_frames = raw.remaining_freeze_frames or 0
   player.recovery_time = raw.recovery_time or 0
   player.hit_stop = raw.hit_stop or 0
   player.dm_stop = raw.dm_stop or 0

   -- Routine state
   player.routine_no = {
      raw.routine_no_0 or 0,
      raw.routine_no_1 or 0,
      raw.routine_no_2 or 0,
      raw.routine_no_3 or 0,
   }
   player.movement_type = raw.movement_type or 0
   player.movement_type2 = raw.movement_type2 or 0

   -- Dead
   player.dead_flag = raw.dead_flag or 0

   -- Extended
   player.running_f = raw.running_f or 0
   player.cancel_timer = raw.cancel_timer or 0
   player.ukemi_ok_timer = raw.ukemi_ok_timer or 0
   player.att_plus = raw.att_plus or 0
   player.def_plus = raw.def_plus or 0

   -- Side detection
   if other then
      local diff = math.floor(player.pos_x) - math.floor(other.pos_x)
      if diff == 0 then
         diff = math.floor(player.previous_pos_x) - math.floor(other.previous_pos_x)
      end
      player.side = diff > 0 and 2 or 1
   end

   -- Input (from WORK_CP)
   if raw.sw_now then
      local prev_down = player.input.down
      player.input.down = decode_input(raw.sw_now)
      player.input.pressed = {}
      player.input.released = {}
      for name, _ in pairs(INPUT_BITS) do
         player.input.pressed[name] = player.input.down[name] and not (prev_down[name] or false)
         player.input.released[name] = not player.input.down[name] and (prev_down[name] or false)
         
         if player.input.pressed[name] then
            player.input.state_time[name] = 1
         elseif player.input.down[name] then
            player.input.state_time[name] = player.input.state_time[name] + 1
         elseif player.input.released[name] then
            player.input.last_state_time[name] = player.input.state_time[name]
            player.input.state_time[name] = 0
         end
      end
   end

   -- ── Derived fields (computed from raw bridge data) ──

   -- just_received_connection: non-zero received_connection_marker
   player.just_received_connection = (raw.received_connection_marker or 0) ~= 0

   -- Wakeup state: posture 0x26 = knocked down / waking up
   player.previous_is_wakingup = player.is_waking_up
   player.is_waking_up = player.posture == 0x26
   player.has_just_woke_up = player.previous_is_wakingup and not player.is_waking_up

   -- Fast wakeup tracking
   player.previous_can_fast_wakeup = player.can_fast_wakeup
   player.can_fast_wakeup = raw.can_fast_wakeup or 0

   -- is_past_fast_wakeup_frame: fires once when can_fast_wakeup transitions 1→0
   if player.previous_can_fast_wakeup ~= 0 and player.can_fast_wakeup == 0 then
      player.is_past_fast_wakeup_frame = true
   end
   if player.has_just_woke_up then
      player.is_past_fast_wakeup_frame = false
      player.is_fast_wakingup = false
   end

   -- has_just_landed: standing_state transitions from non-ground to ground
   player.has_just_landed = is_ground_state(player, player.standing_state) and
                            not is_ground_state(player, player.previous_standing_state)

   -- is_idle: simplified version (no busy_flag / input_capacity in bridge)
   player.is_idle = not player.is_attacking and
                    not player.is_blocking and
                    not player.is_waking_up and
                    not player.is_being_thrown and
                    player.remaining_freeze_frames == 0

   if player.is_idle then
      player.idle_time = (player.idle_time or 0) + 1
   else
      player.idle_time = 0
   end
end

local function read_game_vars()
   local g = engine.read_globals()
   if not g then return end

   local prev_match_state = match_state
   frame_number = g.frame_number or 0
   is_in_match = g.is_in_match or false
   stage = g.stage or 0
   screen_x = g.screen_x or 0
   screen_y = 0 -- not currently exposed

   -- Match transition detection
   has_match_just_started = not match_state and is_in_match
   has_match_just_ended = match_state and not is_in_match
   match_state = is_in_match
end

local function update()
   read_game_vars()

   local raw1 = engine.read_player(1)
   local raw2 = engine.read_player(2)

   if raw1 and raw2 then
      update_player(P1, raw1, P2)
      update_player(P2, raw2, P1)
   end
end

-- Initialize players
P1 = make_player(1)
P2 = make_player(2)
P1.other = P2
P2.other = P1

local player_objects = {P1, P2}

-- Helper functions matching gamestate.lua API
local function is_standing_state(player, state)
   return state == 0x01
end

local function is_crouching_state(player, state)
   return state == 0x02
end

local function is_ground_state(player, state)
   return is_standing_state(player, state) or is_crouching_state(player, state)
end

local function get_side(player_x, other_x, prev_x, prev_other_x)
   local diff = math.floor(player_x) - math.floor(other_x)
   if diff == 0 then diff = math.floor(prev_x) - math.floor(prev_other_x) end
   return diff > 0 and 2 or 1
end

local function get_additional_recovery_delay(char_str, crouching)
   if crouching then
      if char_str == "q" or char_str == "ryu" or char_str == "chunli" then return 2 end
   else
      if char_str == "q" then return 1 end
   end
   return 0
end

-- Module export
local gamestate = {
   update = update,
   read_game_vars = read_game_vars,
   characters = characters,
   is_standing_state = is_standing_state,
   is_crouching_state = is_crouching_state,
   is_ground_state = is_ground_state,
   get_side = get_side,
   get_additional_recovery_delay = get_additional_recovery_delay,
}

setmetatable(gamestate, {
   __index = function(_, key)
      if key == "frame_number" then return frame_number
      elseif key == "player_objects" then return player_objects
      elseif key == "P1" then return P1
      elseif key == "P2" then return P2
      elseif key == "projectiles" then return {}  -- TODO: projectile support
      elseif key == "stage" then return stage
      elseif key == "screen_x" then return screen_x
      elseif key == "screen_y" then return screen_y
      elseif key == "match_state" then return match_state
      elseif key == "is_in_match" then return is_in_match
      elseif key == "is_before_curtain" then return is_before_curtain
      elseif key == "has_match_just_started" then return has_match_just_started
      elseif key == "has_match_just_ended" then return has_match_just_ended
      end
   end,

   __newindex = function(_, key, value)
      if key == "frame_number" then frame_number = value
      elseif key == "player_objects" then player_objects = value
      elseif key == "P1" then P1 = value
      elseif key == "P2" then P2 = value
      else rawset(gamestate, key, value) end
   end
})

return gamestate
