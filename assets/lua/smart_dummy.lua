-- smart_dummy.lua
-- Minimal smart dummy using prediction.predict_hits() + joypad.set()
-- Auto-blocks and auto-parries incoming attacks for P2 (the dummy).

local prediction = _G.prediction
local gamestate  = _G.gamestate

if not prediction then
   print("[smart_dummy] WARNING: prediction module not loaded, dummy disabled")
   return { update = function() end, enabled = false }
end

local smart_dummy = {}
smart_dummy.enabled = true

-- Configuration: can be changed at runtime
smart_dummy.mode = "block"   -- "block", "parry", "off"
smart_dummy.idle_pose = "stand"  -- "stand", "crouch"
smart_dummy.prediction_frames = 3

-- ============================================================
-- Internal State
-- ============================================================

local is_blocking = false
local block_start_frame = 0
local pre_parry_frame = 0

-- ============================================================
-- Helpers
-- ============================================================

--- Build the input table for joypad.set() to make P2 block.
--- @param hit_type number 1=high/mid, 2=low, 3=overhead
--- @param p2 table Player 2 gamestate
--- @return table input table for joypad.set
local function make_block_input(hit_type, p2)
   local input = {}
   -- Block direction: hold BACK relative to opponent
   -- side=1 means P2 is on the left → back is "left"
   -- side=2 means P2 is on the right → back is "right"
   if p2.side == 1 then
      input.left = true   -- back
   else
      input.right = true  -- back
   end
   -- Low block for low attacks
   if hit_type == 2 then
      input.down = true
   end
   return input
end

--- Build the input table for a parry.
--- @param hit_type number
--- @param p2 table Player 2 gamestate
--- @return table input table for joypad.set
local function make_parry_input(hit_type, p2)
   local input = {}
   if hit_type == 2 then
      -- Low parry: tap down
      input.down = true
   else
      -- Forward parry: tap FORWARD relative to opponent
      if p2.side == 1 then
         input.right = true  -- forward
      else
         input.left = true   -- forward
      end
   end
   return input
end

--- Determine the best hit type from a list of expected attacks.
--- Returns 1=high, 2=low, 3=overhead, or nil if no attacks.
local function get_dominant_hit_type(attack_list)
   if not attack_list or #attack_list == 0 then return nil end
   local hit_type = 1  -- default high
   for _, attack in ipairs(attack_list) do
      -- If any attack is low, we need to low-block
      if attack.hit_type and attack.hit_type == 2 then
         hit_type = 2
      end
   end
   return hit_type
end

-- ============================================================
-- Per-Frame Update
-- ============================================================

--- Called every frame from training_main.lua's emu.registerbefore callback.
function smart_dummy.update()
   if not smart_dummy.enabled or smart_dummy.mode == "off" then
      return
   end

   -- Refresh gamestate references
   local gs = gamestate
   local p1 = gs.P1
   local p2 = gs.P2

   if not p1 or not p2 then return end
   if not gs.is_in_match then
      is_blocking = false
      return
   end

   -- Don't override inputs if P2 is in hitstun/blockstun/knockdown
   -- (the game engine handles those states)
   if p2.is_stunned then
      is_blocking = false
      return
   end

   -- Run prediction: detect attacks coming within N frames
   local expected_attacks = prediction.predict_hits(nil, nil, smart_dummy.prediction_frames)

   -- Find the soonest incoming attack
   local soonest_delta = 99
   local soonest_attacks = nil
   for delta, attack_list in pairs(expected_attacks) do
      if type(delta) == "number" and delta < soonest_delta and #attack_list > 0 then
         -- Filter to attacks targeting P2 (from P1 or P1's projectiles)
         local relevant = {}
         for _, atk in ipairs(attack_list) do
            local from_p1 = (atk.blocking_type == "player" and atk.id == p1.id) or
                            (atk.blocking_type == "projectile" and atk.owner_id == p1.id)
            if from_p1 then
               relevant[#relevant + 1] = atk
            end
         end
         if #relevant > 0 then
            soonest_delta = delta
            soonest_attacks = relevant
         end
      end
   end

   local input = {}
   local should_set_input = false

   if soonest_attacks then
      local hit_type = get_dominant_hit_type(soonest_attacks)

      if smart_dummy.mode == "parry" then
         -- Parry timing: need neutral frame before parry frame
         if soonest_delta == 2 then
            -- Pre-parry: go neutral this frame
            input = {}  -- no directional input
            should_set_input = true
            is_blocking = true
            pre_parry_frame = gs.frame_number
         elseif soonest_delta <= 1 then
            -- Parry frame: tap forward (or down for low)
            input = make_parry_input(hit_type, p2)
            should_set_input = true
            is_blocking = true
         end
      else
         -- Block mode: hold back when attacks are within threshold
         if soonest_delta <= smart_dummy.prediction_frames then
            input = make_block_input(hit_type, p2)
            should_set_input = true
            is_blocking = true
            block_start_frame = gs.frame_number
         end
      end
   else
      -- No attacks incoming
      if is_blocking then
         -- Stay blocking for a few extra frames to handle multi-hit moves
         local hold_frames = 5
         if gs.frame_number - block_start_frame < hold_frames then
            -- Maintain last block direction
            if smart_dummy.mode == "block" then
               input = make_block_input(1, p2)  -- default high block
               should_set_input = true
            end
         else
            is_blocking = false
         end
      end

      -- Idle pose when not blocking
      if not is_blocking and smart_dummy.idle_pose == "crouch" then
         input.down = true
         should_set_input = true
      end
   end

   -- Apply inputs via joypad
   if should_set_input then
      joypad.set(input, 2)  -- P2 = player 2
   end
end

print("[smart_dummy] Loaded (mode: " .. smart_dummy.mode .. ")")
return smart_dummy
