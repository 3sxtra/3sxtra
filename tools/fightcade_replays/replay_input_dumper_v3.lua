--[[
  FBNeo Replay Input Dumper v3.1 - Enhanced Edition
  ==================================================
  
  Captures P1/P2 inputs during replay playback for ML training.
  Based on patterns from peon2/fbneo-training-mode.
  
  Features:
  - Automatic replay detection via emu.isreplay()
  - Bit-packed serialization matching rl_bridge.h format
  - Auto-save with timestamped filenames
  - Outputs CSV + binary for training pipeline
  - Super Art selection tracking (SA I/II/III)
  - Animation/action state logging
  - Hit confirmation detection (HP delta)
  
  Usage:
  1. Load replay in Fightcade/FBNeo
  2. Load this Lua script (Game > Lua Scripting > New Lua Script Window)
  3. Script auto-detects replay and starts capturing
  4. Press F12 or wait for replay end to save
  
  Output Format (matches existing SF3 RL training):
  - CSV: frame, p1_input, p2_input (16-bit packed values)
  - Binary: FCI3 header + [P1:u16, P2:u16] per frame
]]--

print("============================================")
print("  FBNeo Replay Input Dumper v3.1")
print("  SF3:3S Input Capture for ML Training")
print("============================================")

-- Configuration
local CONFIG = {
    output_dir = "",  -- Empty = same directory as script
    auto_save_on_end = true,
    turbo_mode = true,           -- Hold F1 for speed
    auto_turbo = true,           -- NEW: Auto-enable turbo on start (for batch processing)
    auto_exit_on_end = true,     -- NEW: Exit emulator when replay ends (for batch processing)
    write_status_file = true,    -- NEW: Write status file for orchestration
    show_hud = true,
}

-- SF3:3S Memory Addresses (verified from Grouflon/3rd_training_lua)
-- Complete game state for supervised learning + RL observation
local GAME_ADDRS = {
    -- Match/Round state (from Grouflon's read_game_vars)
    timer = 0x2011377,           -- Round timer (99 at start, counts down)
    match_state = 0x020154A6,    -- Match state flags (legacy)
    match_state_byte = 0x020154A7,-- Match state: 0x02 = round active, players can move
    p1_locked = 0x020154C6,      -- P1 character locked (0xFF when in match)
    p2_locked = 0x020154C8,      -- P2 character locked (0xFF when in match)
    frame_number = 0x02007F00,   -- Global frame counter
    
    -- Player bases
    p1_base = 0x02068C6C,        -- P1 player object base
    p2_base = 0x02069104,        -- P2 player object base
    
    -- ===========================================
    -- OFFSETS FROM PLAYER BASE (verified Grouflon)
    -- ===========================================
    pos_x_off = 0x64,            -- Position X (signed word)
    pos_y_off = 0x68,            -- Position Y (signed word)
    char_id_off = 0x3C0,         -- Character ID (word)
    
    -- Action/Animation
    action_off = 0xAC,           -- Action ID (dword) - more accurate than 0x08
    animation_off = 0x202,       -- Current animation hex ID (word)
    anim_frame_off = 0x21A,      -- Animation frame ID (word)
    
    -- Priority 1: Essential state
    posture_off = 0x20E,         -- Standing/crouch/jump/knockdown (byte)
    recovery_off = 0x187,        -- Frames until recoverable (byte)
    freeze_off = 0x45,           -- Hitstop/freeze frames remaining (byte)
    attacking_off = 0x428,       -- Is attacking flag (byte)
    
    -- Priority 2: Combat state  
    thrown_off = 0x3CF,          -- Is being thrown (byte)
    blocking_off = 0x3D3,        -- Blocking ID/type (byte)
    hit_count_off = 0x189,       -- Hits landed in current string (byte)
    standing_state_off = 0x297,  -- Ground/air state (byte)
    busy_flag_off = 0x3D1,       -- Busy/can't act flag (word)
    input_capacity_off = 0x46C,  -- Input capacity (word)
    
    -- ===========================================
    -- DIRECT ADDRESSES (verified Grouflon)
    -- ===========================================
    p1_health = 0x2068D0B,       -- P1 health (0xA0 = max)
    p2_health = 0x20691A3,       -- P2 health
    p1_direction = 0x2068C76,    -- P1 facing (0=left, 1=right)
    p2_direction = 0x2068C77,    -- P2 facing
    
    -- Meter (verified master variables)
    p1_meter = 0x020695BF,       -- P1 super meter (master)
    p2_meter = 0x020695EB,       -- P2 super meter (master)
    p1_meter_gauge = 0x020695B5, -- P1 partial bar fill
    p2_meter_gauge = 0x020695E1, -- P2 partial bar fill
    p1_meter_max = 0x020695BD,   -- P1 max meter bars
    p2_meter_max = 0x020695E9,   -- P2 max meter bars
    
    -- Stun
    p1_stun = 0x20696C5,         -- P1 stun meter
    p2_stun = 0x206961D,         -- P2 stun meter
    p1_stun_max = 0x020695F7,    -- P1 stun threshold
    p2_stun_max = 0x0206960B,    -- P2 stun threshold
    
    -- SA Selection (VERIFIED!)
    p1_sa = 0x0201138B,          -- P1 selected SA (add 1 to value)
    p2_sa = 0x0201138C,          -- P2 selected SA (add 1 to value)
    
    -- Combo counters
    p1_combo = 0x020696C5,       -- P1 current combo hits
    p2_combo = 0x0206961D,       -- P2 current combo hits
    
    -- Parry timing (for Priority 3)
    p1_parry_fwd_validity = 0x02026335,
    p1_parry_fwd_cooldown = 0x02025731,
    p1_parry_down_validity = 0x02026337,
    p1_parry_down_cooldown = 0x0202574D,
    p2_parry_fwd_validity = 0x0202673B,  -- P1 + 0x406
    p2_parry_fwd_cooldown = 0x02025D51,  -- P1 + 0x620
    
    -- Charge move addresses (character-specific, for Priority 3)
    p1_charge_1 = 0x02025A49,    -- Alex elbow, etc
    p1_charge_2 = 0x02025A2D,    -- Alex stomp, Urien knee
    p1_charge_3 = 0x020259D9,    -- Urien tackle, Chun bird, Q dash
    p2_charge_1 = 0x02025FF9,
    p2_charge_2 = 0x02026031,
    p2_charge_3 = 0x02026069,
    
    -- Screen position
    screen_x = 0x02026CB0,
    screen_y = 0x02026CB4,
}

-- Memory read macros (FBNeo Lua API)
local rb = memory.readbyte
local rw = memory.readword
local rws = memory.readwordsigned
local rd = memory.readdword

-- Read complete game state for supervised training + RL
local function read_game_state()
    local p1b = GAME_ADDRS.p1_base
    local p2b = GAME_ADDRS.p2_base
    
    -- Match state detection (from Grouflon)
    local match_state_byte = rb(GAME_ADDRS.match_state_byte)
    local p1_locked = rb(GAME_ADDRS.p1_locked)
    local p2_locked = rb(GAME_ADDRS.p2_locked)
    -- is_in_match: true when round active AND players can move (after FIGHT banner)
    local is_in_match = ((p1_locked == 0xFF or p2_locked == 0xFF) and match_state_byte == 0x02)
    
    return {
        -- Match/Round state
        match_state = match_state_byte,  -- 0x02 = round active, players can move
        is_in_match = is_in_match and 1 or 0,  -- 1 = post-FIGHT, gameplay active
        
        -- Core state
        timer = rb(GAME_ADDRS.timer),
        frame_num = rd(GAME_ADDRS.frame_number),
        
        -- Health
        p1_hp = rb(GAME_ADDRS.p1_health),
        p2_hp = rb(GAME_ADDRS.p2_health),
        
        -- Positions (verified Grouflon offsets)
        p1_x = rws(p1b + GAME_ADDRS.pos_x_off),
        p1_y = rws(p1b + GAME_ADDRS.pos_y_off),
        p2_x = rws(p2b + GAME_ADDRS.pos_x_off),
        p2_y = rws(p2b + GAME_ADDRS.pos_y_off),
        
        -- Facing
        p1_facing = rb(GAME_ADDRS.p1_direction),
        p2_facing = rb(GAME_ADDRS.p2_direction),
        
        -- Meter (verified master variables)
        p1_meter = rb(GAME_ADDRS.p1_meter),
        p2_meter = rb(GAME_ADDRS.p2_meter),
        p1_meter_gauge = rb(GAME_ADDRS.p1_meter_gauge),
        p2_meter_gauge = rb(GAME_ADDRS.p2_meter_gauge),
        
        -- Stun
        p1_stun = rb(GAME_ADDRS.p1_stun),
        p2_stun = rb(GAME_ADDRS.p2_stun),
        
        -- Character IDs
        p1_char = rw(p1b + GAME_ADDRS.char_id_off),
        p2_char = rw(p2b + GAME_ADDRS.char_id_off),
        
        -- SA Selection (VERIFIED Grouflon!)
        p1_sa = rb(GAME_ADDRS.p1_sa) + 1,  -- 1-indexed
        p2_sa = rb(GAME_ADDRS.p2_sa) + 1,
        
        -- Action/Animation (Priority 1-2)
        p1_action = rd(p1b + GAME_ADDRS.action_off),
        p2_action = rd(p2b + GAME_ADDRS.action_off),
        p1_animation = rw(p1b + GAME_ADDRS.animation_off),
        p2_animation = rw(p2b + GAME_ADDRS.animation_off),
        
        -- Priority 1: Essential state
        p1_posture = rb(p1b + GAME_ADDRS.posture_off),
        p2_posture = rb(p2b + GAME_ADDRS.posture_off),
        p1_recovery = rb(p1b + GAME_ADDRS.recovery_off),
        p2_recovery = rb(p2b + GAME_ADDRS.recovery_off),
        p1_attacking = rb(p1b + GAME_ADDRS.attacking_off),
        p2_attacking = rb(p2b + GAME_ADDRS.attacking_off),
        
        -- Priority 2: Combat state
        p1_freeze = rb(p1b + GAME_ADDRS.freeze_off),
        p2_freeze = rb(p2b + GAME_ADDRS.freeze_off),
        p1_thrown = rb(p1b + GAME_ADDRS.thrown_off),
        p2_thrown = rb(p2b + GAME_ADDRS.thrown_off),
        p1_blocking = rb(p1b + GAME_ADDRS.blocking_off),
        p2_blocking = rb(p2b + GAME_ADDRS.blocking_off),
        p1_hit_count = rb(p1b + GAME_ADDRS.hit_count_off),
        p2_hit_count = rb(p2b + GAME_ADDRS.hit_count_off),
        p1_standing = rb(p1b + GAME_ADDRS.standing_state_off),
        p2_standing = rb(p2b + GAME_ADDRS.standing_state_off),
        
        -- Combo counters
        p1_combo = rb(GAME_ADDRS.p1_combo),
        p2_combo = rb(GAME_ADDRS.p2_combo),
        
        -- Priority 3: Parry timing
        p1_parry_fwd = rb(GAME_ADDRS.p1_parry_fwd_validity),
        p1_parry_down = rb(GAME_ADDRS.p1_parry_down_validity),
        p2_parry_fwd = rb(GAME_ADDRS.p2_parry_fwd_validity),
        
        -- Priority 3: Charge timers (character-specific)
        p1_charge1 = rb(GAME_ADDRS.p1_charge_1),
        p1_charge2 = rb(GAME_ADDRS.p1_charge_2),
        p1_charge3 = rb(GAME_ADDRS.p1_charge_3),
        p2_charge1 = rb(GAME_ADDRS.p2_charge_1),
        p2_charge2 = rb(GAME_ADDRS.p2_charge_2),
        p2_charge3 = rb(GAME_ADDRS.p2_charge_3),
        
        -- Screen
        screen_x = rws(GAME_ADDRS.screen_x),
        screen_y = rws(GAME_ADDRS.screen_y),
    }
end

-- CPS3 Character ID to name mapping (EMPIRICALLY VERIFIED)
-- Note: CPS3 IDs differ from PS2 decomp! Add mappings as you test replays.
local CHAR_NAMES = {
    -- Verified mappings from Fightcade replay testing
    [11] = "Ken",
    [16] = "ChunLi",
    
    -- Unverified (copied from PS2, may need adjustment)
    [0] = "Gill", [1] = "Alex", [2] = "Ryu", [3] = "Yun", [4] = "Dudley",
    [5] = "Necro", [6] = "Hugo", [7] = "Ibuki", [8] = "Elena", [9] = "Oro",
    [10] = "Yang", [12] = "Sean", [13] = "Urien", [14] = "Akuma", 
    [15] = "Makoto",
    [17] = "Q", [18] = "Twelve", [19] = "Remy"
}

local function get_char_name(id)
    return CHAR_NAMES[id] or string.format("Char%d", id)
end

-- Find next available file number for a matchup
local function get_next_file_number(base_path)
    local num = 1
    while true do
        local test_path = string.format("%s_%03d_full.csv", base_path, num)
        local f = io.open(test_path, "r")
        if f then
            f:close()
            num = num + 1
        else
            return num
        end
    end
end

-- Storage (forward declared for state machine)
local captured = {}

-- =============================================================================
-- GAME STATE MACHINE
-- Uses timer and HP to properly track: MENU -> PLAYING -> ROUND_END -> cycle
-- T=99 = new round starting (most reliable indicator)
-- HP=0 = round ended (KO)
-- Timer=0 or >99 = menu/transition
-- =============================================================================

-- Game phases
local PHASE_MENU = 1        -- In menus, char select, or replay not started
local PHASE_FIGHT_INTRO = 2 -- FIGHT! animation (timer=99, HP=160, but combat not live)
local PHASE_PLAYING = 3     -- Active gameplay (timer counting down)
local PHASE_ROUND_END = 4   -- Round ended (someone KO'd), waiting for next round

local current_phase = PHASE_MENU
local prev_timer = 0
local rounds_seen = 0
local match_num = 1  -- Track matches within replay
local menu_frames = 0  -- Frames spent in menu state
local MAX_HP = 160  -- 0xA0 = max health in SF3
local timer_started_counting = false  -- Track if timer has started counting down this round

-- Track wins for match end detection (SF3 is best of 3 rounds per game)
local p1_wins = 0
local p2_wins = 0
local last_valid_hp = {p1 = 160, p2 = 160}  -- Track last valid HP during gameplay for fallback

local function get_game_phase()
    local timer = rb(GAME_ADDRS.timer)
    local p1_hp = rb(GAME_ADDRS.p1_health)
    local p2_hp = rb(GAME_ADDRS.p2_health)
    
    -- Sanity check: HP > 160 means garbage data (not in real match)
    local hp_valid = (p1_hp <= MAX_HP) and (p2_hp <= MAX_HP)
    
    -- Timer 1-98 with valid HP = definitely active gameplay (combat is live)
    if timer >= 1 and timer <= 98 and hp_valid and p1_hp > 0 and p2_hp > 0 then
        return PHASE_PLAYING, timer, p1_hp, p2_hp, true  -- live=true
    end
    
    -- Timer=99 with full HP = could be FIGHT! intro or just started
    if timer == 99 and hp_valid and p1_hp >= 160 and p2_hp >= 160 then
        -- If timer has previously dropped below 99 this round, we're back in another round
        -- Otherwise this is FIGHT! intro
        if timer_started_counting then
            return PHASE_FIGHT_INTRO, timer, p1_hp, p2_hp, false  -- live=false
        else
            return PHASE_FIGHT_INTRO, timer, p1_hp, p2_hp, false  -- live=false
        end
    end
    
    -- Timer=99, HP damaged = combat is happening even at T=99 edge case
    if timer == 99 and hp_valid and (p1_hp < 160 or p2_hp < 160) and p1_hp > 0 and p2_hp > 0 then
        return PHASE_PLAYING, timer, p1_hp, p2_hp, true  -- live=true (HP changed = combat)
    end
    
    -- Timer 1-99, valid HP, but someone has 0 HP = round just ended (KO animation)
    if timer >= 1 and timer <= 99 and hp_valid and (p1_hp == 0 or p2_hp == 0) then
        return PHASE_ROUND_END, timer, p1_hp, p2_hp, false  -- live=false
    end
    
    -- Timer=0 with valid HP = timeout! Round ended by time
    if timer == 0 and hp_valid and p1_hp > 0 and p2_hp > 0 then
        return PHASE_ROUND_END, timer, p1_hp, p2_hp, false  -- live=false (timeout)
    end
    
    -- Timer 0, >99, or invalid HP = menu/transition state
    return PHASE_MENU, timer, p1_hp, p2_hp, false  -- live=false
end

local function update_game_phase()
    local new_phase, timer, p1_hp, p2_hp, live = get_game_phase()
    
    -- Check if gameplay became active this round (using is_in_match from Grouflon)
    -- This is more accurate than timer < 99 - it detects when FIGHT banner ends
    local gs = read_game_state()
    if gs.is_in_match == 1 and not timer_started_counting then
        timer_started_counting = true
        print(string.format("Gameplay active at frame %d (T:%d, match_state=0x02)", #captured, timer))
    end
    
    -- Detect phase transitions
    if new_phase ~= current_phase then
        local old_name = ({[1]="MENU", [2]="INTRO", [3]="PLAYING", [4]="ROUND_END"})[current_phase]
        local new_name = ({[1]="MENU", [2]="INTRO", [3]="PLAYING", [4]="ROUND_END"})[new_phase]
        
        -- Transitioning TO playing/intro state (new round)
        if new_phase == PHASE_FIGHT_INTRO or new_phase == PHASE_PLAYING then
            -- Only count as new round if we came from MENU or ROUND_END
            if current_phase == PHASE_MENU or current_phase == PHASE_ROUND_END then
                rounds_seen = rounds_seen + 1
                timer_started_counting = false -- Reset for new round
                
                -- Read and display character names for verification
                local gs = read_game_state()
                local p1_name = get_char_name(gs.p1_char)
                local p2_name = get_char_name(gs.p2_char)
                print(string.format("=== MATCH %d ROUND %d: %s (P1:%d) vs %s (P2:%d) T:%d frame:%d ===", 
                      match_num, rounds_seen, p1_name, gs.p1_char, p2_name, gs.p2_char, timer, #captured))
            end
        end
        
        -- Transitioning TO round end (KO or timeout)
        if new_phase == PHASE_ROUND_END then
            local winner, reason
            if p1_hp == 0 then
                winner = "P2"
                reason = "KO"
                p2_wins = p2_wins + 1
            elseif p2_hp == 0 then
                winner = "P1"
                reason = "KO"
                p1_wins = p1_wins + 1
            elseif timer == 0 then
                -- Timeout: winner has more HP
                if p1_hp > p2_hp then
                    winner = "P1"
                    p1_wins = p1_wins + 1
                elseif p2_hp > p1_hp then
                    winner = "P2"
                    p2_wins = p2_wins + 1
                else
                    winner = "DRAW"
                end
                reason = "TIMEOUT"
            else
                winner = p1_hp > p2_hp and "P1" or "P2"
                reason = "?"
            end
            print(string.format("Round ended: %s wins by %s (T:%d, HP:%d/%d, Score:%d-%d) at frame %d", 
                  winner, reason, timer, p1_hp, p2_hp, p1_wins, p2_wins, #captured))
            
            -- Check for match end (best of 3: first to 2 wins)
            if p1_wins >= 2 or p2_wins >= 2 then
                local match_winner = p1_wins >= 2 and "P1" or "P2"
                print(string.format("=== MATCH %d OVER: %s wins %d-%d ===", match_num, match_winner, p1_wins, p2_wins))
                -- Reset counters for next match
                p1_wins = 0
                p2_wins = 0
                rounds_seen = 0  -- Reset round counter for new match
                match_num = match_num + 1
            end
        end
        
        -- Transitioning TO menu from PLAYING (missed round end - likely timeout or perfect KO)
        if new_phase == PHASE_MENU and current_phase == PHASE_PLAYING then
            -- Infer round end from last known HP (with sanity check)
            local gs = read_game_state()
            local use_p1_hp, use_p2_hp = gs.p1_hp, gs.p2_hp
            
            -- If current HP is garbage, use last valid HP from gameplay
            if gs.p1_hp > MAX_HP or gs.p2_hp > MAX_HP then
                use_p1_hp = last_valid_hp.p1
                use_p2_hp = last_valid_hp.p2
                print(string.format("Using fallback HP: %d/%d (current invalid: %d/%d)", 
                      use_p1_hp, use_p2_hp, gs.p1_hp, gs.p2_hp))
            end
            
            -- Count the win
            if use_p1_hp > use_p2_hp or use_p2_hp == 0 then
                p1_wins = p1_wins + 1
                print(string.format("Round ended (inferred): P1 wins (HP:%d/%d, Score:%d-%d) at frame %d", 
                      use_p1_hp, use_p2_hp, p1_wins, p2_wins, #captured))
            elseif use_p2_hp > use_p1_hp or use_p1_hp == 0 then
                p2_wins = p2_wins + 1
                print(string.format("Round ended (inferred): P2 wins (HP:%d/%d, Score:%d-%d) at frame %d", 
                      use_p1_hp, use_p2_hp, p1_wins, p2_wins, #captured))
            end
            
            if p1_wins >= 2 or p2_wins >= 2 then
                local match_winner = p1_wins >= 2 and "P1" or "P2"
                print(string.format("=== MATCH %d OVER: %s wins %d-%d ===", match_num, match_winner, p1_wins, p2_wins))
                -- Reset counters for next match
                p1_wins = 0
                p2_wins = 0
                rounds_seen = 0  -- Reset round counter for new match
                match_num = match_num + 1
            end
        end
        
        -- Generic menu transition log
        if new_phase == PHASE_MENU then
            print(string.format("Menu/transition state entered at frame %d", #captured))
        end
        
        current_phase = new_phase
    end
    
    -- Track time spent in menu state and update last valid HP
    if current_phase == PHASE_MENU then
        menu_frames = menu_frames + 1
    else
        menu_frames = 0
        -- Track HP while playing for fallback during transitions
        if current_phase == PHASE_PLAYING then
            local gs = read_game_state()
            if gs.p1_hp <= MAX_HP and gs.p2_hp <= MAX_HP then
                last_valid_hp = {p1 = gs.p1_hp, p2 = gs.p2_hp}
            end
        end
    end
    
    prev_timer = timer
    return current_phase
end

local function get_game_state_str()
    local phase, timer, p1_hp, p2_hp, live = get_game_phase()
    local live_str = live and "LIVE" or "WAIT"
    
    if phase == PHASE_PLAYING then
        return string.format("R%d %s T:%d HP:%d/%d", rounds_seen, live_str, timer, p1_hp, p2_hp)
    elseif phase == PHASE_FIGHT_INTRO then
        return string.format("R%d INTRO T:%d", rounds_seen, timer)
    elseif phase == PHASE_ROUND_END then
        return string.format("KO T:%d HP:%d/%d", timer, p1_hp, p2_hp)
    else
        return "MENU"
    end
end




-- Detect replay mode
local IS_REPLAY = false
if emu.isreplay then
    IS_REPLAY = emu.isreplay()
    print("Replay mode: " .. (IS_REPLAY and "YES" or "NO"))
else
    print("Note: emu.isreplay() not available, assuming live capture")
end

-- Input mapping for SF3:3S (CPS3)
-- joypad.get() returns keys like "P1 Up", "P1 Weak Punch", etc.
local INPUTS = {
    up = " Up",
    down = " Down", 
    left = " Left",
    right = " Right",
    lp = " Weak Punch",
    mp = " Medium Punch",
    hp = " Strong Punch",
    lk = " Weak Kick",
    mk = " Medium Kick",
    hk = " Strong Kick",
    start = " Start",
    coin = " Coin",
}

-- Pack inputs to 16-bit value matching rl_bridge.h / core.py
-- Bits 0-3: U,D,L,R | Bits 4-6: LP,MP,HP | Bit 7: unused | Bits 8-10: LK,MK,HK | Bit 11: Start
local function pack_inputs(prefix, inputs)
    local value = 0
    if inputs[prefix .. INPUTS.up] then value = value + 0x0001 end     -- bit 0
    if inputs[prefix .. INPUTS.down] then value = value + 0x0002 end   -- bit 1
    if inputs[prefix .. INPUTS.left] then value = value + 0x0004 end   -- bit 2
    if inputs[prefix .. INPUTS.right] then value = value + 0x0008 end  -- bit 3
    if inputs[prefix .. INPUTS.lp] then value = value + 0x0010 end     -- bit 4
    if inputs[prefix .. INPUTS.mp] then value = value + 0x0020 end     -- bit 5
    if inputs[prefix .. INPUTS.hp] then value = value + 0x0040 end     -- bit 6
    -- bit 7 unused (0x0080 skipped)
    if inputs[prefix .. INPUTS.lk] then value = value + 0x0100 end     -- bit 8
    if inputs[prefix .. INPUTS.mk] then value = value + 0x0200 end     -- bit 9
    if inputs[prefix .. INPUTS.hk] then value = value + 0x0400 end     -- bit 10
    if inputs[prefix .. INPUTS.start] then value = value + 0x0800 end  -- bit 11
    return value
end

-- Storage (captured already declared at line 97)
local start_frame = -1
local last_frame = -1
local is_done = false
local stall_count = 0

-- Hit detection: track previous HP to detect damage
local prev_p1_hp = 160
local prev_p2_hp = 160

-- Capture current frame inputs AND game state
local function capture_frame()
    local frame = emu.framecount()
    if frame == last_frame then return end
    last_frame = frame
    
    if start_frame == -1 then start_frame = frame end
    local rel_frame = frame - start_frame
    
    local inputs = joypad.get()
    local p1 = pack_inputs("P1", inputs)
    local p2 = pack_inputs("P2", inputs)
    
    -- Read complete game state for supervised training
    local gs = read_game_state()
    local _, _, _, _, live = get_game_phase() -- Get live status
    
    -- Hit confirmation: detect HP changes (positive = took damage)
    local p1_hit = (prev_p1_hp > gs.p1_hp) and (prev_p1_hp - gs.p1_hp) or 0
    local p2_hit = (prev_p2_hp > gs.p2_hp) and (prev_p2_hp - gs.p2_hp) or 0
    prev_p1_hp = gs.p1_hp
    prev_p2_hp = gs.p2_hp
    
    table.insert(captured, {
        frame = rel_frame,
        p1 = p1, p2 = p2,
        -- Match/Round state
        match_state = gs.match_state,    -- 0x02 = round active
        is_in_match = gs.is_in_match,    -- 1 = post-FIGHT, gameplay active
        -- Core game state
        timer = gs.timer,
        p1_hp = gs.p1_hp, p2_hp = gs.p2_hp,
        p1_x = gs.p1_x, p1_y = gs.p1_y,
        p2_x = gs.p2_x, p2_y = gs.p2_y,
        p1_facing = gs.p1_facing, p2_facing = gs.p2_facing,
        p1_meter = gs.p1_meter, p2_meter = gs.p2_meter,
        p1_stun = gs.p1_stun, p2_stun = gs.p2_stun,
        p1_char = gs.p1_char, p2_char = gs.p2_char,
        live = live and 1 or 0,
        
        -- SA Selection (VERIFIED!)
        p1_sa = gs.p1_sa, p2_sa = gs.p2_sa,
        
        -- Priority 1: Essential state
        p1_action = gs.p1_action, p2_action = gs.p2_action,
        p1_posture = gs.p1_posture, p2_posture = gs.p2_posture,
        p1_recovery = gs.p1_recovery, p2_recovery = gs.p2_recovery,
        p1_attacking = gs.p1_attacking, p2_attacking = gs.p2_attacking,
        
        -- Priority 2: Combat state
        p1_animation = gs.p1_animation, p2_animation = gs.p2_animation,
        p1_freeze = gs.p1_freeze, p2_freeze = gs.p2_freeze,
        p1_thrown = gs.p1_thrown, p2_thrown = gs.p2_thrown,
        p1_blocking = gs.p1_blocking, p2_blocking = gs.p2_blocking,
        p1_combo = gs.p1_combo, p2_combo = gs.p2_combo,
        p1_standing = gs.p1_standing, p2_standing = gs.p2_standing,
        
        -- Priority 3: Parry and charge
        p1_parry_fwd = gs.p1_parry_fwd, p1_parry_down = gs.p1_parry_down,
        p2_parry_fwd = gs.p2_parry_fwd,
        p1_charge1 = gs.p1_charge1, p1_charge2 = gs.p1_charge2, p1_charge3 = gs.p1_charge3,
        p2_charge1 = gs.p2_charge1, p2_charge2 = gs.p2_charge2, p2_charge3 = gs.p2_charge3,
        
        -- Hit confirmation
        p1_hit = p1_hit, p2_hit = p2_hit,
    })
    
    -- Progress report every 3000 frames (~50 sec)
    if rel_frame % 3000 == 0 and rel_frame > 0 then
        print(string.format("Captured %d frames (%.0fs)", rel_frame, rel_frame / 60))
    end
end

-- Save captured data
local function save_data()
    if is_done or #captured == 0 then return end
    is_done = true
    
    -- Get character names from FIRST LIVE gameplay frame (not intro garbage)
    local p1_char_id, p2_char_id = 0, 0
    for _, f in ipairs(captured) do
        if f.live == 1 and f.p1_char and f.p2_char then
            p1_char_id = f.p1_char
            p2_char_id = f.p2_char
            break
        end
    end
    
    local p1_name = get_char_name(p1_char_id)
    local p2_name = get_char_name(p2_char_id)
    local matchup_base = CONFIG.output_dir .. p1_name .. "_vs_" .. p2_name
    
    -- Find next available file number
    local file_num = get_next_file_number(matchup_base)
    local csv_path = string.format("%s_%03d_full.csv", matchup_base, file_num)
    
    -- Full game state CSV output for supervised learning + RL
    -- 54 columns: match state + core state + Priority 1/2/3 fields
    local csv = io.open(csv_path, "w")
    csv:write("frame,p1_input,p2_input,match_state,is_in_match,timer,p1_hp,p2_hp,p1_x,p1_y,p2_x,p2_y,p1_facing,p2_facing,p1_meter,p2_meter,p1_stun,p2_stun,p1_char,p2_char,live,p1_sa,p2_sa,p1_action,p2_action,p1_posture,p2_posture,p1_recovery,p2_recovery,p1_attacking,p2_attacking,p1_animation,p2_animation,p1_freeze,p2_freeze,p1_thrown,p2_thrown,p1_blocking,p2_blocking,p1_combo,p2_combo,p1_standing,p2_standing,p1_parry_fwd,p1_parry_down,p2_parry_fwd,p1_charge1,p1_charge2,p1_charge3,p2_charge1,p2_charge2,p2_charge3,p1_hit,p2_hit\n")
    for _, f in ipairs(captured) do
        csv:write(string.format("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
            f.frame, f.p1, f.p2,
            f.match_state, f.is_in_match,
            f.timer,
            f.p1_hp, f.p2_hp,
            f.p1_x, f.p1_y, f.p2_x, f.p2_y,
            f.p1_facing, f.p2_facing,
            f.p1_meter, f.p2_meter,
            f.p1_stun, f.p2_stun,
            f.p1_char, f.p2_char,
            f.live,
            f.p1_sa, f.p2_sa,
            f.p1_action, f.p2_action,
            f.p1_posture, f.p2_posture,
            f.p1_recovery, f.p2_recovery,
            f.p1_attacking, f.p2_attacking,
            f.p1_animation, f.p2_animation,
            f.p1_freeze, f.p2_freeze,
            f.p1_thrown, f.p2_thrown,
            f.p1_blocking, f.p2_blocking,
            f.p1_combo, f.p2_combo,
            f.p1_standing, f.p2_standing,
            f.p1_parry_fwd, f.p1_parry_down, f.p2_parry_fwd,
            f.p1_charge1, f.p1_charge2, f.p1_charge3,
            f.p2_charge1, f.p2_charge2, f.p2_charge3,
            f.p1_hit, f.p2_hit
        ))
    end
    csv:close()
    
    print("")
    print("============================================")
    print("  SAVED!")
    print("  CSV: " .. csv_path)
    print("  Frames: " .. #captured)
    print("  Duration: " .. string.format("%.1f", #captured / 60) .. " seconds")
    print("============================================")
    
    -- Calculate input stats
    local p1_active, p2_active = 0, 0
    for _, f in ipairs(captured) do
        if f.p1 > 0 then p1_active = p1_active + 1 end
        if f.p2 > 0 then p2_active = p2_active + 1 end
    end
    print(string.format("P1 active: %d (%.1f%%)", p1_active, p1_active / #captured * 100))
    print(string.format("P2 active: %d (%.1f%%)", p2_active, p2_active / #captured * 100))
    
    -- Write status file for batch orchestration
    if CONFIG.write_status_file then
        local status_path = string.format("%s_%03d_status.txt", matchup_base, file_num)
        local status_file = io.open(status_path, "w")
        if status_file then
            status_file:write("COMPLETE\n")
            status_file:write("frames=" .. #captured .. "\n")
            status_file:write("csv=" .. csv_path .. "\n")
            status_file:write("p1=" .. p1_name .. "\n")
            status_file:write("p2=" .. p2_name .. "\n")
            status_file:close()
            print("Status file: " .. status_path)
        end
    end
    
    -- Auto-exit emulator for batch processing
    if CONFIG.auto_exit_on_end then
        print("Auto-exiting emulator...")
        -- Give a moment for file writes to complete
        for i = 1, 60 do end  -- Brief delay
        os.execute("taskkill /F /IM fcadefbneo.exe")
    end
end

-- Detect replay end using game state machine
-- Only ends when stuck in MENU state for extended time (true replay end)
local MIN_FRAMES_BEFORE_END_CHECK = 1000  -- ~17 seconds before checking for end
local MENU_TIMEOUT_FRAMES = 3600  -- 60 seconds in MENU = replay ended
local prev_check_frame = 0  -- Separate frame tracker for stall detection

local function check_replay_end()
    -- Don't check until we have enough data
    if #captured < MIN_FRAMES_BEFORE_END_CHECK then
        return
    end
    
    -- Use menu_frames from state machine
    -- Only trigger end when in MENU state for extended period
    if current_phase == PHASE_MENU and menu_frames > MENU_TIMEOUT_FRAMES then
        print(string.format("Replay ended (in menu for %ds after %d frames, %d rounds)", 
              math.floor(menu_frames / 60), #captured, rounds_seen))
        save_data()
        return
    end
    
    -- Also handle literal frame stall (FBNeo froze)
    -- Use prev_check_frame which is NOT updated by capture_frame()
    local frame = emu.framecount()
    if frame == prev_check_frame then
        stall_count = stall_count + 1
        if stall_count > 7200 then  -- 2 minutes of no frame progress
            print(string.format("Replay ended (emulator stalled after %d frames)", #captured))
            save_data()
        end
    else
        stall_count = 0
    end
    prev_check_frame = frame
end

-- Main frame callback
local function on_frame()
    if is_done then return end
    
    -- Update game state machine first
    update_game_phase()
    
    -- Capture frame data
    capture_frame()
    
    -- Check for replay end
    if CONFIG.auto_save_on_end then
        check_replay_end()
    end
    
    -- HUD shows round number, timer, HP
    if CONFIG.show_hud then
        local state = get_game_state_str()
        gui.text(2, 2, string.format("REC: %d (%.1fm) | %s", 
                 #captured, #captured/3600, state), 0x00FF00)
    end
end

emu.registerafter(on_frame)

-- Auto-save on script stop/close
if emu.registerexit then
    emu.registerexit(function()
        if not is_done and #captured > 0 then
            print("Script stopping - auto-saving...")
            save_data()
        end
    end)
end

-- Hotkeys and turbo mode
local prev_keys = {}
local turbo_active = false
gui.register(function()
    local keys = input.get()
    
    -- F12: Force save
    if keys.F12 and not prev_keys.F12 then
        save_data()
    end
    
    -- F1: Toggle turbo mode (press to toggle, not hold)
    -- When auto_turbo is enabled, turbo stays on by default
    if CONFIG.turbo_mode then
        if keys.F1 and not prev_keys.F1 then
            -- Toggle turbo on F1 press
            turbo_active = not turbo_active
            if turbo_active then
                if emu.speedmode then
                    emu.speedmode("turbo")
                elseif fba and fba.speedmode then
                    fba.speedmode("turbo")
                end
                print("Turbo: ON")
            else
                if emu.speedmode then
                    emu.speedmode("normal")
                elseif fba and fba.speedmode then
                    fba.speedmode("normal")
                end
                print("Turbo: OFF")
            end
        end
    end
    
    prev_keys = keys
end)

-- Startup message
print("")
if IS_REPLAY then
    print("Replay detected - capturing inputs automatically")
else
    print("Live mode - capturing inputs for training")
end
print("Press F12 to force-save at any time")
print("Hold F1 for turbo speed")
if CONFIG.auto_exit_on_end then
    print("Auto-exit: ENABLED (will close emulator when done)")
end
print("")

-- Auto-enable turbo mode on startup for batch processing
if CONFIG.auto_turbo and CONFIG.turbo_mode then
    print("Auto-turbo: Enabling fast-forward...")
    if emu.speedmode then
        emu.speedmode("turbo")
    elseif fba and fba.speedmode then
        fba.speedmode("turbo")
    end
    turbo_active = true
end
