# 3SX Configuration Reference

All config files live in the **user preferences directory**:

| Mode | Path |
|------|------|
| Standard (default) | `%APPDATA%/CrowdedStreet/3SX/` (Windows) |
| Portable | `<exe_dir>/config/` (create folder to enable) |

Detection is automatic — if `config/` exists next to the executable, portable mode activates.

---

## File Inventory

| File | Format | Owner | Description |
|------|--------|-------|-------------|
| `config` | `key = value` | `config.c` | Port/app settings (window, shaders, training, netplay, mods) |
| `options.ini` | INI `[Section]` | `native_save.c` | Game state (difficulty, pad layout, rankings) — replaces PS2 memory card |
| `direction.ini` | `key=values` | `native_save.c` | Per-character direction dipswitches (10 pages × 7 chars) |
| `imgui.ini` | ImGui native | Dear ImGui | Window layout/positions — auto-managed. Delete to reset layouts. |
| `replays/` | Binary (`.bin` + `.meta`) | `native_save.c` | Replay data — slots 0-9 manual, 10-19 auto-save ring buffer |

> **Deleted orphans**: `keymap` and `mappings.ini` were legacy files with no source code references.

---

## `config` — App / Port Settings

Source: [config.c](file:///d:/3sxtra/src/port/config/config.c) · [config.h](file:///d:/3sxtra/src/port/config/config.h)

Parser: flat `key = value`, `#` comments, auto-typed as bool/int/string.  
Delete the file to regenerate with defaults.

### Window

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `fullscreen` | bool | `true` | Start in fullscreen mode |
| `fullscreen-width` | int | `0` | Fullscreen width (`0` = desktop resolution) |
| `fullscreen-height` | int | `0` | Fullscreen height (`0` = desktop resolution) |
| `window-width` | int | `640` | Windowed mode width (pixels) |
| `window-height` | int | `480` | Windowed mode height (pixels) |
| `window-x` | int | — | Window X position (saved at exit) |
| `window-y` | int | — | Window Y position (saved at exit) |

### Rendering

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `scale-mode` | string | `nearest` | Upscale filter: `nearest` or `soft-linear` |
| `draw-rect-borders` | bool | `false` | Draw debug borders around rects |
| `dump-textures` | bool | `false` | Dump textures to disk (debug) |
| `shader-mode-libretro` | bool | `false` | Use libretro `.slangp` shader pipeline |
| `shader-path` | string | `""` | Path to `.slangp` shader preset (relative to `shaders/`) |
| `bezel-enabled` | bool | `false` | Show CRT bezel overlay |

### Broadcast

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `broadcast-enabled` | bool | `false` | Enable broadcast/spectator mode |
| `broadcast-source` | int | `0` | `0`=Game, `1`=Window |
| `broadcast-show-ui` | bool | `false` | Show UI overlay in broadcast |

### Training Mode

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `training-hitboxes` | bool | `true` | Show collision boxes |
| `training-pushboxes` | bool | `true` | Show push boxes |
| `training-hurtboxes` | bool | `true` | Show hurt boxes |
| `training-attackboxes` | bool | `true` | Show attack boxes |
| `training-throwboxes` | bool | `true` | Show throw boxes |
| `training-advantage` | bool | `false` | Show frame advantage |
| `training-stun` | bool | `true` | Show stun meter |
| `training-inputs` | bool | `true` | Show input display |
| `training-frame-meter` | bool | `true` | Show frame meter |

### Training Dummy

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `dummy-block` | int | — | `0`=None, `1`=After First, `2`=All |
| `dummy-parry` | int | — | `0`=None, `1`=After First, `2`=All, `3`=Random |
| `dummy-stun-mash` | int | — | `0`=None, `1`=Buttons, `2`=Directions |
| `dummy-wakeup-mash` | int | — | `0`=None, `1`=Buttons, `2`=Directions |
| `dummy-wakeup-reversal` | bool | — | Perform reversal on wakeup |
| `dummy-guard-low` | bool | — | Guard low attacks |
| `dummy-tech-throw` | int | — | `0`=None, `1`=Tech |
| `dummy-fast-wakeup` | int | — | `0`=Normal, `1`=Fast |
| `dummy-block-direction` | int | — | `0`=Standing, `1`=Crouching, `2`=Random |
| `dummy-playback-mode` | int | — | `0`=Off, `1`=Loop |
| `dummy-auto-reversal` | bool | — | Perform auto-reversal |

### Netplay / Lobby

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `netplay-auto-connect` | bool | `true` | Auto-connect to lobby |
| `lobby-server-url` | string | — | Custom matchmaking server URL |
| `lobby-server-key` | string | — | HMAC key for custom server |
| `identity-public-key` | string | auto | Ed25519 public key (auto-generated) |
| `identity-secret-key` | string | auto | Ed25519 secret key (auto-generated) |
| `lobby-client-id` | string | auto | 32-char hex unique client ID |
| `lobby-display-name` | string | auto | Display name in lobby (`Player-XXXX`) |
| `lobby-auto-connect` | bool | `true` | Auto-connect to lobby on startup |
| `lobby-auto-search` | bool | `true` | Auto-search for matches |
| `lobby-region` | string | — | Regional filter for matchmaking |
| `netplay-region-lock` | bool | — | Only match within region |
| `netplay-max-ping` | int | — | Max ping (ms), `0`=any |
| `netplay-block-wifi` | bool | — | Block WiFi opponents |
| `netplay-ft` | int | `2` | First-to-N wins per set (1–10) |
| `netplay-invite-cooldown` | int | — | Cooldown between invites (s) |

### Display / Performance

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `vsync` | bool | — | Enable vertical sync |
| `debug-hud` | bool | — | Show debug HUD overlay |
| `skip-intro` | bool | `false` | Skip intro sequence on boot |
| `ui-mode` | string | — | UI rendering mode |
| `hd-stages` | bool | — | Use HD stage backgrounds |

### Mods

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `modded-bgm-enabled` | bool | `false` | Enable custom BGM from `assets/bgm_mod/` |
| `modded-voice-enabled` | bool | `false` | Enable custom voice packs |

---

## `options.ini` — Game Save State

Source: [native_save.c](file:///d:/3sxtra/src/port/save/native_save.c)

This file stores game options that mirror the original PS2 memory card save data. Auto-generated by the game; hand-editable.

### [Controller]

| Key | Type | Description |
|-----|------|-------------|
| `pad_Np_buttons` | 8 ints | Button mapping: LP, MP, HP, unused, LK, MK, HK, unused (0–11 = button index) |
| `pad_Np_vibration` | int | `0`=off, `1`=on |

### [Game]

| Key | Range | Description |
|-----|-------|-------------|
| `difficulty` | 1–8 | AI difficulty (1=easiest, 8=hardest) |
| `time_limit` | -1–99 | Round timer in seconds (`-1`=infinite) |
| `battle_number_N` | 1–9 | Rounds per match for player N |
| `damage_level` | 0–3 | Damage multiplier (0=lowest, 3=highest) |
| `handicap` | 0–1 | `0`=off, `1`=on |
| `partner_type_Np` | 0–1 | `0`=Normal, `1`=CPU |

### [Display]

| Key | Description |
|-----|-------------|
| `adjust_x`, `adjust_y` | Screen offset in pixels |
| `screen_size` | Packed display size (internal) |
| `screen_mode` | `0`=Normal, `1`=Flipped |

### [Gameplay]

| Key | Range | Description |
|-----|-------|-------------|
| `guard_check` | 0–1 | Guard indicator (`0`=off, `1`=on) |
| `auto_save` | 0–1 | `0`=off, `1`=on |
| `analog_stick` | 0–1 | `0`=off, `1`=on |
| `unlock_all` | 0–1 | `0`=locked, `1`=all characters unlocked |

### [Sound]

| Key | Range | Description |
|-----|-------|-------------|
| `bgm_type` | 0–1 | `0`=Original, `1`=Arranged |
| `sound_mode` | 0–1 | `0`=Stereo, `1`=Mono |
| `bgm_level` | 0–15 | BGM volume |
| `se_level` | 0–15 | Sound effects volume |

### [Extra]

| Key | Description |
|-----|-------------|
| `extra_option` | `0`=off, `1`=on (enables extra option pages) |
| `pl_color_Np` | 20 ints — per-character color index (`0`=default) |
| `extra_option_page_N` | 8 ints — extra options per page (internal dipswitch values) |

### [Broadcast]

| Key | Description |
|-----|-------------|
| `broadcast_enabled` | `0`=off, `1`=on |
| `broadcast_source` | `0`=Game, `1`=Window |
| `broadcast_show_ui` | `0`=hide overlay, `1`=show overlay |

### [Rankings]

Binary ranking data stored as comma-separated bytes. **Do not edit manually.**

---

## `direction.ini` — Character Direction Config

Source: [native_save.c](file:///d:/3sxtra/src/port/save/native_save.c)

10 pages × 7 values each. Each value is a per-character direction dipswitch:
- `0` = default
- `1` = right
- `2` = left

---

## `imgui.ini` — ImGui Window Layout

Managed entirely by Dear ImGui. Safe to delete to reset all window positions/sizes. Not generated by 3SX code.

---

## Architecture Note: `config` vs `options.ini`

These two files serve different purposes and use different subsystems:

| | `config` | `options.ini` |
|---|----------|---------------|
| **Scope** | Port/app layer | Game engine state |
| **Source** | `config.c` | `native_save.c` |
| **Parser** | Custom typed (bool/int/string) | Simple INI (all ints) |
| **When saved** | On setting change | On game exit / save trigger |
| **Replaces** | N/A (new for port) | PS2 memory card save |

> **Note**: Broadcast settings appear in both files. `config` is authoritative for the port layer; `options.ini` stores the game-side copy.
