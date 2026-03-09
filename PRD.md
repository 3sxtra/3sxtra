# PRD: Advanced Training Mode — Fix Lua Dummy Integration

## Overview
Static analysis revealed two root causes preventing the Lua dummy from working:

1. **The C dummy overwrites Lua.** In `plcnt_move()`, `lua_engine_bridge_tick()` runs first (line 576→259), then `Player_move()` runs later (line 914→47→184). Inside `CPU_Sub()`, `Lever_Buff[id]` is cleared to 0 (line 191) then `training_dummy_update_input()` overwrites it (line 205). All Lua output is destroyed.

2. **12+ derived player fields are nil.** `engine_gamestate.lua` (360 LOC) is a minimal shim that copies raw C bridge fields. The original `gamestate.lua` (1779 LOC) computes dozens of derived fields (`standing_state`, `is_waking_up`, `has_just_been_thrown`, `can_fast_wakeup`, `idle_time`, `stun_just_began`, etc.) that `dummy_control.lua` depends on. These fields are `nil` at runtime, silently breaking every advanced feature.

### Reference
- Detailed findings: see `training_audit.md` in brain artifacts
- Original gamestate: `src/lua/3rd_training_lua-main/src/gamestate.lua` (1779 LOC)
- Compat shim: `src/lua/compat/engine_gamestate.lua` (360 LOC)
- C overwrite path: `com_pl.c:191,205` → `CPU_Sub()`

---

## Task 1: Stop C Dummy From Overwriting Lua Output
`CPU_Sub()` clears `Lever_Buff[id] = 0` (com_pl.c:191) then calls `training_dummy_update_input()` (com_pl.c:205), destroying whatever Lua wrote.

**Action:**
1. In `com_pl.c`, skip BOTH the `Lever_Buff[id] = 0` clear AND `training_dummy_update_input()` when Lua dummy is active.
2. Add `bool g_lua_dummy_active` to `training_state.h`. Set to `true` by `training_main.lua` via a new bridge function `engine.set_lua_dummy_active(true)`.
3. In `CPU_Sub()`, gate the Lever_Buff clear and training_dummy_update_input call: `if (!g_lua_dummy_active) { ... }`
4. Keep the C dummy as fallback when Lua doesn't load.

**Acceptance:**
- When Lua is active, Lua's `Lever_Buff` writes survive to `Player_move()`.
- When Lua is not active, C dummy still functions.
- `.\recompile.bat` passes.

---

## Task 2: Add Missing C Bridge Fields
`engine_gamestate.lua` needs three raw fields from `l_read_player()` that are not currently exposed:

**Action:**
1. Find the correct struct offsets for these fields in PLW/WORK:
   - `standing_state` — the original gamestate reads this from base+0x204
   - `can_fast_wakeup` — original reads from base+0x1B3
   - `received_connection_marker` — for hit confirmation
2. Add `PUSH_INT()` lines to `l_read_player()` in `lua_engine_bridge.cpp`.
3. Verify field names match what `engine_gamestate.lua` will use.

**Acceptance:**
- `engine.read_player(id)` returns the three new fields.
- `.\recompile.bat` passes.

---

## Task 3: Add Missing Derived Fields to engine_gamestate.lua
The compat shim needs to compute derived fields that `dummy_control.lua` reads.

**Action:**
Add to `update_player()` in `engine_gamestate.lua`:

1. **`standing_state`**: read from the new bridge field (Task 2).
2. **`previous_posture`**: store `player.posture` before overwriting with new value.
3. **`is_waking_up`**: `= player.posture == 0x26`
4. **`has_just_been_thrown`**: track `is_being_thrown` transition (false→true).
5. **`idle_time`**: counter, increment when idle, reset otherwise.
6. **`stun_just_began`**: track `is_stunned` transition (false→true).
7. **`just_received_connection`**: derive from new `received_connection_marker` field or `hit_flag` changes.
8. **`can_fast_wakeup`** / **`previous_can_fast_wakeup`**: read from new bridge field, track transitions.
9. **`is_past_fast_wakeup_frame`**: derive from `can_fast_wakeup` transition (1→0).
10. **`has_just_landed`**: derive from `standing_state` transition (air→ground).

Reference: original `gamestate.lua` lines 481-934 for exact computation logic.

**Acceptance:**
- All fields return correct values (not nil) when checked in Lua.
- `dummy_control.update_pose()` correctly detects ground state.

---

## Task 4: Wire Menu Settings to Lua Dummy
`training_main.lua` hardcodes all settings to OFF:
```lua
dummy_control.update_mash_inputs(input, dummy_player, 1)  -- OFF
dummy_control.update_fast_wake_up(input, dummy_player, 1) -- OFF
dummy_control.update_tech_throws(input, dummy_player, 1)  -- OFF
```

**Action:**
1. Add `engine.get_dummy_settings()` to `lua_engine_bridge.cpp` that reads `g_dummy_settings`.
2. In `training_main.lua`, call it each frame and map C enum values to Lua enum values.
3. Pass mapped values to `dummy_control.update_*()` calls instead of hardcoded `1`.

**Acceptance:**
- Changing menu settings changes Lua dummy behavior.
- `.\recompile.bat` passes.

---

## Task 5: Add Missing Menu Items
The Lua dummy supports features not in the C training menu.

**Action:**
1. Add `tech_throw_type`, `fast_wakeup`, `counter_attack_type` fields to `DummySettings`.
2. Add menu mappings in `sync_dummy_settings_from_menu()`.
3. Expose all new fields in `engine.get_dummy_settings()`.
4. Add the `DUMMY_BLOCK_AFTER_FIRST_HIT` enum value (Lua has both FIRST_HIT and AFTER_FIRST_HIT).
5. Fix mash enum overflow (values 4-6 exceed `DummyMashType`).

**Acceptance:**
- All Lua dummy features configurable from training menu.

---

## Task 6: Final Verification

1. `.\lint.bat`
2. `.\recompile.bat`
3. `cd build_tests && ctest --output-on-failure`

**Acceptance:**
- All pass. No regressions.
