# CPS3 SF3 3rd Strike — FBNeo Lua Script with Memory Access

## Overview

Street Fighter III: 3rd Strike running on FBNeo (FinalBurn Neo) supports Lua scripting with full memory read/write access. This enables training modes, hitbox viewers, and custom tools. FBNeo's Lua API provides functions like `memory.readbyte()`, `memory.writeword()`, and `memory.readdword()` that map directly to the CPS3 program RAM at addresses beginning with `0x02xxxxxx`. The two major community scripts — Grouflon's `3rd_training_lua` and peon2's `fbneo-training-mode` — are the primary references for documented memory addresses.[^1][^2][^3][^4]

## FBNeo Lua Memory API

FBNeo exposes the following memory access functions for Lua scripts:[^5][^3]

### Read Functions

| Function | Description |
|----------|-------------|
| `memory.readbyte(addr)` | Read unsigned 8-bit value |
| `memory.readbytesigned(addr)` | Read signed 8-bit value |
| `memory.readword(addr)` | Read unsigned 16-bit value |
| `memory.readwordsigned(addr)` | Read signed 16-bit value |
| `memory.readdword(addr)` | Read unsigned 32-bit value |
| `memory.readdwordsigned(addr)` | Read signed 32-bit value |
| `memory.readbyterange(addr, len)` | Read a range of bytes |

### Write Functions

| Function | Description |
|----------|-------------|
| `memory.writebyte(addr, val)` | Write 8-bit value |
| `memory.writeword(addr, val)` | Write 16-bit value |
| `memory.writedword(addr, val)` | Write 32-bit value |

**Note:** FBNeo (unlike MAME) cannot read from the `0x04xxxxxx` address range. The hitbox viewer script works around this by using an alternate scale address in the `0x02xxxxxx` range.[^1]

## SF3 3rd Strike Memory Map (sfiii3)

All addresses below are for the Japanese revision (sfiii3 / sfiii3nr1) ROM running on FBNeo.[^2][^6]

### Player Base Addresses

The player object structs are the foundation for accessing character state. All per-player offsets below are relative to these base addresses:[^6][^1]

| Player | Base Address |
|--------|-------------|
| Player 1 | `0x02068C6C` |
| Player 2 | `0x02069104` |

### Player Object Offsets (from base)

These offsets apply to both P1 and P2 base addresses:

| Offset | Size | Description |
|--------|------|-------------|
| `+0x01` | byte | Friends flag (Yang SA3 shadows) |
| `+0x0A` | byte (signed) | Flip X (sprite facing; left by default) |
| `+0x20E` | byte | Posture |
| `+0x27` | byte | Character state byte (Hugo claps/throws detection) |
| `+0x3B` | byte | Recovery flag |
| `+0x45` | byte | Remaining freeze frames (hitstop) |
| `+0x64` | word (signed) | Position X |
| `+0x68` | word (signed) | Position Y |
| `+0x8D` | byte | Flying down flag |
| `+0x9F` | byte | Life (health) |
| `+0x0AC` | dword | Action |
| `+0x0AD` | byte | Movement type |
| `+0x0AF` | byte | Movement type 2 (basic movement state) |
| `+0x12C` | dword | Action extended |
| `+0x12F` | byte | Air recovery flag 1 |
| `+0x187` | byte | Recovery time |
| `+0x189` | byte | Hit count |
| `+0x17B` | byte | Connected action count |
| `+0x202` | word | Animation ID |
| `+0x214` | byte | Frame ID 2 |
| `+0x21A` | word | Animation frame ID |
| `+0x297` | byte | Standing state (0x01=standing, 0x02=crouching) |
| `+0x2A0` | dword | Validity check (0 = invalid object) |
| `+0x32E` | word | Received connection marker |
| `+0x33E` | word | Total received hit count |
| `+0x3C0` | word | Character ID |
| `+0x3CF` | byte | Being thrown flag |
| `+0x3D1` | word | Busy flag |
| `+0x3D3` | byte | Blocking ID |
| `+0x402` | byte | Can fast wakeup |
| `+0x403` | byte | Fast wakeup flag |
| `+0x428` | byte | Is attacking byte |
| `+0x429` | byte | Is attacking ext byte |
| `+0x430` | word | Total received projectiles count |
| `+0x434` | byte | Throw countdown |
| `+0x43A` | word | Damage bonus |
| `+0x43E` | word | Stun bonus |
| `+0x440` | word | Defense bonus |
| `+0x459` | byte | Action count |
| `+0x46C` | word | Input capacity |

### Gauge, Meter, and Stun Addresses

These addresses are not relative to the player base — they are absolute:[^6]

| Address | Size | Description |
|---------|------|-------------|
| `0x020695B5` | byte | P1 Super gauge |
| `0x020695BF` | byte | P1 Meter count (master) |
| `0x020286AB` | byte | P1 Meter (secondary) |
| `0x020695B3` | byte | P1 Max meter gauge |
| `0x020695BD` | byte | P1 Max meter count |
| `0x020695F7` | byte | P1 Stun max |
| `0x020695F9` | byte | P1 Stun timer |
| `0x020695FD` | byte | P1 Stun bar |
| `0x020695E1` | byte | P2 Super gauge |
| `0x020695EB` | byte | P2 Meter count (master) |
| `0x020286DF` | byte | P2 Meter (secondary) |
| `0x020695DF` | byte | P2 Max meter gauge |
| `0x020695E9` | byte | P2 Max meter count |
| `0x0206960B` | byte | P2 Stun max |
| `0x0206960D` | byte | P2 Stun timer |
| `0x02069611` | byte | P2 Stun bar |

### Combo and Damage Addresses

| Address | Size | Description |
|---------|------|-------------|
| `0x020696C5` | byte | P1 combo counter |
| `0x0206961D` | byte | P2 combo counter |
| `0x020691A7` | byte | P1 damage of next hit |
| `0x02069437` | byte | P1 stun of next hit |
| `0x02068D0F` | byte | P2 damage of next hit |
| `0x02068F9F` | byte | P2 stun of next hit |

### Game State Addresses

| Address | Size | Description |
|---------|------|-------------|
| `0x02007F00` | dword | Frame number |
| `0x020154A7` | byte | Match state (0x02 = round active) |
| `0x020154C6` | byte | P1 character locked (0xFF = locked) |
| `0x020154C8` | byte | P2 character locked (0xFF = locked) |
| `0x0201136F` | byte | Freeze game (0xFF = frozen, 0x00 = normal) |
| `0x02011377` | byte | Round timer |
| `0x020154F5` | byte | Stage select |
| `0x02078D06` | byte | Music volume |

### Character Select Addresses

| Address | Size | Description |
|---------|------|-------------|
| `0x020154CF` | byte | P1 character select row (0–6) |
| `0x0201566B` | byte | P1 character select column (0–2) |
| `0x020154D3` | byte | P1 SA select (0–2) |
| `0x02015683` | byte | P1 character color (0–6) |
| `0x0201553D` | byte | P1 character select state |
| `0x020154D1` | byte | P2 character select row |
| `0x0201566D` | byte | P2 character select column |
| `0x020154D5` | byte | P2 SA select |
| `0x02015684` | byte | P2 character color |
| `0x02015545` | byte | P2 character select state |
| `0x020154FB` | byte | Character select timer |

### Parry System Addresses (P1)

| Address | Size | Description |
|---------|------|-------------|
| `0x02026335` | byte | Forward parry validity time |
| `0x02025731` | byte | Forward parry cooldown time |
| `0x02026337` | byte | Down parry validity time |
| `0x0202574D` | byte | Down parry cooldown time |
| `0x02026339` | byte | Air parry validity time |
| `0x02025769` | byte | Air parry cooldown time |
| `0x02026347` | byte | Anti-air parry validity time |
| `0x0202582D` | byte | Anti-air parry cooldown time |

P2 parry addresses follow by adding `+0x406` to the validity addresses and `+0x620` to the cooldown addresses.[^6]

### Charge Move Addresses (P1)

| Address | Description |
|---------|-------------|
| `0x020259D8` | Charge gauge 1 (Urien H, Oro V, Chun V, Q V, Remy V) |
| `0x020259F4` | Charge gauge 2 (Urien V, Q H, Remy H) |
| `0x02025A10` | Charge gauge 3 (Oro H, Remy H2) |
| `0x02025A2C` | Charge gauge 4 (Urien V2, Alex V) |
| `0x02025A48` | Charge gauge 5 (Alex H) |

P2 charge addresses: `0x02025FF8`, `0x02026014`, `0x02026030`, `0x0202604C`, `0x02026068`.[^6]

### Character ID Table

The character ID (read from `base + 0x3C0`) maps to characters as follows:[^6]

| ID | Character | ID | Character |
|----|-----------|-----|-----------|
| 0 | Gill | 11 | Ken |
| 1 | Alex | 12 | Sean |
| 2 | Ryu | 13 | Urien |
| 3 | Yun | 14 | Gouki (Akuma) |
| 4 | Dudley | 15 | Gill (alt) |
| 5 | Necro | 16 | Chun-Li |
| 6 | Hugo | 17 | Makoto |
| 7 | Ibuki | 18 | Q |
| 8 | Elena | 19 | Twelve |
| 9 | Oro | 20 | Remy |
| 10 | Yang | | |

### Hitbox Pointer Offsets (from base)

Used by the CPS3 hitbox viewer script:[^1]

| Offset | Type |
|--------|------|
| `+0x2A0` | Vulnerability boxes (×4) |
| `+0x2A8` | Extended vulnerability boxes (×4) |
| `+0x2B8` | Throw box |
| `+0x2C0` | Throwable box |
| `+0x2C8` | Attack boxes (×4) |
| `+0x2D4` | Push box |

### Screen and Scale Addresses

| Address | Description |
|---------|-------------|
| `0x02026CB0` | Screen X position |
| `0x02026CB4` | Screen Y position |
| `0x0200DCBA` | Scale factor |

## Example Lua Script

Below is a working FBNeo Lua script for SF3 3rd Strike that demonstrates reading common game values and optionally refilling health/meter:

```lua
-- SF3 3rd Strike - FBNeo Lua Script with Memory Access
-- Compatible with sfiii3 / sfiii3nr1 ROMs on FBNeo

-- Player base addresses
local P1_BASE = 0x02068C6C
local P2_BASE = 0x02069104

-- Game state addresses
local FRAME_NUMBER   = 0x02007F00
local MATCH_STATE    = 0x020154A7
local ROUND_TIMER    = 0x02011377
local FREEZE_GAME    = 0x0201136F

-- Gauge addresses
local P1_HEALTH_OFFSET = 0x9F
local P1_GAUGE   = 0x020695B5
local P1_METER   = 0x020695BF
local P1_STUN_BAR = 0x020695FD
local P2_GAUGE   = 0x020695E1
local P2_METER   = 0x020695EB
local P2_STUN_BAR = 0x02069611

-- Character table
local characters = {
 ="Gill", "Alex", "Ryu", "Yun", "Dudley",
  "Necro", "Hugo", "Ibuki", "Elena", "Oro",
  "Yang", "Ken", "Sean", "Urien", "Gouki",
  "Gill", "Chun-Li", "Makoto", "Q", "Twelve", "Remy"
}

-- Settings
local infinite_health = true
local infinite_meter  = true
local infinite_time   = true

-- Read player data
function read_player(base)
  local p = {}
  p.pos_x       = memory.readwordsigned(base + 0x64)
  p.pos_y       = memory.readwordsigned(base + 0x68)
  p.life        = memory.readbyte(base + P1_HEALTH_OFFSET)
  p.char_id     = memory.readword(base + 0x3C0)
  p.char_name   = characters[p.char_id] or "Unknown"
  p.flip_x      = memory.readbytesigned(base + 0x0A)
  p.animation   = memory.readword(base + 0x202)
  p.standing    = memory.readbyte(base + 0x297)
  p.movement    = memory.readbyte(base + 0x0AD)
  p.attacking   = memory.readbyte(base + 0x428)
  p.freeze      = memory.readbyte(base + 0x45)
  return p
end

-- Check if match is active
function is_match_active()
  local p1_locked = memory.readbyte(0x020154C6)
  local p2_locked = memory.readbyte(0x020154C8)
  local state = memory.readbyte(MATCH_STATE)
  return (p1_locked == 0xFF or p2_locked == 0xFF) and state == 0x02
end

-- HUD display
function draw_hud()
  if not is_match_active() then return end

  local p1 = read_player(P1_BASE)
  local p2 = read_player(P2_BASE)

  local frame = memory.readdword(FRAME_NUMBER)

  -- P1 info (left side)
  gui.text(10, 20, string.format("P1: %s", p1.char_name))
  gui.text(10, 30, string.format("HP: %d  Pos: %d,%d", p1.life, p1.pos_x, p1.pos_y))
  gui.text(10, 40, string.format("Gauge: %d  Meter: %d",
    memory.readbyte(P1_GAUGE), memory.readbyte(P1_METER)))
  gui.text(10, 50, string.format("Stun: %d  Anim: %04X",
    memory.readbyte(P1_STUN_BAR), p1.animation))

  -- P2 info (right side)
  gui.text(220, 20, string.format("P2: %s", p2.char_name))
  gui.text(220, 30, string.format("HP: %d  Pos: %d,%d", p2.life, p2.pos_x, p2.pos_y))
  gui.text(220, 40, string.format("Gauge: %d  Meter: %d",
    memory.readbyte(P2_GAUGE), memory.readbyte(P2_METER)))
  gui.text(220, 50, string.format("Stun: %d  Anim: %04X",
    memory.readbyte(P2_STUN_BAR), p2.animation))

  -- Frame counter
  gui.text(150, 10, string.format("F: %d", frame))
end

-- Training mode logic
function training_mode()
  if not is_match_active() then return end

  -- Infinite health
  if infinite_health then
    memory.writebyte(P1_BASE + P1_HEALTH_OFFSET, 160)
    memory.writebyte(P2_BASE + P1_HEALTH_OFFSET, 160)
  end

  -- Infinite meter (fill gauge + count)
  if infinite_meter then
    memory.writebyte(P1_GAUGE, 0x80)
    memory.writebyte(P1_METER, 0x02)
    memory.writebyte(P2_GAUGE, 0x80)
    memory.writebyte(P2_METER, 0x02)
  end

  -- Infinite time
  if infinite_time then
    memory.writebyte(ROUND_TIMER, 100)
  end
end

-- Register callbacks
gui.register(draw_hud)
emu.registerafter(training_mode)

print("SF3 3rd Strike training script loaded.")
print("Infinite health: " .. tostring(infinite_health))
print("Infinite meter: " .. tostring(infinite_meter))
print("Infinite time: " .. tostring(infinite_time))
```

## How to Run

1. Launch FBNeo and load the `sfiii3` or `sfiii3nr1` ROM.[^7][^2]
2. Go to **Game → Lua Scripting → New Lua Script Window**.
3. Browse to the `.lua` script file and click **Run**.[^2]
4. For Fightcade's FBNeo, the command line also works: `fcadefbneo.exe sfiii3nr1 path/to/script.lua`.[^8][^7]

## Existing Community Scripts

| Script | Author | Features |
|--------|--------|----------|
| [3rd_training_lua](https://github.com/Grouflon/3rd_training_lua) | Grouflon | Full training mode: blocking, parry training, recording/replay, hitbox display, frame advantage, counter-attack, input display[^2] |
| [fbneo-training-mode](https://github.com/peon2/fbneo-training-mode) | peon2 | Multi-game training mode supporting 60+ fighting games including sfiii3, with hitbox overlays, health/meter refill, recording[^3][^4] |
| [cps3-hitboxes.lua](https://github.com/Jesuszilla/mame-rr-scripts) | dammit / Jesuszilla | Hitbox viewer for all CPS3 games (sfiii, sfiii2, sfiii3, redearth) with vulnerability, attack, push, throw, and throwable boxes[^1] |

## Movement Type Reference

The `movement_type2` byte at offset `+0x0AF` encodes the player's basic movement state:[^6]

- `0x00` — Standing neutral
- `0x20` — Crouching
- `0x90` (high nibble `0x9x`) — Throwing

The `standing_state` byte at offset `+0x297` provides ground/air status:

- `0x01` — Standing
- `0x02` — Crouching
- `0x00` — Airborne (used in conjunction with other flags for air recovery detection)

## Important Notes

- All addresses are for the **sfiii3 Japan 990512** revision. Other revisions may differ slightly.[^2]
- FBNeo's CPS3 emulation maps program RAM to `0x02000000`–`0x0207FFFF`. ROM/sprite data at `0x04xxxxxx` and above is not accessible from FBNeo's Lua memory functions.[^1]
- Writing to memory during online play (Fightcade) will desync the game. Training scripts disable writes during replays.[^3]
- The `memory.writeword_audio()` function is exclusive to Fightcade's FBNeo fork for sound CPU access.[^9][^3]

---

## References

1. [cps3-hitboxes.lua - Jesuszilla/mame-rr-scripts - GitHub](https://github.com/Jesuszilla/mame-rr-scripts/blob/master/cps3-hitboxes.lua) - -- This function reads all game objects other than the two player characters. -- This includes all p...

2. [Grouflon/3rd_training_lua: Training mode for Street Fighter ... - GitHub](https://github.com/Grouflon/3rd_training_lua) - Go to Game->Lua Scripting->New Lua Script Window and run the script 3rd_training.lua from here ... s...

3. [peon2/fbneo-training-mode · GitHub](https://github.com/peon2/fbneo-training-mode/blob/master/fbneo-training-mode.lua) - a simple training mode for multiple games on the fbneo platform - fbneo-training-mode/fbneo-training...

4. [peon2/fbneo-training-mode - GitHub](https://github.com/peon2/fbneo-training-mode) - A simple training mode for multiple games on the fbneo platform. Written to allow easy incorporation...

5. [Lua Functions List - FCEUX](https://fceux.com/web/help/LuaFunctionsList.html) - Lua Functions. The following functions are available in FCEUX, in addition to standard LUA capabilit...

6. [3rd_training_lua/3rd_training.lua at master](https://github.com/Grouflon/3rd_training_lua/blob/master/3rd_training.lua) - Training mode for Street Fighter III 3rd Strike (Japan 990512), on Fightcade - 3rd_training_lua/3rd_...

7. [3rd Strike & an improved Training Mode in Fightcade on Windows 10](https://www.youtube.com/watch?v=4kbNFH0SFqc) - navigate to the fbneo folder (C:\Users\[name]\Documents\Fightcade\emulator\fbneo); extract the 3rd_t...

8. [3s Training Mode Shortcut - sick of all the clicking to get to ... - Reddit](https://www.reddit.com/r/fightcade/comments/1105bac/3s_training_mode_shortcut_sick_of_all_the/) - Find your fightcade/fbneo folder on your computer and note down the file paths of fbneo, 3s rom and ...

9. [Fightcade Updates: July 2024 | PDF | Bios | Computing - Scribd](https://www.scribd.com/document/750022835/ChangeLog) - - Refactor commandline, now you can load lua + savestate (fcadefbneo.exe savestate.fs script.lua) or...

