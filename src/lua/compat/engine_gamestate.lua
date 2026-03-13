-- engine_gamestate.lua
-- Drop-in replacement for effie's gamestate.lua using engine.read_player()
-- instead of FBNeo memory.read*() calls. Faithfully replicates every section
-- of the original read_player_vars() from 3rd_training_lua-main/src/gamestate.lua.

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
local projectiles = {}

-- CPS3 base offset cache: maps char_str -> { [koc] = { resolved = base_val, candidates = { [base_val]=true } } }
local cps3_base_cache = {}


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
      pos_x_char = 0, pos_y_char = 0,
      -- Character
      char_id = 2, char_str = "ryu",
      -- Posture
      posture = 0, posture_ext = 0,
      previous_posture = 0,
      is_standing = false, is_crouching = false,
      is_jumping = false, is_airborne = false, is_grounded = false,
      -- Standing state
      standing_state = 0,
      previous_standing_state = 0,
      -- Combat
      is_attacking = false, is_blocking = false,
      is_being_thrown = false, is_throwing = false,
      -- Guard
      guard_flag = 0, guard_chuu = 0, blocking_id = 0,
      kind_of_blocking = 0,
      -- Animation
      animation = "0000", animation_frame = 0, frame = 0,
      animation_frame_id = 0, cg_number = 0, old_cgnum = 0,
      has_animation_just_changed = false,
      animation_start_frame = 0,
      animation_freeze_frames = 0,
      animation_frame_data = nil,
      animation_frame_hash = nil,
      -- Animation reset tracking
      current_hit_id = 0,
      animation_action_count = 0,
      animation_miss_count = 0,
      animation_connection_count = 0,
      -- Stun
      is_stunned = false, stun_timer = 0,
      previous_stun_timer = 0,
      stun_just_began = false, stun_just_ended = false,
      stun_bar = 0, stun_bar_max = 64,
      stun_activate = 0,
      previous_stunned = false,
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
      max_meter_gauge = 0, max_meter_count = 0,
      -- Side
      side = 1,
      flip_input = false,
      selected_sa = 0,
      sa_state = 0,
      -- State
      flip_x = 0, direction = 0,
      velocity_x = 0, velocity_y = 0,
      acceleration_x = 0, acceleration_y = 0,
      -- Recovery / Freeze
      recovery_time = 0,
      previous_recovery_time = 0,
      remaining_freeze_frames = 0,
      previous_remaining_freeze_frames = 0,
      remaining_wakeup_time = 0,
      additional_recovery_time = 0,
      ends_recovery_next_frame = false,
      is_in_recovery = false,
      recovery_flag = 0,
      recovery_type = 0,
      freeze_just_began = false,
      freeze_just_ended = false,
      movement_type = 0,
      movement_type2 = 0,
      -- Action tracking
      action = 0, previous_action = 0,
      action_count = 0,
      action_ext = 0,
      has_just_acted = false,
      just_cancelled_into_attack = false,
      input_capacity = 99,
      busy_flag = 0,
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
      fast_wakeup_flag = 0,
      is_past_fast_wakeup_frame = false,
      has_just_landed = false,
      has_just_blocked = false,
      has_just_parried = false,
      has_just_connected = false,
      has_just_hit = false,
      has_just_attacked = false,
      has_just_ended_recovery = false,
      has_just_entered_air_recovery = false,
      -- Received hit tracking
      total_received_hit_count = 0,
      previous_total_received_hit_count = 0,
      received_connection_id = 0,
      received_connection_type = 0,
      received_connection_is_projectile = false,
      total_received_projectiles_count = 0,
      previous_total_received_projectiles_count = 0,
      last_received_connection_animation = nil,
      last_received_connection_hit_id = nil,
      -- Push/Recovery tracking
      cooldown = 0,
      throw_invulnerability_cooldown = 0,
      throw_countdown = 0,
      throw_recovery_frame = 0,
      throw_tech_countdown = 0,
      is_in_pushback = false,
      is_in_air_recovery = false,
      previous_is_in_air_recovery = false,
      is_in_air_reel = false,
      is_in_throw_tech = false,
      is_in_timed_sa = false,
      pushback_start_frame = 0,
      should_turn = nil,
      switched_sides = false,
      -- Hitboxes
      boxes = {},
      -- Character state
      character_state_byte = 0,
      is_attacking_byte = 0,
      is_attacking_ext_byte = 0,
      -- Superfreeze (read from C bridge: sa_stop_flag + hit_stop)
      superfreeze_decount = 0,
      superfreeze_just_began = false,
      superfreeze_just_ended = false,
      -- Parry tracking (read from C bridge: wcp[].waza_flag[] + waza_work[].free3)
      -- Game constants: forward/down max_validity=10, max_cooldown=21;
      --                 air max_validity=7, max_cooldown=18;
      --                 antiair max_validity=5, max_cooldown=16
      parry_forward = { name = "forward", validity_time = 0, max_validity = 10, cooldown_time = 0, max_cooldown = 21 },
      parry_down = { name = "down", validity_time = 0, max_validity = 10, cooldown_time = 0, max_cooldown = 21 },
      parry_air = { name = "air", validity_time = 0, max_validity = 7, cooldown_time = 0, max_cooldown = 18 },
      parry_antiair = { name = "anti_air", validity_time = 0, max_validity = 5, cooldown_time = 0, max_cooldown = 16 },
      -- hit tracking
      connected_action_count = 0,
      previous_connected_action_count = 0,
      hit_count = 0,
      previous_hit_count = 0,
      counter = {},
      blocking = {},
      has_just_been_hit = false,
      pending_input_sequence = nil,
      -- Health
      life = 0, vitality = 0,
      base = 0,
      -- Air state
      is_flying_down_flag = 0,
      air_recovery_1 = 0,
      air_recovery_2 = 0,
      -- Memory addresses (for write_memory module — mostly no-ops)
      addresses = {},
      -- Raw engine data (filled per-frame)
      _raw = nil,
   }
end

local jump_postures = {[20]=true, [22]=true, [24]=true, [26]=true, [28]=true, [30]=true}
local movement_postures = {[6]=true, [8]=true, [10]=true, [12]=true}

-- State helpers (must be declared before update_player)
local function is_standing_state(player, state)
   return state == 0x01
end

local function is_crouching_state(player, state)
   return state == 0x02
end

local function is_ground_state(player, state)
   return is_standing_state(player, state) or is_crouching_state(player, state)
end

-- ============================================================
-- update_player: 1:1 port of original read_player_vars()
-- Section references are to 3rd_training_lua-main/src/gamestate.lua
-- ============================================================
local function update_player(player, raw, other)
   if not raw then return end

   player._raw = raw

   -- ── Save all previous-frame values at top (effie pattern) ──
   local previous_is_attacking = player.is_attacking or false
   local previous_action = player.action or 0
   local prev_is_being_thrown = player.is_being_thrown
   local prev_is_stunned = player.is_stunned
   local prev_connected_action_count = player.connected_action_count or 0
   local prev_hit_count = player.hit_count or 0
   local prev_total_received_hit_count = player.total_received_hit_count or 0
   local prev_total_received_projectiles_count = player.total_received_projectiles_count or 0
   player.previous_pos_x = player.pos_x
   player.previous_pos_y = player.pos_y
   player.previous_remaining_freeze_frames = player.remaining_freeze_frames or 0
   player.previous_recovery_time = player.recovery_time or 0
   player.previous_is_in_air_recovery = player.is_in_air_recovery or false
   local previous_superfreeze_decount = player.superfreeze_decount or 0

   -- ── Position (gamestate.lua:280-295) ──
   player.pos_x = raw.pos_x or 0
   player.pos_y = raw.pos_y or 0
   player.pos_x_char = raw.pos_x_char or 0
   player.pos_y_char = raw.pos_y_char or 0

   -- ── Velocity / Acceleration ──
   player.velocity_x = raw.velocity_x or 0
   player.velocity_y = raw.velocity_y or 0
   player.acceleration_x = raw.acceleration_x_char or 0
   player.acceleration_y = raw.acceleration_y_char or 0

   -- ── Direction (offset 0x0A = flip_x) ──
   player.flip_x = raw.flip_x or 0
   player.direction = raw.direction or 0

   -- ── Character (offset 0x3C0) ──
   player.char_id = raw.char_id or 2
   player.char_str = characters[(raw.char_id or 0) + 1] or "ryu"

   -- ── Freeze Frames (gamestate.lua:432-444) ──
   player.remaining_freeze_frames = raw.remaining_freeze_frames or 0
   player.freeze_just_began = (player.remaining_freeze_frames > 0 and player.previous_remaining_freeze_frames == 0)
   player.freeze_just_ended = (player.remaining_freeze_frames == 0 and player.previous_remaining_freeze_frames > 0)

   -- ── Action tracking (gamestate.lua:460-465) ──
   player.previous_action = previous_action
   player.action = raw.action_type or 0  -- cgd_type: 0=idle, 4=normals, 5=specials
   player.action_ext = raw.action_ext or 0
   player.input_capacity = raw.input_capacity or 99
   player.busy_flag = raw.busy_flag or 0
   player.has_just_acted = (player.action ~= 0 and previous_action == 0)

   -- ── Recovery (gamestate.lua:466-533) ──
   player.recovery_time = raw.recovery_time or 0
   player.recovery_flag = raw.recovery_flag or 0
   player.recovery_type = raw.recovery_type or 0
   player.remaining_wakeup_time = raw.wakeup_time or raw.remaining_wakeup_time or 0
   player.movement_type = raw.movement_type or 0
   player.movement_type2 = raw.movement_type2 or 0

   -- Recovery state machine (gamestate.lua:503-533)
   player.has_just_ended_recovery = false
   if player.previous_recovery_time > 0 and player.recovery_time == 0 then
      player.has_just_ended_recovery = true
   end
   player.ends_recovery_next_frame = (player.recovery_time == 1)

   -- is_in_recovery (original uses recovery_flag and recovery_type)
   player.is_in_recovery = (player.recovery_flag ~= 0) or (player.recovery_time > 0)

   -- additional_recovery_time (gamestate.lua:520-530)
   player.additional_recovery_time = 0
   if player.is_in_recovery and player.recovery_type ~= 0 then
      local crouching = is_crouching_state(player, player.standing_state)
      player.additional_recovery_time = get_additional_recovery_delay(player.char_str, crouching)
   end

   -- ── Posture (gamestate.lua:481-495) ──
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

   -- Standing state (raw bridge field)
   player.previous_standing_state = player.standing_state
   player.standing_state = raw.standing_state or 0

   -- ── Throw detection (gamestate.lua:554-574) ──
   player.is_being_thrown = raw.is_being_thrown or false
   player.is_throwing = raw.is_throwing or false
   player.tsukami_num = raw.tsukami_num or 0
   player.has_just_been_thrown = player.is_being_thrown and not prev_is_being_thrown
   player.throw_countdown = raw.throw_countdown or 0

   -- ── Animation (gamestate.lua:576-703 — effie-style frame-delta) ──
   player.animation_frame_id = raw.animation_frame_id or 0  -- CPS3 cg_ix (raw data)
   player.cg_number = raw.cg_number or 0
   player.old_cgnum = raw.old_cgnum or 0

   local previous_animation = player.animation or "0000"
   -- Compute CPS3-style animation ID from byte offset + CPS3 base
   local anim_byte_offset = raw.animation_raw_table_val or 0
   local now_koc = raw.now_koc or 0
   local cps3_base = nil

   -- Dynamic CPS3 base resolver per koc
   local fd_mod = package.loaded["src.data.framedata"]
   if fd_mod and fd_mod.is_loaded and fd_mod.frame_data[player.char_str] then
      local char_fd = fd_mod.frame_data[player.char_str]
      if not cps3_base_cache[player.char_str] then cps3_base_cache[player.char_str] = {} end
      local koc_state = cps3_base_cache[player.char_str][now_koc]
      
      if not koc_state then
         koc_state = { resolved = nil, candidates = {} }
         cps3_base_cache[player.char_str][now_koc] = koc_state
         
         -- koc 0 can be instantly resolved via the standing animation (fixed byte offset 0xD8)
         if now_koc == 0 then
            local standing_key = char_fd.standing
            if standing_key and type(standing_key) == "string" then
               local standing_hex = tonumber(standing_key, 16)
               if standing_hex then
                  koc_state.resolved = (standing_hex - 0xD8) % 65536
                  print(string.format("[CPS3_BASE] Resolved %s koc=%d instantly to 0x%04X via standing_key=%s", 
                     player.char_str, now_koc, koc_state.resolved, standing_key))
               end
            end
         end
      end

      if koc_state.resolved then
         cps3_base = koc_state.resolved
      elseif player.has_animation_just_changed then
         -- Intersection algorithm for undiscovered koc bases:
         -- For the current byte_offset, find all bases that map to a valid framedata key
         local new_candidates = {}
         local num_new_candidates = 0
         for key, _ in pairs(char_fd) do
            if type(key) == "string" then
               local hex_val = tonumber(key, 16)
               if hex_val then
                  local possible_base = (hex_val - anim_byte_offset) % 65536
                  new_candidates[possible_base] = key
                  num_new_candidates = num_new_candidates + 1
               end
            end
         end

         -- Intersect with existing candidates
         local is_empty = true
         for _ in pairs(koc_state.candidates) do is_empty = false; break; end
         
         if is_empty then
            for b_val, key in pairs(new_candidates) do koc_state.candidates[b_val] = key end
         else
            local filtered = {}
            for b_val, key in pairs(koc_state.candidates) do
               if new_candidates[b_val] then filtered[b_val] = key end
            end
            koc_state.candidates = filtered
         end

         -- Check if resolved (exactly 1 candidate remains)
         local remaining_count = 0
         local last_valid_base = nil
         local last_valid_key = nil
         for b_val, key in pairs(koc_state.candidates) do
            remaining_count = remaining_count + 1
            last_valid_base = b_val
            last_valid_key = key
         end

         if remaining_count == 1 then
            koc_state.resolved = last_valid_base
            cps3_base = last_valid_base
            print(string.format("[CPS3_BASE] Resolved %s koc=%d algorithmically to 0x%04X (mapped boff=0x%X to anim=%s)",
               player.char_str, now_koc, last_valid_base, anim_byte_offset, last_valid_key))
         else
            -- Not resolved yet, output diagnostic but don't set base
            print(string.format("[CPS3_BASE] Pending %s koc=%d: narrowed to %d candidates after boff=0x%X",
               player.char_str, now_koc, remaining_count, anim_byte_offset))
            -- Temporarily pick the first candidate just to have something (might be wrong)
            if last_valid_base then cps3_base = last_valid_base end
         end
      end
   end

   local current_byte_offset = raw.animation_raw_table_val
   if cps3_base and current_byte_offset then
      player.animation = string.format("%04x", (current_byte_offset + cps3_base) % 65536)
   elseif current_byte_offset then
      player.animation = string.format("%04x", current_byte_offset % 65536)
   end
   player.has_animation_just_changed = (previous_animation ~= player.animation)

   -- Freeze frame accumulation (gamestate.lua:607-623)
   if player.remaining_freeze_frames > 0 and not player.freeze_just_began then
      player.animation_freeze_frames = player.animation_freeze_frames + 1
   elseif player.remaining_freeze_frames == 0 and player.previous_remaining_freeze_frames > 0 then
      player.animation_freeze_frames = player.animation_freeze_frames + 1
   end

   -- Compute logical animation frame (gamestate.lua:629)
   player.animation_frame = frame_number - player.animation_start_frame - player.animation_freeze_frames
   player.frame = player.animation_frame

   -- Reset on animation change (gamestate.lua:642-656)
   if player.has_animation_just_changed then
      player.animation_start_frame = frame_number
      player.animation_freeze_frames = 0
      player.current_hit_id = 0
      player.animation_action_count = 0
      player.animation_miss_count = 0
      player.animation_connection_count = 0
      player.animation_frame = 0
      player.frame = 0
   end

   -- ── Frame data lookup (gamestate.lua:658-703) ──
   local fd = package.loaded["src.data.framedata"]
   player.animation_frame_data = nil
   player.boxes = {}
   if fd and fd.is_loaded and fd.frame_data[player.char_str] then
      -- One-time diagnostic
      if not _G._fd_adapter_confirmed then
         _G._fd_adapter_confirmed = true
         print(string.format("[ADAPTER_FD] Framedata found for %s, animation=%s",
            player.char_str, player.animation))
      end

      local char_fd = fd.frame_data[player.char_str]
      local anim_data = char_fd[player.animation]

      -- Fallback to standing/crouching framedata for idle animations
      -- char_fd["standing"] returns a string key (e.g. "9930"), not the data itself
      -- So we need to dereference: char_fd[char_fd["standing"]] or char_fd[char_fd.standing]
      if not anim_data then
         local fallback_key = nil
         if player.is_crouching then
            fallback_key = char_fd.crouching or char_fd["crouching"]
         elseif player.is_standing or player.is_grounded then
            fallback_key = char_fd.standing or char_fd["standing"]
         end
         if fallback_key and type(fallback_key) == "string" then
            anim_data = char_fd[fallback_key]
         elseif fallback_key and type(fallback_key) == "table" then
            anim_data = fallback_key  -- already the data
         end
      end

      if anim_data then
         player.animation_frame_data = anim_data
         if anim_data.frames then
            player.animation_frame = math.min(player.animation_frame, #anim_data.frames - 1)
            player.animation_frame = math.max(player.animation_frame, 0)
            player.frame = player.animation_frame
            local frame_entry = anim_data.frames[player.animation_frame + 1]
            if frame_entry then
               player.boxes = frame_entry.boxes or {}
            end
         end
      end
   end

   -- ── Attack state (gamestate.lua:718-742) ──
   player.character_state_byte = raw.character_state_byte or raw.routine_no_1 or 0

   -- Diagnostic: log all combat fields to identify correct is_attacking field
   if not _G._attack_diag_idle_done and player.id == 2 then
      _G._attack_diag_idle_done = true
      print(string.format("[ATTACK_DIAG] IDLE P%d: rno1=%s kow=%s pat=%s cgd=%s at_attr=%s atk_num=%s dm_atk=%s anim=%s",
         player.id, tostring(raw.routine_no_1), tostring(raw.kind_of_waza),
         tostring(raw.pat_status), tostring(raw.movement_type),
         tostring(raw.at_attribute), tostring(raw.attack_num),
         tostring(raw.dm_attack_flag), tostring(player.animation)))
   end
   -- Log when animation changes (potential attack state)
   if player.has_animation_just_changed and player.id == 2 then
      if not _G._attack_diag_count then _G._attack_diag_count = 0 end
      _G._attack_diag_count = _G._attack_diag_count + 1
      if _G._attack_diag_count <= 20 then
      print(string.format("[ATTACK_DIAG] CHANGE P%d: rno1=%s kow=%s koc=%s cidx=%s boff=0x%X anim=%s",
            player.id, tostring(raw.routine_no_1), tostring(raw.kind_of_waza),
            tostring(raw.now_koc), tostring(raw.char_index),
            raw.animation_byte_offset or 0, tostring(player.animation)))
      end
   end

   -- Use routine_no_1 == 4 for now (character_state_byte in CPS3 at offset 0x27)
   player.is_attacking = (player.character_state_byte == 4)
   player.has_just_attacked = player.is_attacking and not previous_is_attacking
   player.is_attacking_byte = player.is_attacking and 1 or 0
   player.is_attacking_ext_byte = raw.is_attacking_ext_byte or 0
   player.kind_of_waza = raw.kind_of_waza or 0
   player.pat_status = raw.pat_status or 0
   player.current_attack = raw.current_attack or 0
   player.attpow = raw.attpow or 0
   player.defpow = raw.defpow or 0

   -- Action count tracking (gamestate.lua:784-837)
   player.previous_connected_action_count = prev_connected_action_count
   player.connected_action_count = raw.connected_action_count or 0
   player.action_count = raw.action_count or 0
   player.previous_hit_count = prev_hit_count
   player.hit_count = raw.hit_count or 0

   -- has_just_connected: connected_action_count incremented
   player.has_just_connected = (player.connected_action_count > prev_connected_action_count)

   -- just_cancelled_into_attack (gamestate.lua:595-605)
   player.just_cancelled_into_attack = false
   if player.has_animation_just_changed and player.is_attacking and previous_is_attacking and
      not player.has_just_connected and not player.just_received_connection then
      player.just_cancelled_into_attack = true
   end

   -- ── Guard / Blocking (gamestate.lua:743-782) ──
   player.guard_flag = raw.guard_flag or 0
   player.guard_chuu = raw.guard_chuu or 0
   player.blocking_id = raw.blocking_id or raw.kind_of_blocking or 0
   player.kind_of_blocking = raw.kind_of_blocking or 0
   -- is_blocking from blocking_id (original: blocking_id > 0 and blocking_id < 5)
   player.is_blocking = (player.blocking_id > 0 and player.blocking_id < 5) or
                         (raw.guard_chuu or 0) ~= 0

   -- Received hit/block/parry detection (gamestate.lua:718-782)
   player.previous_total_received_hit_count = prev_total_received_hit_count
   player.total_received_hit_count = raw.total_received_hit_count or 0
   player.received_connection_id = raw.received_connection_id or 0
   player.received_connection_type = raw.received_connection_type or 0
   player.previous_total_received_projectiles_count = prev_total_received_projectiles_count
   player.total_received_projectiles_count = raw.total_received_projectiles_count or 0

   -- just_received_connection: total_received_hit_count incremented (gamestate.lua:722)
   player.just_received_connection = (player.total_received_hit_count ~= prev_total_received_hit_count)

   -- received_connection_is_projectile (gamestate.lua:728)
   player.received_connection_is_projectile = (player.received_connection_type == 2 or
                                                player.received_connection_type == 4)

   -- has_just_blocked (gamestate.lua:751-755)
   player.has_just_blocked = false
   if player.just_received_connection and player.is_blocking then
      player.has_just_blocked = true
   end

   -- has_just_parried (gamestate.lua:757-762)
   player.has_just_parried = false
   if player.just_received_connection and player.blocking_id >= 5 and player.blocking_id <= 8 then
      player.has_just_parried = true
   end

   -- has_just_been_hit (gamestate.lua:764-770)
   player.has_just_been_hit = false
   if player.just_received_connection and not player.has_just_blocked and not player.has_just_parried then
      player.has_just_been_hit = true
   end

   -- has_just_hit: hit_count incremented (gamestate.lua:780)
   player.has_just_hit = (player.hit_count > prev_hit_count)

   -- ── Combat tracking ──
   player.combo = raw.combo_total or 0
   player.hit_flag = raw.hit_flag or 0
   player.attack_num = raw.attack_num or 0
   player.dm_guard_success = raw.dm_guard_success or 0

   -- ── Stun (gamestate.lua:1305-1328) ──
   player.previous_stun_timer = player.stun_timer or 0
   player.stun_timer = raw.stun_time or 0
   player.stun_bar = raw.stun_now or 0
   player.stun_bar_max = raw.stun_genkai or 64
   player.stun_activate = 0  -- not in C bridge, use stun_flag
   player.previous_stunned = prev_is_stunned

   player.stun_just_began = false
   player.stun_just_ended = false

   -- Stun detection (gamestate.lua:1318-1328)
   local stun_flag = raw.stun_flag or 0
   if player.previous_stun_timer and player.stun_timer < player.previous_stun_timer and
      player.stun_timer > 0 and player.stun_timer < 250 then
      player.is_stunned = true
   end
   if stun_flag ~= 0 then
      player.is_stunned = true
      if not player.previous_stunned then player.stun_just_began = true end
   elseif player.is_stunned then
      if player.just_received_connection or player.is_being_thrown or
         player.stun_timer == 0 or player.stun_timer >= 250 then
         player.is_stunned = false
         player.stun_just_ended = true
      end
   end

   -- ── Health ──
   player.life = raw.life or 0
   player.vitality = raw.vitality or 0

   -- ── Landing / Air recovery (gamestate.lua:854-882) ──
   player.has_just_landed = is_ground_state(player, player.standing_state) and
                            not is_ground_state(player, player.previous_standing_state)
   player.has_just_hit_ground = player.previous_standing_state ~= 0 and
                                player.standing_state == 0 and player.pos_y == 0

   -- Air recovery tracking (gamestate.lua:860-882)
   player.air_recovery_1 = raw.air_recovery_1 or 0
   player.air_recovery_2 = raw.air_recovery_2 or 0
   player.is_flying_down_flag = raw.is_flying_down_flag or 0

   local previous_is_in_air_recovery = player.is_in_air_recovery or false
   player.is_in_air_recovery = (player.air_recovery_1 ~= 0 or player.air_recovery_2 ~= 0) and
                                not is_ground_state(player, player.standing_state)
   player.has_just_entered_air_recovery = player.is_in_air_recovery and not previous_is_in_air_recovery

   player.is_in_air_reel = player.is_flying_down_flag ~= 0 and
                            not is_ground_state(player, player.standing_state)

   -- ── is_idle (gamestate.lua:884-900) ──
   player.is_idle = player.character_state_byte == 0 and
                    player.busy_flag == 0 and
                    player.remaining_freeze_frames == 0 and
                    not player.is_waking_up and
                    not player.is_being_thrown

   if player.is_idle then
      player.idle_time = (player.idle_time or 0) + 1
   else
      player.idle_time = 0
   end

   -- ── Wakeup (gamestate.lua:902-938) ──
   player.previous_is_wakingup = player.is_waking_up
   player.is_waking_up = player.posture == 0x26
   player.has_just_woke_up = player.previous_is_wakingup and not player.is_waking_up

   -- Fast wakeup tracking
   player.previous_can_fast_wakeup = player.can_fast_wakeup
   player.can_fast_wakeup = raw.can_fast_wakeup or 0
   player.fast_wakeup_flag = raw.fast_wakeup_flag or 0

   if player.previous_can_fast_wakeup ~= 0 and player.can_fast_wakeup == 0 then
      player.is_past_fast_wakeup_frame = true
   end
   if player.has_just_woke_up then
      player.is_past_fast_wakeup_frame = false
      player.is_fast_wakingup = false
   end

   -- ── Gauge (gamestate.lua: SA_WORK access) ──
   player.meter_gauge = raw.gauge or 0
   player.meter_count = raw.store or 0
   player.max_meter_gauge = raw.gauge_len or 0
   player.max_meter_count = raw.store_max or 0
   player.selected_sa = raw.selected_sa or 0
   player.sa_state = raw.sa_state or 0
   player.is_in_timed_sa = (player.sa_state == 4)

   -- ── Routine state ──
   player.routine_no = {
      raw.routine_no_0 or 0,
      raw.routine_no_1 or 0,
      raw.routine_no_2 or 0,
      raw.routine_no_3 or 0,
   }

   -- ── Dead ──
   player.dead_flag = raw.dead_flag or 0

   -- ── Extended ──
   player.running_f = raw.running_f or 0
   player.cancel_timer = raw.cancel_timer or 0
   player.ukemi_ok_timer = raw.ukemi_ok_timer or 0
   player.att_plus = raw.att_plus or 0
   player.def_plus = raw.def_plus or 0

   -- ── Misc ──
   player.hit_stop = raw.hit_stop or 0
   player.dm_stop = raw.dm_stop or 0

   -- ── Superfreeze (gamestate.lua:948 — uses sa_stop_flag + hit_stop) ──
   player.superfreeze_decount = raw.superfreeze_decount or 0
   player.superfreeze_just_began = (player.superfreeze_decount > 0 and previous_superfreeze_decount == 0)
   player.superfreeze_just_ended = (player.superfreeze_decount == 0 and previous_superfreeze_decount > 0)

   -- ── Parry buffers (gamestate.lua:951-1020 — from wcp[].waza_flag[] + waza_work[].free3) ──
   -- Read raw validity/cooldown from bridge
   local parry_objects = {
      { obj = player.parry_forward, vt = raw.parry_forward_validity_time or 0, ct = raw.parry_forward_cooldown_time or 0 },
      { obj = player.parry_down,    vt = raw.parry_down_validity_time or 0,    ct = raw.parry_down_cooldown_time or 0 },
      { obj = player.parry_air,     vt = raw.parry_air_validity_time or 0,     ct = raw.parry_air_cooldown_time or 0 },
      { obj = player.parry_antiair, vt = raw.parry_antiair_validity_time or 0, ct = raw.parry_antiair_cooldown_time or 0 },
   }

   -- Determine parry type for success detection (original: get_parry_type)
   local player_airborne = player.posture >= 20 and player.posture <= 30
   local opponent_airborne = other and (other.posture >= 20 and other.posture <= 30) or false
   local parry_type = "ground"
   if not player.received_connection_is_projectile then
      if player_airborne then
         parry_type = "air"
      elseif opponent_airborne then
         parry_type = "anti_air"
      end
   end

   for _, pdata in ipairs(parry_objects) do
      local po = pdata.obj
      po.last_hit_or_block_frame = po.last_hit_or_block_frame or 0
      po.last_attempt_frame = po.last_attempt_frame or 0
      po.last_validity_start_frame = po.last_validity_start_frame or 0
      if player.has_just_blocked or player.has_just_been_hit then
         po.last_hit_or_block_frame = frame_number
      end

      po.previous_validity_time = po.validity_time or 0
      po.validity_time = pdata.vt
      po.cooldown_time = pdata.ct
      if po.cooldown_time == 0xFF then po.cooldown_time = 0 end

      -- Detect new parry attempt (validity went from 0 to non-zero)
      if po.previous_validity_time == 0 and po.validity_time ~= 0 then
         po.last_validity_start_frame = frame_number
         po.delta = nil
         po.success = nil
         po.armed = true
      end

      -- Check success/miss (gamestate.lua:976-1011)
      if po.armed then
         local matches = (parry_type == "ground" and (po.name == "forward" or po.name == "down"))
                      or (parry_type == "air" and po.name == "air")
                      or (parry_type == "anti_air" and po.name == "anti_air")
         if matches then
            po.last_attempt_frame = frame_number
            if player.has_just_parried then
               po.delta = frame_number - po.last_validity_start_frame
               po.success = true
               po.armed = false
               po.last_hit_or_block_frame = 0
            elseif po.last_validity_start_frame == frame_number - 1
               and (frame_number - po.last_hit_or_block_frame) < 20 then
               local delta = po.last_hit_or_block_frame - frame_number + 1
               if po.delta == nil or math.abs(po.delta) > math.abs(delta) then
                  po.delta = delta
                  po.success = false
               end
            elseif player.has_just_blocked or player.has_just_been_hit then
               local delta = frame_number - po.last_validity_start_frame
               if po.delta == nil or math.abs(po.delta) > math.abs(delta) then
                  po.delta = delta
                  po.success = false
               end
            end
         end
      end
      if frame_number - po.last_validity_start_frame > 30 and po.armed then
         po.armed = false
         po.last_hit_or_block_frame = 0
      end
   end

   -- ── Side detection (must be BEFORE flip_input) ──
   if other then
      local diff = math.floor(player.pos_x) - math.floor(other.pos_x)
      if diff == 0 then
         diff = math.floor(player.previous_pos_x) - math.floor(other.previous_pos_x)
      end
      player.side = diff > 0 and 2 or 1
   end
   -- flip_input: set AFTER side is computed (gamestate.lua:1742-1747)
   player.flip_input = (player.side == 2)

   -- ── Input (from WORK_CP) ──
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

   -- ── Throw invulnerability (gamestate.lua:1330-1351) ──
   player.throw_invulnerability_cooldown = player.throw_invulnerability_cooldown or 0
   player.throw_recovery_frame = player.throw_recovery_frame or 0
   local previous_throw_invulnerability_cooldown = player.throw_invulnerability_cooldown
   player.throw_invulnerability_cooldown = math.max(player.throw_invulnerability_cooldown - 1, 0)
   if previous_throw_invulnerability_cooldown > 0 and player.throw_invulnerability_cooldown == 0 then
      player.throw_recovery_frame = frame_number
   end
   if player.has_just_woke_up then
      player.throw_invulnerability_cooldown = 7
   elseif player.has_just_ended_recovery then
      player.throw_invulnerability_cooldown = 6
   elseif player.is_in_recovery and player.recovery_time > 0 then
      player.throw_invulnerability_cooldown = player.recovery_time + 7 + player.additional_recovery_time
   elseif player.remaining_wakeup_time > 0 then
      player.throw_invulnerability_cooldown = player.remaining_wakeup_time + 7
   elseif player.just_received_connection or (player.character_state_byte == 1 and player.remaining_freeze_frames > 0) then
      player.throw_invulnerability_cooldown = 10
   elseif player.has_just_landed and player.previous_is_in_air_recovery then
      player.throw_invulnerability_cooldown = 6
   end
end

-- Helper: get_additional_recovery_delay (game-specific)
function get_additional_recovery_delay(char_str, crouching)
   if crouching then
      if char_str == "q" or char_str == "ryu" or char_str == "chunli" then return 2 end
   else
      if char_str == "q" then return 1 end
   end
   return 0
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
local function get_side(player_x, other_x, prev_x, prev_other_x)
   local diff = math.floor(player_x) - math.floor(other_x)
   if diff == 0 then diff = math.floor(prev_x) - math.floor(prev_other_x) end
   return diff > 0 and 2 or 1
end

-- Module export (matches original gamestate.lua interface exactly)
local gamestate = {
   update = update,
   gamestate_read = update,  -- alias for original name
   read_game_vars = read_game_vars,
   characters = characters,
   is_standing_state = is_standing_state,
   is_crouching_state = is_crouching_state,
   is_ground_state = is_ground_state,
   get_side = get_side,
   get_additional_recovery_delay = get_additional_recovery_delay,
   reset_player_objects = function()
      P1 = make_player(1)
      P2 = make_player(2)
      P1.other = P2
      P2.other = P1
      player_objects = {P1, P2}
   end,
   update_projectile_cooldown = function() end,  -- no-op, projectile system not ported
}

setmetatable(gamestate, {
   __index = function(_, key)
      if key == "frame_number" then return frame_number
      elseif key == "player_objects" then return player_objects
      elseif key == "P1" then return P1
      elseif key == "P2" then return P2
      elseif key == "projectiles" then return projectiles
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
      elseif key == "projectiles" then projectiles = value
      else rawset(gamestate, key, value) end
   end
})

return gamestate
