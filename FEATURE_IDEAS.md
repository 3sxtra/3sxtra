# 3SXtra — Feature Ideas & Roadmap

> Living document of planned features, brainstorms, and design notes.
> Captured from team discussions (March 2026) and expanded with implementation context.

---

## Implementation Status & Gap Analysis

> [!NOTE]
> Audit performed March 23, 2026. For each feature area: **what exists in code today**, **what the vision calls for**, and **what remains to be built** — with specific file and function references.

---

### §1. Mod Menu & FX Screens

#### What Exists

| System | Details |
|---|---|
| **Mods menu** | [rmlui_mods_menu.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_mods_menu.cpp) (~341 lines), toggled via F3 in [sdl_app_input.c](file:///d:/3sxtra/src/port/sdl/app/sdl_app_input.c). Toggles: shader bypass, fast pre-game, HD stages, bezel, **modded BGM/voice** (with track count). |
| **Shader menu** | [rmlui_shader_menu.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_shader_menu.cpp) (19KB) |
| **Stage config** | [rmlui_stage_config.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_stage_config.cpp) (10KB) |
| **Phase 3 toggles** | [rmlui_phase3_toggles.h](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_phase3_toggles.h) — per-component runtime fallback to CPS3 rendering |
| **Audio mods** | [modded_bgm.c](file:///d:/3sxtra/src/port/sound/modded_bgm.c) — BGM + voice replacement from `assets/voice_mod/`, supports ogg/flac/opus/mp3/wav. Toggle via `CFG_KEY_MODDED_BGM_ENABLED`. |
| **Sprite overrides** | [sprite_override.c](file:///d:/3sxtra/src/port/sdl/renderer/sprite_override.c) — HD sprite replacement with 17+ hooks in [mtrans.c](file:///d:/3sxtra/src/sf33rd/Source/Game/rendering/mtrans.c) |
| **HD stage overrides** | [modded_stage.c](file:///d:/3sxtra/src/port/mods/modded_stage.c) — 22-stage system with layer-based replacement |
| **Hot-reload** | [rmlui_wrapper.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_wrapper.cpp) — Ctrl+F5 (stylesheets), Ctrl+Shift+F5 (all documents) |

#### Vision

- **Unified in-game menu tree** — one discoverable entry point from the main/pause menu, not scattered F-key overlays
- Per-character FX overrides (hit-spark and super-flash styles per character)
- Named mod profiles/presets with one-click swap ("Tournament Clean", "Maximum Drip")
- Automatic mod discovery — scan `assets/` at startup
- Live previews showing a thumbnail before committing

#### The Gap

| Gap | Notes |
|---|---|
| **No unified parent menu** | The overlays work but are islands — discoverable only if you know the hotkeys. Needs new entries in [ms_mode_select.c](file:///d:/3sxtra/src/port/screens/ms_mode_select.c) / [ms_option_select.c](file:///d:/3sxtra/src/port/screens/ms_option_select.c). |
| **No mod profiles** | Persistence layer needed — extend [native_save.c](file:///d:/3sxtra/src/port/save/native_save.c) |
| **No mod discovery** | Nothing scans `assets/` or builds a registry of installed packs |
| **No per-character FX** | New configuration needed in the render pipeline |
| **No live previews** | Significant UI investment |

---

### §2. Replay Autosaving & In-Match Chat

#### What Exists

| System | Details |
|---|---|
| **Replay recording/playback** | [sys_replay.c](file:///d:/3sxtra/src/sf33rd/Source/Game/system/sys_replay.c) — CPS3-native format |
| **Replay picker (local)** | [rmlui_replay_picker.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_replay_picker.cpp) — string-based UI with metadata display |
| **Replay picker (online)** | [rmlui_network_replay_picker.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_network_replay_picker.cpp) — browse/download replays from lobby server. Accessed via Network Gateway → REPLAYS. Async fetch + download threads, injects into `Replay_w` for playback. Server endpoints: `GET /replays` (paginated list), `GET /replays/:id` (binary download). |
| **Save/load flow** | [ms_save_replay.c](file:///d:/3sxtra/src/port/screens/ms_save_replay.c) — full enter/tick/exit lifecycle |
| **Auto-save (netplay)** | `NativeSave_AutoSaveReplay(1)` called in [sdl_netplay_ui.cpp](file:///d:/3sxtra/src/port/sdl/netplay/sdl_netplay_ui.cpp) when match ends. Generates descriptive string filename. |
| **Server upload** | `AsyncReportMatch()` snapshots `Replay_w` and uploads via `LobbyServer_UploadReplay()` on a background thread. Server stores as `replays/replay_{matchId}.bin`, sets `has_replay=1` in matches table. |
| **Lobby chat** | [rmlui_casual_lobby.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_casual_lobby.cpp) — 50-message buffer, real-time via `SSE_EVENT_CHAT`, `LobbyServer_SendChat()` |

#### Vision & Gap

| Feature | Status | What's Needed |
|---|---|---|
| Auto-save every match | ✅ Works for netplay | — |
| Online replay browsing | ✅ Implemented | Network Gateway → REPLAYS. Paginated list, async download, inject into `Replay_w`. |
| Descriptive filenames | ✅ Implemented | String-based filenames with timestamp, mode, chars, winner |
| Metadata auto-tagging | ✅ Implemented | `NativeReplayHeader` v2 + sidecar `.meta` files |
| Retention policy | ❌ | Auto-save slots just rotate/overwrite |
| Highlight bookmarks | ❌ | New input handler during gameplay recording frame numbers |
| In-match quick-chat | ❌ | Entirely new UI — lobby chat is hidden during matches |
| Quick-chat wheel | ❌ | D-pad emote selection, new component |
| Between-rounds chat | ❌ | Hook into round transition in `game.c` |

---

### §3. Tournaments

#### What Exists

> **Tournaments are fully implemented** (March 24, 2026). All bracket formats, TO controls, bracket UI, and server API extensions are operational. The system extends the existing room infrastructure.

#### Architecture Decision: Tournament = Extended Room

A tournament **is a room** with additional bracket state. Players join a tournament the same way they join a casual room (same `ListRooms`, `JoinRoom`, SSE subscription). The key difference:

| Casual Room | Tournament Room |
|---|---|
| 1 match slot (`match_p1`/`match_p2`) | **N concurrent match slots** (`matches[MAX_CONCURRENT_MATCHES]`) |
| FIFO queue → next in line plays | **Bracket-driven pairing** → server determines who plays whom |
| Winner stays on | **Winner advances** in bracket |
| No rounds | **Round progression** (bracket updates via SSE) |
| Host = host | **Host = Tournament Organizer (TO)** with override powers |

**Multi-match slots (parallel bracket play):** all independent bracket matches in a round fire simultaneously. Each pair gets their own P2P GekkoNet session. Non-playing participants choose which match to spectate via a **match selector**.

```c
typedef struct {
    char p1[64];
    char p2[64];
    int active;
    int bracket_round;
    int bracket_position;  // match index within this round
} RoomMatch;

#define MAX_CONCURRENT_MATCHES 8

// Extended RoomState for tournament rooms
typedef struct {
    // ... existing RoomState fields (players, chat, queue) ...
    int room_type;           // ROOM_CASUAL, ROOM_KOTH, ROOM_TOURNAMENT
    int tournament_format;   // SINGLE_ELIM, DOUBLE_ELIM, ROUND_ROBIN, SWISS
    int tournament_round;
    int tournament_total_rounds;
    RoomMatch matches[MAX_CONCURRENT_MATCHES];
    int match_count;
    BracketEntry bracket[MAX_BRACKET_SIZE]; // full bracket tree
    int bracket_size;
} TournamentRoomState;
```

**SSE event extensions:**
- Existing `MATCH_PROPOSE`, `MATCH_START`, `MATCH_END` gain a `match_index` field so each player knows which proposal is theirs
- New `SSE_EVENT_BRACKET_UPDATE` — server pushes updated bracket tree after each match result
- New `SSE_EVENT_ROUND_ADVANCE` — signals all matches in current round are complete

**Bracket generation strategy:**
- **Single/Double Elim:** full bracket generated at registration close (all matchups predetermined)
- **Swiss/Round-Robin:** server generates only the next round's pairings after current round completes

#### Implementation Status

| Component | Status | Notes |
|---|---|---|
| Room infrastructure (join, SSE, chat, P2P) | ✅ Reused | Direct reuse of `RoomState`, `LobbyServer_*`, casual lobby SSE |
| Match lifecycle (propose/accept/start/end) | ✅ Reused | `match_index` added for parallel matches |
| Match reporting + cross-validation | ✅ Reused | Server auto-advances bracket on `MATCH_SESSION_COMPLETE` |
| STUN/UPnP P2P connections | ✅ Reused | Each bracket match pair punches independently |
| Spectating + match selector | ✅ Done | D-pad ◄ ► scrolls `s_match_selector_idx` in [rmlui_tournament_lobby.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_tournament_lobby.cpp) |
| `room_type` field + 🏆 badge | ✅ Done | `is_tournament` binding in [network_lobby.rml](file:///d:/3sxtra/assets/ui/network_lobby.rml) |
| `CREATE ROOM` tournament toggle | ✅ Done | TYPE selector → format, max players, seeding fields in network lobby |
| Multi-match slot support | ✅ Done | `RoomMatch matches[MAX_CONCURRENT_MATCHES]` in [lobby_server.h](file:///d:/3sxtra/src/netplay/lobby_server.h) |
| Bracket logic (all 4 formats) | ✅ Done | [bracket.c](file:///d:/3sxtra/src/netplay/bracket.c): SE, DE (with cross-bracket routing), RR (circle algorithm), Swiss (Monrad pairing). 28 unit tests in [test_bracket.c](file:///d:/3sxtra/tests/unit/test_bracket.c) |
| Tournament registration + seeding | ✅ Done | Rating (Glicko-2), join order, random — server + client |
| TO controls (DQ, override, pause) | ✅ Done | DQ player selector + Override button in [tournament_lobby.rml](file:///d:/3sxtra/assets/ui/tournament_lobby.rml). API: `BracketDQ`, `BracketOverride`, `BracketPause` |
| Tournament UI (bracket display) | ✅ Done | [rmlui_tournament_lobby.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_tournament_lobby.cpp) — bracket panel, TO controls, dynamic cursor |
| Server API extensions | ✅ Done | All 5 endpoints in [lobby-server.js](file:///d:/3sxtra/tools/lobby-server/lobby-server.js): `/bracket/start`, `/bracket`, `/bracket/override`, `/bracket/dq`, `/bracket/pause`. Integration tests in [__test_tournament.js](file:///d:/3sxtra/tools/lobby-server/__test_tournament.js) (11 scenarios) |
| Restart match (re-fire disputed) | ✅ Done | TO re-fires a match proposal for a disputed game |

> [!TIP]
> Tournament system is **feature-complete** and 100% finished (March 24, 2026). All TO controls, bracket logic, UI, and parallel match flows are operational.

---

### §4. Dynamic Bezel During Netplay

#### What Exists

| System | Details |
|---|---|
| **Bezel loader/renderer** | [sdl_app_bezel.c](file:///d:/3sxtra/src/port/sdl/app/sdl_app_bezel.c), [sdl_bezel.c](file:///d:/3sxtra/src/port/rendering/sdl_bezel.c) — 40+ per-character arcade bezels, auto-swap on character change |
| **Netplay stats overlay** | [netstats_renderer.c](file:///d:/3sxtra/src/port/sdl/netstats_renderer.c), [rmlui_netplay_ui.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_netplay_ui.cpp) — ping, rollback, quality (separate layer) |

#### Vision & Gap

| Feature | Status | Notes |
|---|---|---|
| Opponent-aware (P1 left / P2 right) | ❌ | Current system renders one full-frame image — no left/right panel concept. Requires composite at runtime from half-bezel assets, or 400 pre-rendered pairs (impractical). |
| Stats embedded in bezel art | ❌ | Stats and bezels are currently separate render layers |
| Win/loss streak in bezel | ❌ | No session record display |
| Animated bezels | ❌ | Renderer handles static textures only |
| Regional/seasonal bezels | ❌ | Additional artwork + selection mechanism |
| Spectator broadcast frame | ❌ | New layout |

---

### §5. King of the Hill

#### What Exists

| System | Details |
|---|---|
| **Winner-stays-on rotation** | `LobbyServer_ReportMatchEnd()` auto-rotates the queue. `SSE_EVENT_MATCH_END` carries `match_winner_id`/`match_loser_id`. |
| **Queue management** | `LobbyServer_JoinQueue/LeaveQueue`, `queue[MAX_ROOM_PLAYERS]` in `RoomState` |
| **Win streak display** | [win.c](file:///d:/3sxtra/src/sf33rd/Source/Game/screen/win.c) + [rmlui_win_screen.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_win_screen.cpp) — binds `WGJ_Win` (consecutive wins), shows "1st WIN", "2nd WIN+" |
| **Spectating** | [netplay.c](file:///d:/3sxtra/src/netplay/netplay.c) — up to 4 spectators, 15-frame delay |

> The **mechanics** work. The **KOTH experience** doesn't exist yet.

#### The Gap

| Gap | Notes |
|---|---|
| **No KOTH room type** | All rooms look identical in the lobby browser — no "KOTH" label in `RoomListItem` |
| **No session stats** | `WGJ_Win` exists but no longest-streak, total defenses, character usage, avg duration, parry/super counts |
| **No KOTH lobby UI** | Queue is shown but not in a KOTH-themed way |
| **No dethroned animation** | "UPSET!" screen when a long streak is broken |
| **No CPU fill** | When queue is empty, King should play CPU — conceptually simple, untested in match flow |

**Implementation steps:** (1) add `room_type` to the room API, (2) build KOTH-flavored casual lobby variant, (3) wire server-side stats aggregation, (4) add dethroned/upset RmlUi overlay.

---

### §6. Private / Hidden Rooms

#### What Exists

| System | Details |
|---|---|
| **Room creation** | `LobbyServer_CreateRoom(name, ft, out_room)` — server-assigned 4-char code |
| **Room listing/joining** | `LobbyServer_ListRooms()`, `LobbyServer_JoinRoom()` |
| **Invite cooldown** | `LobbyServer_DeclineInvite()` + `add_declined_player()` — 30s local cooldown, configurable via `CFG_KEY_NETPLAY_INVITE_COOLDOWN` |

> Room codes act as *soft* private access — shareable, but rooms are still publicly visible.

#### Vision & Gap

| Feature | Status | Notes |
|---|---|---|
| Password-protected rooms | ✅ | Password dialog + `password` param in `CreateRoom`/`JoinRoom` |
| Hidden rooms (not in `ListRooms`) | ✅ | `visibility` field implemented (Public/Private) |
| Human-readable codes (`HADOKEN-42`) | ❌ | Server-side cosmetic change |
| Per-room allowlist/blocklist | ❌ | Server storage + client enforcement |
| Persistent rooms (host grace period) | ❌ | Host-migration timer |
| Password dialog in lobby UI | ✅ | Arcade-style password entry in `rmlui_network_lobby.cpp` |
| Lobby list filters (Public/Private/My) | ❌ | New filter UI |
| QR Code Sharing | ❌ | Display a QR code for easy joining of hidden/password/tournament rooms |

> All of this is **API extensions + UI changes** — no fundamental architectural work.

---

### §7. Match Flow / Game Flow & Blind Picks

#### What Exists

| System | Details |
|---|---|
| **Character select** | [rmlui_char_select.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_char_select.cpp) (485 lines) — HD portraits for all 21 chars, SA display, stage select with flags, BCD countdown timer, player visibility flags |
| **VS screen** | [rmlui_vs_screen.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_vs_screen.cpp) |
| **Match proposals** | `SSE_EVENT_MATCH_PROPOSE` — delivers P1/P2 name, conn type, RTT, region, room code. Accept/decline + timeout handling. |
| **Game HUD** | [rmlui_game_hud.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_game_hud.cpp) (549 lines, 52 bindings) — health bars, round timer, stun/SA gauges, combo/parry counters, result bubbles |

#### Vision & Gap

| Feature | Status | Notes |
|---|---|---|
| Blind picks | ❌ | Requires network sync layer — both players currently see real-time char select. Need: hide opponent cursor until lock-in, lock-in sync messages, reveal animation. Hooks exist: `Sel_PL_Complete[0/1]`, `My_char[0/1]`. |
| Stage bans | ❌ | New pre-match negotiation step before VS screen |
| Match flow state machine | ❌ | State machine layered on `menu_task_phases.h` |
| Ranked/Tournament game flow modes | ❌ | New server-coordinated flow |

---

### §8. Fight Requests — Matchmaking & Ranked

#### What Exists

| System | Details |
|---|---|
| **Match reporting** | `LobbyServer_ReportMatch()` — cross-validated, 4 statuses (pending/in_progress/complete/dispute) |
| **Player stats** | `LobbyServer_GetPlayerStats()` — Glicko-2 `rating`, `rd`, `tier`, `wins`, `losses`, `disconnects` |
| **Disconnect handling** | `LobbyServer_ReportDisconnect()` — 30s server-side timeout for ragequit detection |
| **Leaderboards** | `LobbyServer_GetLeaderboard()` (paginated) + [rmlui_leaderboard.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_leaderboard.cpp) — rank, rating, tier, grade, most-played char, country, disconnects |
| **Connection filtering** | `player_passes_filters()` in [sdl_netplay_ui.cpp](file:///d:/3sxtra/src/port/sdl/netplay/sdl_netplay_ui.cpp#L276-L301) — region lock, max ping (P2P RTT via `PingProbe_GetRTT()`), Wi-Fi block |
| **Searching** | `LobbyServer_StartSearching/StopSearching/GetSearching` |
| **Net detection** | [net_detect.c](file:///d:/3sxtra/src/netplay/net_detect.c) — Wi-Fi/wired; [ping_probe.c](file:///d:/3sxtra/src/netplay/ping_probe.c) — P2P RTT |
| **Anti-spam** | `add_declined_player()` — 30s cooldown; `LobbyServer_DeclineInvite()` for server-side rate limiting |

#### The Gap

Every item below is **client-side gameplay or matchmaking algorithm** — the server plumbing is ready:

| Feature | Notes |
|---|---|
| **Character Lock/Ban** | Client-side state + UI. Char select needs to read lock/ban config, hide banned chars, enforce lock between matches. Ranked point modifier needs multiplier field in match report. |
| **Ranked matchmaking queue** | Currently manual browse + connect. Needs `POST /matchmaking/enqueue` + SSE event, "Searching..." client UI. |
| **Hidden names pre-match** | Simple — omit `display_name` from match proposal until post-accept. |
| **Win streak bonuses** | `WGJ_Win` exists. No streak-aware ranked point modifier, no priority matching logic. |
| **Graceful quit** | New in-game protocol — hold-Start detection, reason cycling, mutual confirmation. Needs game loop + netplay protocol hooks. |
| **Jail system** | `disconnects` already tracked. Needs: `jailed` flag, auto-jail threshold logic, jail search filter, rehabilitation counter. |
| **Skill range matching** | Needs server-side matchmaking (not just filtering) with rating bands. |

---

### §9. Attract Mode & Information Bar

#### What Exists

| System | Details |
|---|---|
| **Attract mode** | Original arcade code — [demo01.c](file:///d:/3sxtra/src/sf33rd/Source/Game/demo/demo01.c) (title sequences), [demo02.c](file:///d:/3sxtra/src/sf33rd/Source/Game/demo/demo02.c) (demo gameplay), [demo_dat.c](file:///d:/3sxtra/src/sf33rd/Source/Game/demo/demo_dat.c) (input-replay data), [demo_states.h](file:///d:/3sxtra/src/sf33rd/Source/Game/demo/demo_states.h) |
| **HD attract overlay** | [rmlui_attract_overlay.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_attract_overlay.cpp) — HD logo + "PRESS START BUTTON" during Loop_Demo cases 3/5 |
| **High-score tables** | [sys_ranking.c](file:///d:/3sxtra/src/sf33rd/Source/Game/system/sys_ranking.c) |

#### Vision & Gap

| Feature | Status | Notes |
|---|---|---|
| Online replay attract | ❌ | Replay *upload* works; needs new `GET /replay/random` endpoint + client download + inject into replay playback + suppress "PRESS START" overlay |
| Live server stats display | ❌ | Data fetchable via `LobbyServer_GetLeaderboard()` — just needs rendering during attract, not only in network menu |
| Live win streak leaderboard | ❌ | Same — data exists, no attract-mode rendering |
| Offline fallback | ✅ | Already works — CPU demos always play when offline |
| Information bar | ❌ | New standalone RmlUi component. Challenge: spans game + menu contexts; menu system has no human-readable help text for items. |

---

### §10. Additional Ideas

#### What Exists

| System | Details |
|---|---|
| **Spectator mode** | [netplay.c](file:///d:/3sxtra/src/netplay/netplay.c) — GekkoNet spectate session, 15-frame delay, up to 4 spectators, connected/paused/unpaused events. Spectate button in `rmlui_casual_lobby.cpp` gated on `can_spectate`. |
| **Input display** | [rmlui_input_display.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_input_display.cpp) (335 lines) — per-frame input history, mods menu toggle |
| **Frame data display** | [rmlui_frame_display.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_frame_display.cpp) (342 lines) — frame meter (startup/active/recovery/hitstun/blockstun/down), startup F / total F / advantage, training mode only |
| **Player stats** | `LobbyServer_GetPlayerStats()` — wins/losses/disconnects/rating/rd/tier. `LeaderboardEntry` — rank, most-played char, country, grade |
| **Lobby indicators** | [rmlui_network_lobby.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_network_lobby.cpp) — per-player `ping_label` (~42ms), `ping_class` (green/yellow/red), `conn_type` (wifi/wired), country flags ✅ |
| **Player identity** | [identity.c](file:///d:/3sxtra/src/netplay/identity.c) — SHA256-based ID generation |

#### The Gap

**Spectator:**

| Feature | Status | Notes |
|---|---|---|
| Spectator count shown to players | ❌ | Data in `RoomState` server-side, not surfaced |
| Rewind without affecting live stream | ❌ | Requires circular buffer of game states on spectator side |
| Input display for spectators | ❌ | |
| Commentary mode | ❌ | Stretch goal |

**Rating:**

| Feature | Status | Notes |
|---|---|---|
| Global Glicko-2 rating + tiers | ✅ | Fully operational |
| Global leaderboards | ✅ | Paginated, full RmlUi display |
| Character-specific rating | ❌ | Needs `{player_id, character}` composite key in server DB |
| Seasonal resets + placement matches | ❌ | Server config changes |

**Social:**

| Feature | Status |
|---|---|
| Player profiles (avatar, bio, stats) | ❌ |
| Friends list | ❌ |
| Clan / team tags | ❌ |
| Match history (searchable log) | ❌ |

**Quality of Life:**

| Feature | Status | Notes |
|---|---|---|
| Lobby connection indicators | ✅ | ping_class, conn_type, country flags in network lobby |
| Auto-region detection | ❌ | Region manually set via `CFG_KEY_LOBBY_REGION` |
| Notification sounds | ❌ | |
| Idle timeout | ❌ | |

---

### Summary

| Feature Area | What Exists | What's Missing |
|---|---|---|
| **§1 Mod Menu** | Mods/shader/stage menus, F-key toggles, phase3 per-component, audio/voice mods, sprite overrides, HD stages, hot-reload | Unified tree, profiles, discovery, previews, per-char FX |
| **§2 Replays & Chat** | Auto-save + upload, string-based picker, metadata v2, descriptive filenames, lobby chat | Retention policy, bookmarks, in-match quick chat, emotes |
| **§3 Tournaments** | ✅ **Implemented** — SE/DE/RR/Swiss brackets, TO controls (DQ/override/pause/restart), bracket UI, match selector, server API (6 endpoints), 28 unit tests, 12 integration tests | — |
| **§4 Dynamic Bezel** | 40+ char bezels, auto-swap | Opponent-aware compositing, stats-in-bezel, animation, spectator frame |
| **§5 KOTH** | Rotation + queue + streak count (server + casual lobby) | KOTH room type, session stats, queue viz, dethroned anim, CPU fill |
| **§6 Private Rooms** | Room codes, create/join, **passwords, hidden visibility, UI dialog** | Allow/blocklists, persistence, filters, human-readable codes |
| **§7 Match Flow** | Char select (HD portraits, SA, timer, stage), VS screen, accept/decline proposals | Blind pick sync, stage bans, match flow state machine, ranked/tournament modes |
| **§8 Fight Requests** | Glicko-2, match reporting, disconnect tracking, leaderboards, region/ping/WiFi filtering, searching, anti-spam | Character lock/ban, ranked auto-match, hidden names, streak bonuses, graceful quit, jail, skill range pairing |
| **§9 Attract Mode** | CPU demos, HD overlay, arcade ranking tables | Online replay playback, live stats, live streaks, information bar |
| **§10 Additional** | Spectator (4 max, 15f delay), input display, frame data, player stats/identity, lobby ping/conn indicators ✅ | Spectator count/rewind, per-char rating, seasonal resets, profiles, friends, clans, match history |

> [!IMPORTANT]
> **Key finding:** The **server infrastructure and netplay plumbing are significantly ahead of the client-side UX**. Match reporting with cross-validation, Glicko-2 stats, disconnect tracking, P2P ping probing, Wi-Fi detection, connection quality filtering, replay auto-save + upload, winner-stays-on rotation, spectator support, and the full SSE streaming system are all operational. The remaining work falls into three categories:
>
> 1. **New gameplay protocols** — blind picks, character lock/ban, graceful quit, KOTH mode (netcode + game loop changes)
> 2. **Server-side matchmaking** — ranked queue, skill range pairing, jail system, priority matching
> 3. **Client-side UX** — unified menus, information bar, tournament UI, private room dialogs, KOTH stats (RmlUi screens)

---

## 1. Mod Menu & FX Screens via Game Menus

**Original idea:** Expose mod configuration and visual-effects screens through the in-game menu system.

### What This Means
- Unify the **F3 mods menu** and shader/FX controls into a single, discoverable in-game menu tree accessible from the main menu or pause menu.
- Let players toggle **HD stages**, **sprite overrides**, **bezel packs**, **shader presets**, and **audio mods** without memorizing hotkeys.
- Present live previews: show a thumbnail or looping clip of the active shader/stage/bezel before committing.

### Augmented Scope
| Sub-feature | Details |
|---|---|
| **Per-character FX overrides** | Individual hit-spark and super-flash styles per character (e.g., classic CPS3 sparks vs. HD redrawn). |
| **Mod profiles / presets** | Save and name combinations of visual settings ("Tournament Clean", "Maximum Drip"). One-click swap. |
| **Mod discovery** | Scan `assets/` at startup, auto-populate the menu. Show installed vs. missing mod packs. |
| **Hot-reload** | Apply mod/shader changes without restarting. Already partially supported via F2/F4 and Ctrl+F5 (RmlUi) — extend to all mod types. |

---

## 2. Replay Autosaving & In-Match Chat

### Replay Autosaving

> ✅ Auto-save + upload, v2 metadata headers, and descriptive string filenames (`2026-03-24_15-23-09_net_Ryu-vs-Ken_W-P1.bin`) are implemented.
> 
> The remaining work is retention and game-time marking (bookmarks).

- **Configurable retention policy**: keep last N replays, or last N days, or unlimited.
- **Highlight bookmarks** — press a hotkey mid-match to bookmark a moment; replay viewer jumps to bookmarks.

### In-Match Chat

- **Pre-set quick messages** (stickers/emotes): "GG", "Rematch?", "One more", "BRB", custom messages.
- Quick-chat wheel activated by hotkey — select with D-pad, no typing mid-match.
- **Between-rounds text chat** — small text input during the "Ready" countdown or post-match results screen.
- **Chat history** — scrollable log in the lobby and post-match screen.
- Profanity filter toggle (opt-in).
- Visual indicator: small speech bubble above player name when opponent sends a message.

---

## 3. Tournaments

> [!NOTE]
> ✅ **Implemented March 24, 2026.** A tournament is an extended room. All 4 bracket formats (SE/DE/RR/Swiss), TO controls (DQ/override/pause), bracket UI with match selector, and server API are operational. See §3 gap analysis above for implementation details.

### Bracket Formats
| Format | Details | Bracket Generation |
|---|---|---|
| **Single elimination** | Classic bracket, seeded or random. | Full bracket at registration close |
| **Double elimination** | Winners/losers bracket with grand finals reset. | Full bracket at registration close |
| **Round robin** | League-style, everyone plays everyone. Points-based ranking. | Round-by-round |
| **Swiss** | Pair players with similar records each round. Good for large pools. | Round-by-round |

### Parallel Bracket Matches

All independent matches in a bracket round fire **simultaneously**. In an 8-player single elim Round 1, all four matches run at the same time:

```
Round 1 (all parallel):     Round 2 (all parallel):     Finals:
  Match 0: Seed1 vs Seed8     Match 0: W0 vs W1           Match 0: W0 vs W1
  Match 1: Seed2 vs Seed7     Match 1: W2 vs W3
  Match 2: Seed3 vs Seed6
  Match 3: Seed4 vs Seed5
```

Each pair gets an independent P2P GekkoNet session (STUN punch happens per pair). Non-playing participants use a **match selector** to choose which match to spectate.

### Network Lobby Integration

Tournaments are accessed through the **existing Network Lobby** — no separate menu entry:

- **ROOMS panel** (right side): tournament rooms appear alongside casual rooms with a 🏆 icon/badge and player count (e.g., `🏆 Friday Night FT3  4/16`)
- **CREATE ROOM** (left side): existing option gains a `TYPE: CASUAL / TOURNAMENT` toggle. Selecting tournament reveals additional fields: format (single elim, double elim, Swiss, round robin), max players, seeding method (rating / join order / random)
- **Joining**: clicking a tournament room in the ROOMS list enters a **tournament lobby variant** (bracket display + match selector) instead of the casual lobby

### Tournament Flow
1. **Creation:** TO selects CREATE ROOM → TYPE: TOURNAMENT → sets name, format, max players, FT value, seeding method.
2. **Registration:** Tournament room appears in the ROOMS panel with a 🏆 badge. Players join via room list or room code (same `JoinRoom` flow). Bracket populates as players register.
3. **Bracket close:** TO triggers bracket generation. Seeding: by rating (Glicko-2), by join order, or random.
4. **Round start:** Server fires `MATCH_PROPOSE` for all pairings in the round simultaneously (each with a `match_index`). Players accept/decline as normal.
5. **Parallel play:** All accepted matches run concurrently. `MATCH_END` events carry `match_index` so the server knows which bracket position to advance.
6. **Round advance:** When all matches in a round complete, server sends `SSE_EVENT_ROUND_ADVANCE` and auto-fires the next round's proposals.
7. **Results reporting:** Match results auto-reported via existing `LobbyServer_ReportMatch()` cross-validation. TO can override via privileged API.
8. **Spectating:** Non-playing participants see a match selector and can spectate any active match.

### TO (Tournament Organizer) Controls
| Action | Details |
|---|---|
| **Start bracket** | Lock registration, generate bracket |
| **DQ player** | Remove from bracket, opponent auto-advances |
| **Override result** | Correct misreported match (privileged API) |
| **Pause tournament** | Freeze bracket progression |
| **Restart match** | Re-fire a match proposal for a disputed game |

### Server API Extensions
All endpoints are scoped under the existing room namespace (no separate `/tournament/` namespace):
- `POST /room/:code/bracket/start` — close registration, generate bracket
- `GET /room/:code/bracket` — fetch current bracket state
- `POST /room/:code/bracket/override` — TO result override
- `POST /room/:code/bracket/dq` — DQ a player
- `POST /room/:code/bracket/pause` — pause/resume

---

## 4. Dynamic Bezel During Netplay

> 40+ per-character arcade bezels already auto-swap on character selection. This extends bezels to be **context-aware during netplay**.

### Proposed Enhancements
| Feature | Details |
|---|---|
| **Opponent-aware bezels** | Show P1's character on the left bezel panel, P2's on the right. Both players see a combined bezel. |
| **Netplay stats overlay on bezel** | Display ping, rollback frames, and connection quality as part of the bezel artwork — LED-style indicator in the cabinet art. |
| **Win/loss streak** | Show session record (W-L) integrated into the bezel — like a coin-op win counter. |
| **Animated bezels** | Subtle animations: flickering arcade cabinet lights, scrolling marquee text with player names, glowing buttons that pulse on input. |
| **Regional/seasonal bezels** | Themed bezels for events, holidays, or regions. Auto-select based on lobby region. |
| **Spectator bezel** | When spectating, show both player names + connection info in a broadcast-style frame. |

---

## 5. King of the Hill Mode with Stats

### Core Loop
1. One player is the **King** (on the cabinet). Challengers queue up.
2. Winner stays, loser goes to the back of the queue.
3. **Win streak counter** prominently displayed — emulate the arcade "consecutive wins" counter.
4. If the queue is empty, King plays the CPU to stay warm.

### Stats & Leaderboards
| Stat | Description |
|---|---|
| **Current streak** | Consecutive wins as King. |
| **Longest streak (session)** | Best run this session. |
| **Total defenses** | How many challengers defeated. |
| **Character usage** | Who the King and challengers picked. |
| **Average match duration** | Time per game — identifies quick blowouts vs. nail-biters. |
| **Parry count / Super usage** | Hype stats for spectators. |

### UX Details
- **Queue visualization:** Show upcoming challengers in order, with their name and (optionally) rank/record.
- **Auto-promote:** When the King loses, the next challenger auto-connects. Seamless transitions — no lobby downtime.
- **Spectator mode:** Everyone in the queue watches the current match live.
- **Dethroned animation:** Special screen/sound when a long streak is broken ("UPSET!" with the streak count).
- **KOTH Lobby type:** Appears as a distinct room type in the lobby browser — players know what they're joining.

---

## 6. Private / Hidden Rooms

### Implementation
| Feature | Details |
|---|---|
| **Password-protected rooms** | Host sets a password; joiners must enter it. Uses HMAC-SHA256 (already in the lobby server). |
| **Invite-only (hidden)** | Room does not appear in the public lobby list. Share a room code or direct-join link. |
| **Room codes** | Short, human-readable codes (e.g., `HADOKEN-42`). Easy to share over Discord/voice chat. |
| **Allowlist / Blocklist** | Host can pre-approve specific client IDs, or ban problem players from their room. |
| **Persistent rooms** | Optionally keep the room alive even if the host disconnects briefly (grace period before dissolving). |
| **QR Code Sharing** | When a hidden/password/tournament room is created, display a QR code containing the join link/credentials. Users can take a picture and share it with friends so they can easily join without typing codes or passwords. |

### Integration
- Add `password` and `visibility` fields to the lobby server's room creation API.
- Client-side: password entry dialog in the RmlUi lobby screen.
- Room codes generated server-side, stored alongside room metadata.
- Lobby list filters: "Public", "Private (joined)", "My Rooms".

---

## 7. Match Flow / Game Flow & Blind Picks

### Match Flow Pipeline

```
┌─────────────┐    ┌──────────────┐    ┌─────────────┐    ┌───────────┐
│  Find Match  ├───►│  Blind Pick   ├───►│  Stage Ban   ├───►│  Loading   │
└─────────────┘    └──────────────┘    └─────────────┘    └─────┬─────┘
                                                                │
         ┌──────────┐    ┌──────────────┐    ┌────────────┐     │
         │  Results  │◄───┤  Match Play   │◄───┤   VS Screen │◄──┘
         └────┬─────┘    └──────────────┘    └────────────┘
              │
     ┌────────▼────────┐
     │  Rematch / Next  │
     └─────────────────┘
```

### Blind Picks

Both players select their character and Super Art **simultaneously**, without seeing the opponent's choice.

| Feature | Details |
|---|---|
| **Simultaneous selection** | Both players pick at the same time. Selections hidden until both are locked in. |
| **Lock-in confirmation** | Visual + audio cue when a player locks. Opponent sees "READY" but not the character. |
| **Reveal animation** | Dramatic character reveal once both lock in — VS splash screen. |
| **Timer** | 30-second pick timer. If time expires, last highlighted character is locked. Random if no selection. |
| **Super Art selection** | Integrated into the blind pick — select character + SA in one flow. |
| **Pick history** | Show what opponent picked in previous games of a set (for counter-picking). |

### Stage Selection
- **Random stage** (default for competitive).
- **Stage ban system** — each player bans 1–2 stages, random from remainder.
- **Loser picks** — optional rule: loser of last game picks the stage.
- **Gentleman's rule** — both players can agree on a stage override.

### Game Flow Modes
| Mode | Description |
|---|---|
| **Casual** | No restrictions. Normal character select, pick after seeing opponent. |
| **Ranked** | Blind pick enforced. Best-of-3. Stage random. ELO tracking. |
| **Tournament** | Blind pick game 1, loser counter-picks games 2+. Best-of-3 or best-of-5. |
| **First-to-N** | Continuous play until one player reaches N wins. Great for long sets. |

---

## 8. Fight Requests — Matchmaking & Ranked System

A comprehensive matchmaking system designed to cater to beginners, casuals, and veterans while maintaining fairness and preventing smurfing and skill fabrication.

### Search Configuration

Players configure their Fight Request preferences before searching. All settings persist between sessions.

| Setting | Options | Notes |
|---|---|---|
| **Search Ranked** | On / Off | Include the ranked pool in matchmaking. |
| **Search Casual** | On / Off | Include casual opponents. At least one pool must be active. |
| **Search Scope** | LAN / Regional / World | Progressively wider geographic range. |
| **Skill Range** | Low / Same / High | Target opponents below, at, or above your skill level. |
| **Connection Quality** | 1–5 bars minimum | Reject matches below this threshold. |
| **Allow Wi-Fi** | On / Off | Filter out Wi-Fi connections for maximum stability. |
| **Jailed Players** | On / Off *(casual only)* | Whether to include jailed players in the search pool. |

### Character Lock

| Setting | Behaviour |
|---|---|
| **On** | Enables **win streak display and bonuses**. On first search (zero wins), character select appears. After getting a win the character is locked for the duration of the streak. Turning this off **resets any existing win streak**. |
| **Off** | Win streak hidden, no streak bonuses. Player can freely re-select between matches. If the opponent has Lock **on**, only they are fixed — the unlocked player may still pick freely. |

> [!IMPORTANT]
> Character Lock creates a risk/reward dynamic: locking in earns streak bonuses but removes the ability to counter-pick. This naturally rewards character loyalty and mastery.

### Character Ban

Available in **all Casual matches** and in **Ranked up to a configurable rank threshold** (e.g., Gold and below).

| Rule | Details |
|---|---|
| **Ban up to 2 characters** | Each banned character applies a **ranked-point penalty multiplier** to that player's wins — the ban holder is penalized, not the opponent. |
| **Stacking bans** | If both players ban, up to 4 characters are removed from the pool. |
| **Streak interaction** | Win streak display and bonuses are **inactive** while character bans are enabled. |
| **Rank gating** | Players above the rank threshold can only enable character bans in casual mode. You cannot climb to the top while avoiding matchups. |

> [!NOTE]
> The penalty multiplier means a player banning 2 characters earns ~0.7× ranked points per win. This lets lower-skill players enjoy the game while naturally capping their rank progression.

### Pre-Match Acceptance

Before a match begins, each player sees **only**:
- Connection quality (bars)
- Opponent's search mode (Ranked / Casual)

**Player names are hidden** until the match starts. This prevents:
- **Player dodging** — can't avoid known strong players.
- **Win trading** — can't identify friends to throw matches.
- **Intimidation** — no reputation bias before the match.

### Win Streaks

Win streaks are a core progression mechanic when Character Lock is on:

| Feature | Details |
|---|---|
| **Streak counter** | Prominently displayed during and between matches. |
| **Unique opponents count** | Shows how many *different* players were defeated in the streak. |
| **Highest streak beaten** | If an opponent was on a streak, display the streak number that was ended. |
| **VS Screen fight card** | Displays both players' streak info, unique opponents, and highest streak broken — creating a hype moment before the match. |
| **Streak-breaker bonus** | Ending someone's win streak awards bonus ranked points proportional to the streak length. |
| **Priority matchmaking** | After reaching a threshold (e.g., 10+ wins), matchmaking prioritizes other streak holders or higher-ranked players. |

### Ranked Point System

```
Base Points  ×  Streak Bonus  ×  Character Ban Penalty  ×  Disconnect Modifier
     │               │                    │                        │
     │               │                    │                        └─ Reduced gains
     │               │                    │                           if recent DCs
     │               │                    └─ 0.7× per banned character
     │               └─ Multiplier grows with streak length
     └─ Base ELO/Glicko-2 delta from skill difference
```

### Graceful Quit System

If the connection is poor, both players can **quit without penalty** during the first round:

1. **Request:** Hold Start → a quit request message appears at the top while the game continues.
2. **Reason toggle:** Tap Start to cycle through reasons: *Bad Connection*, *Wrong Character*, *Controller Issue*, etc.
3. **Cancel:** Hold Start again to cancel.
4. **Mutual quit:** If the opponent also holds Start, the match ends with **no disconnect penalties** for either player.
5. **One-sided quit:** Normal disconnect penalties apply.

> [!TIP]
> The reason display serves double duty: it communicates intent to the opponent and provides analytics data for matchmaking quality improvements.

### Jail System 💀

Anti-abuse mechanism for the casual pool:

| Trigger | Consequence |
|---|---|
| **Consistent disconnect pattern** | Player is flagged and moved to the Jail pool. |
| **Suspected cheating** | Player is jailed pending review. |
| **Jail restrictions** | Jailed players **cannot** enter the ranked pool. They can only match with other jailed players, or casual players who have "Jailed Players: On" in their search settings. |
| **Redemption** | Complete N matches without disconnecting to leave Jail. Repeat offenses increase the threshold. |

---

## 9. Attract Mode & Information Bar

### Online-Aware Attract Mode

When the game is idle at the title screen and an internet connection is available, replace the traditional arcade attract mode with live online content:

| Classic (Offline) | Online-Enhanced |
|---|---|
| CPU vs. CPU demo fights | **Download and replay a random recorded online match** (player names hidden). |
| High-score table | **Live server stats** — daily active players, most-played characters, total matches today, server uptime. |
| Win streaks screen | **Live win streak leaderboard** — top 10–20 players currently on an active win streak. Shows character, streak count, and region. |

> [!NOTE]
> If no internet connection is detected, the attract mode falls back to the classic arcade behaviour: CPU demo fights, local high scores, and local win streak records.

### Information Bar

A persistent information bar at the bottom of the screen that adapts its content based on context:

| Context | Content |
|---|---|
| **Option menus** | Brief description of the currently highlighted menu item + which buttons are Confirm and Back. |
| **Attract mode** | Scrolling ticker with online information: announcements, server status, scheduled downtime, upcoming tournament notifications. |
| **Lobby** | Room count, player count, your ping to the lobby server. |

---

## 10. Additional Ideas

### Spectator Mode
- **Live spectating** of any public match from the lobby.
- **Spectator count** shown to players ("3 watching").
- **Rewind** — spectators can rewind and rewatch moments without affecting the live stream.
- Input display overlay for spectators (see what both players are pressing).
- **Commentary mode** — designated spectator can overlay text/voice (stretch goal).

### Ranked / Rating System
- ~~**ELO or Glicko-2** rating per player.~~ ✅ **DONE** — Glicko-2 with rating + RD, implemented server-side via `LobbyServer_GetPlayerStats()`.
- ~~**Rank tiers** with icons/borders: Bronze → Silver → Gold → Platinum → Diamond → Master → Legend.~~ ✅ **DONE** — `tier` field returned by server, displayed in leaderboard.
- **Seasonal resets** — soft reset each season, placement matches.
- **Character-specific rating** — separate rank per character to encourage variety.
- ~~**Leaderboards** — global, regional, and per-character.~~ ✅ **DONE** — paginated via `LobbyServer_GetLeaderboard()` with full RmlUi display in [rmlui_leaderboard.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_leaderboard.cpp). Per-character leaderboards not yet implemented.

### Community & Social
- **Player profiles** — avatar, bio, main character, stats, match history.
- **Friends list** — add players from lobbies, see online status, challenge directly.
- **Clan / Team tags** — display `[TEAM]PlayerName` in lobbies and matches.
- **Match history** — searchable log of recent matches with opponent, result, rating change.

### Quality of Life
- ~~**Connection quality indicator** in lobby list (green/yellow/red based on region + estimated ping).~~ ✅ **DONE** — `ping_class` (good/ok/bad), `conn_type`, country flags rendered per player in network lobby.
- **Auto-region detection** — detect player region and default to optimal lobby server.
- **Notification sounds** — when a match is found, when a challenger enters KOTH queue, etc.
- **Idle timeout** — auto-kick idle players from lobbies to keep rooms active.

---

## Priority Matrix

> Suggested prioritization based on community impact and implementation complexity.

| Priority | Feature | Effort | Impact |
|---|---|---|---|
| 🔴 High | Fight Requests / Matchmaking | High | Critical — core online experience |
| 🔴 High | Private / Hidden Rooms | Low | High — ✅ Create/join with passwords & visibility complete; remaining: persistence, filters |
| 🔴 High | Replay Autosaving | Low | High — ✅ v2 header, string filenames, and auto-save complete; remaining: retention policy, bookmarks |
| 🔴 High | Match Flow / Blind Picks | Medium | High — competitive integrity |
| 🟡 Medium | King of the Hill | Medium | High — social/arcade atmosphere |
| 🟡 Medium | Attract Mode & Info Bar | Low | Medium — polish, community engagement |
| 🟡 Medium | Dynamic Bezel (Netplay) | Low | Medium — polish and immersion |
| 🟡 Medium | In-Match Chat | Medium | Medium — social connectivity |
| 🟡 Medium | Mod Menu via Game Menus | Medium | Medium — discoverability |
| 🟡 Medium | Tournaments | Medium | High — ⚠️ room-based architecture reuses SSE/match/P2P infrastructure; new work: bracket logic, multi-match slots, bracket UI, TO controls |
| 🟢 Low | Ranked / Rating System | Medium | High — ⚠️ Glicko-2, tiers, leaderboards, match reporting already exist; remaining: per-char rating, seasonal resets |
| 🟢 Low | Spectator Mode | Medium | Medium — ⚠️ core spectating works (4 viewers, 15f delay); remaining: count display, rewind, commentary |

---

*Last updated: March 23, 2026*
