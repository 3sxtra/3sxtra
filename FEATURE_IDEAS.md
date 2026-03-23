# 3SXtra — Feature Ideas & Roadmap

> Living document of planned features, brainstorms, and design notes.
> Captured from team discussions (March 2026) and expanded with implementation context.

---

## Implementation Status & Gap Analysis

> [!NOTE]
> Audit performed March 23, 2026. For each feature area, this section describes **what exists in code today**, **what the vision calls for**, and **what remains to be built** — with specific file and function references.

---

### §1. Mod Menu & FX Screens

**What exists:**
The mods menu is implemented as an RmlUi data-bound overlay in [rmlui_mods_menu.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_mods_menu.cpp) (14KB, ~341 lines), toggled via F3 in [sdl_app_input.c](file:///d:/3sxtra/src/port/sdl/app/sdl_app_input.c). It exposes toggles for shader bypass, fast pre-game, HD stages, and bezel on/off. A separate shader menu exists in [rmlui_shader_menu.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_shader_menu.cpp) (19KB), and stage config in [rmlui_stage_config.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_stage_config.cpp) (10KB). Per-component phase 3 toggles are defined in [rmlui_phase3_toggles.h](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_phase3_toggles.h) and owned by `rmlui_game_hud.cpp`, allowing runtime fallback to CPS3 rendering per HUD element. Hotkeys F2–F9 are individually handled for different overlays.

**What the vision calls for:**
A **unified in-game menu tree** accessible from the main or pause menu — not scattered F-key overlays. Per-character FX overrides (hit-spark styles per character), named mod profiles/presets with one-click swap, automatic discovery of mod packs in `assets/`, and live previews showing a thumbnail before committing.

**The gap:**
The individual overlay screens work well, but they are **islands** — there's no parent menu that ties them together. A user who doesn't know the F-key shortcuts can't discover these features. The menu system in [ms_mode_select.c](file:///d:/3sxtra/src/port/screens/ms_mode_select.c) and [ms_option_select.c](file:///d:/3sxtra/src/port/screens/ms_option_select.c) would need new entries to link into the mods/shaders/stages screens. Mod profiles need a persistence layer (likely extending [native_save.c](file:///d:/3sxtra/src/port/save/native_save.c)). Mod discovery requires scanning `assets/` subdirectories and building a registry of available packs — nothing exists for this. Per-character FX overrides would need new configuration in the render pipeline. Live previews are a significant UI investment.

---

### §2. Replay Autosaving & In-Match Chat

**What exists:**
Replay recording and playback are mature — [sys_replay.c](file:///d:/3sxtra/src/sf33rd/Source/Game/system/sys_replay.c) handles the CPS3-native replay format, and [rmlui_replay_picker.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_replay_picker.cpp) provides a 20-slot RmlUi picker. The save/load flow goes through [ms_save_replay.c](file:///d:/3sxtra/src/port/screens/ms_save_replay.c) which has a full enter/tick/exit lifecycle with RmlUi integration. **Auto-saving already works for netplay matches**: `NativeSave_AutoSaveReplay()` is called in [sdl_netplay_ui.cpp](file:///d:/3sxtra/src/port/sdl/netplay/sdl_netplay_ui.cpp#L974-L979) when a match ends naturally (guarded by `PL_Wins[0] + PL_Wins[1] > 0` to avoid corrupt data from early disconnects). Replay slots 0-9 are reserved for manual saves; slots 10-19 for auto-saves (see [native_save.c](file:///d:/3sxtra/src/port/save/native_save.c#L704)). Additionally, `AsyncReportMatch()` in `sdl_netplay_ui.cpp` snapshots `Replay_w` from memory and uploads it to the server via `LobbyServer_UploadReplay()` on a background thread — so replays are both saved locally and uploaded to the server automatically.

Chat is implemented in the casual lobby via [rmlui_casual_lobby.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_casual_lobby.cpp) (39KB) with text input, message history (50-message buffer), and real-time delivery via SSE (`SSE_EVENT_CHAT`). Server-side chat is handled through `LobbyServer_SendChat()`.

**What the vision calls for:**
Configurable retention policies (keep last N, last N days), metadata auto-tagging (characters, stage, winner, match duration, date, opponent name), highlight bookmarks during gameplay (hotkey to mark a moment), descriptive auto-naming (`2026-03-14_Ken-vs-Chun_ranked_W.rep`). For chat: pre-set quick messages (stickers/emotes) during gameplay, a quick-chat wheel via D-pad, between-rounds text chat, speech bubble indicators, and profanity filtering.

**The gap:**
Auto-save *works*, but lacks the envisioned metadata and naming — replays are saved with a slot index, not descriptive filenames with match context. The `NativeReplayHeader` struct (defined in `sdl_netplay_ui.cpp`) only stores magic/version/size/reserved — it has no fields for characters, winner, or stage. Adding these would require extending the header and the save path. Retention policy doesn't exist — auto-save slots rotate through 10-19 and presumably overwrite. Highlight bookmarks would require a new input handler during gameplay that records frame numbers into the replay metadata.

For chat, the gap is larger: lobby chat exists but **nothing runs during actual gameplay**. The quick-chat wheel (D-pad emote selection without typing) is an entirely new UI component. Between-rounds chat would need to hook into the round transition flow in `game.c`. The infrastructure (SSE, message delivery) is there — the problem is purely UX/timing: chat is only visible when the lobby RmlUi document is active, and that's hidden during matches.

---

### §3. Tournaments

**What exists:**
Nothing tournament-specific. The lobby server has rooms, queues, match reporting, and FT tracking — but no bracket management, no tournament state machine, and no server-side endpoints for tournament lifecycle.

**What the vision calls for:**
Full bracket systems (single/double elimination, round robin, Swiss), tournament creation from the Network menu, player registration with live bracket display, auto-matching from bracket positions via existing STUN/UPnP, auto-reported results, and TO manual overrides. Server infra: `POST /tournament/create`, `POST /tournament/join`, `GET /tournament/bracket`, `POST /tournament/report`.

**The gap:**
This is **entirely greenfield**. The existing `LobbyServer_ReportMatch()` could feed results into brackets, and the SSE streaming could push bracket updates — so the networking foundation exists. But the bracket logic (seeding, losers bracket, Swiss pairing), tournament-specific UI screens, and the server API all need to be built from scratch. This is the largest single effort in the document.

---

### §4. Dynamic Bezel During Netplay

**What exists:**
The bezel system is well-established: [sdl_app_bezel.c](file:///d:/3sxtra/src/port/sdl/app/sdl_app_bezel.c) and [sdl_bezel.c](file:///d:/3sxtra/src/port/rendering/sdl_bezel.c) handle loading and rendering 40+ per-character arcade bezels. Bezels auto-swap when the selected character changes. The toggle lives in the mods menu (`rmlui_mods_menu.cpp`). Netplay stats (ping, rollback frames, connection quality) are rendered separately by [netstats_renderer.c](file:///d:/3sxtra/src/port/sdl/netstats_renderer.c) and the RmlUi netplay overlay [rmlui_netplay_ui.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_netplay_ui.cpp).

**What the vision calls for:**
**Opponent-aware combined bezels** (P1's character on left panel, P2's on right — both players see a combined image). Netplay stats embedded into the bezel artwork (LED-style indicators in the cabinet art, not a separate overlay). Win/loss session record integrated into the bezel. Animated bezels (flickering cabinet lights, scrolling marquee with player names, input-reactive button glow). Regional/seasonal themed bezels. A spectator-specific broadcast-style frame.

**The gap:**
The current bezel system renders a single full-frame image per character — it has no concept of left/right panels or compositing two character bezels. Implementing opponent-aware bezels would require either: (a) generating composite images at runtime from half-bezel assets, or (b) pre-rendering all 20×20 character pairings (400 images, impractical). Option (a) means splitting bezel artwork into P1-side and P2-side panels and compositing them in the renderer.

Embedding stats into the bezel is a rendering integration task — currently stats and bezels are in different render layers. Animated bezels would need per-frame texture updates or sprite sheet playback in the bezel renderer, which currently handles static textures only. Seasonal/regional bezels need a selection mechanism but are otherwise just additional artwork.

---

### §5. King of the Hill

**What exists:**
The foundation is strong. The lobby server already implements **winner-stays-on rotation**: `LobbyServer_ReportMatchEnd()` sends winner/loser to the server, which auto-rotates the queue. `SSE_EVENT_MATCH_END` carries `match_winner_id` and `match_loser_id`. The queue system (`LobbyServer_JoinQueue/LeaveQueue`, `queue[MAX_ROOM_PLAYERS]` in `RoomState`) manages challenger ordering. Match proposals (`SSE_EVENT_MATCH_PROPOSE` → accept/decline Phase 6) handle the transition to the next match.

Win streak display exists in two places: [win.c](file:///d:/3sxtra/src/sf33rd/Source/Game/screen/win.c) for the arcade win screen, and [rmlui_win_screen.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_win_screen.cpp) which binds `WGJ_Win` (consecutive wins) and `WGJ_Score` as an RmlUi overlay showing winner name, score, and streak text ("1st WIN", "2nd WIN+").

Spectating is available: casual lobby has a spectate button, and [netplay.c](file:///d:/3sxtra/src/netplay/netplay.c) supports up to 4 spectators with 15-frame delay.

**What the vision calls for:**
A dedicated KOTH *mode* — not just a room with rotation, but a recognised room type in the lobby browser ("KOTH" label), with KOTH-specific stats (current streak, longest streak this session, total defenses, character usage, average match duration, parry/super counts), visible queue lineup with names/ranks, auto-promote with seamless transitions, CPU fill for empty queues, and a "dethroned" animation when a long streak is broken.

**The gap:**
The **mechanics** work (rotate, queue, streak count), but the **KOTH experience** doesn't exist. There's no KOTH room type in `RoomListItem` — all rooms look the same in the lobby browser. There's no KOTH-specific stats tracking beyond the basic `WGJ_Win` counter — session aggregate stats (longest streak, total defenses, character usage breakdowns) aren't tracked. The queue visualization in the casual lobby shows player names but not in a KOTH-themed way. The "dethroned" animation is purely cosmetic but signals the mode identity. CPU fill when the queue is empty would require triggering local AI matches while waiting — conceptually simple but untested in the current match flow.

Implementing KOTH mostly means: (1) add a `room_type` field to the room API, (2) build a KOTH-flavored variant of the casual lobby UI, (3) wire server-side stats aggregation for the KOTH metrics, (4) add the dethroned/upset screen as an RmlUi overlay.

---

### §6. Private / Hidden Rooms

**What exists:**
Room creation works: `LobbyServer_CreateRoom(name, ft, out_room)` creates a room with a server-assigned 4-character code. `LobbyServer_ListRooms()` returns all active rooms. `LobbyServer_JoinRoom()` accepts a room code. The room code system already acts as a simple form of private access — you can share a code and only people who know it can join. But rooms are not truly hidden from the public list.

The invite cooldown/decline system exists: `LobbyServer_DeclineInvite()` reports to the server, and `add_declined_player()` in `sdl_netplay_ui.cpp` enforces a local 30-second cooldown with configurable duration via `CFG_KEY_NETPLAY_INVITE_COOLDOWN`.

**What the vision calls for:**
**Password-protected rooms** (HMAC-SHA256 — SHA256 is already available in [sha256.c](file:///d:/3sxtra/src/netplay/sha256.c)). **Hidden rooms** that don't appear in `ListRooms()`. Human-readable room codes (e.g., `HADOKEN-42` instead of 4-char codes). Per-room allowlists/blocklists. Persistent rooms with a grace period when the host briefly disconnects.

**The gap:**
The server API needs: a `password` parameter in `CreateRoom`, a `visibility` field (public/hidden), allowlist/blocklist storage per room, and a host-migration grace timer. On the client: a password-entry dialog in the RmlUi lobby join flow, and lobby list filtering (Public / Private / My Rooms). SHA256 is available for the HMAC. The room code format is cosmetic — the server generates codes, so switching to word-based codes is a server change.

All of this is **API extensions + UI changes** — no fundamental architectural work. The server already stores room state with SSE streaming, so adding fields is straightforward.

---

### §7. Match Flow / Game Flow & Blind Picks

**What exists:**
**Character select** is fully implemented in [rmlui_char_select.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_char_select.cpp) (485 lines) — HD character portraits (select and versus variants for all 21 characters), Super Art name/numeral display per player, stage select with country names and flag images, BCD-decoded countdown timer with per-phase visibility gating, player visibility flags (solo/dual/confirmed states), and auto-hide when `Play_Game` starts.

**VS screen** overlays character names via [rmlui_vs_screen.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_vs_screen.cpp). The **match proposal** system is complete: `SSE_EVENT_MATCH_PROPOSE` delivers P1/P2 info (name, connection type, RTT, region, room code), `LobbyServer_AcceptMatch/DeclineMatch` handles responses, and `SSE_EVENT_MATCH_DECLINE` carries reason ("declined" or "timeout"). FT is tracked per room and per match proposal.

The **game HUD** in [rmlui_game_hud.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_game_hud.cpp) (549 lines, 52 bindings) provides: health bars with drain animation, round timer, stun gauge, SA gauge with stock/fill/percentage, combo counter with hit kind, parry counter with red parry flag, per-round result bubbles (V/P/C/D/J/S types), score, and player names — all with per-component phase 3 toggles allowing fallback to CPS3 sprites.

**What the vision calls for:**
**Blind picks** — both players select simultaneously with selections hidden until both lock in, with a reveal animation. A structured match flow pipeline: Find Match → Blind Pick → Stage Ban → Loading → VS Screen → Match → Results → Rematch. Stage ban system (each player bans 1-2 stages, random from remainder, loser picks option). Game flow modes: Casual (open picks), Ranked (blind enforced, BO3, random stage), Tournament (blind G1, counter-pick G2+), FT-N.

**The gap:**
Character select works perfectly for local/standard play, but **blind pick requires a network synchronization layer** that doesn't exist. Currently both players see the same character select screen in real-time — implementing blind picks means: (a) hiding the opponent's cursor/portrait until lock-in, (b) adding lock-in state sync messages to the netplay protocol, (c) building the reveal animation. The existing `rmlui_char_select.cpp` bindings track `Sel_PL_Complete[0/1]` and `My_char[0/1]` — these could drive blind pick visibility gating, but the netcode doesn't currently decouple selection visibility from selection state.

Stage bans need a new pre-match negotiation step — the current flow goes directly from character confirm to VS screen. The "match flow pipeline" diagram would require a state machine layered on top of the existing `menu_task_phases.h` phase system.

---

### §8. Fight Requests — Matchmaking & Ranked

**What exists:**
The **server infrastructure is mature**. `LobbyServer_ReportMatch()` implements cross-validated match reporting with 4 statuses (pending/in_progress/complete/dispute). `LobbyServer_GetPlayerStats()` returns Glicko-2 stats: `rating`, `rd` (rating deviation), `tier` (bronze/silver/gold/etc.), `wins`, `losses`, `disconnects`. `LobbyServer_ReportDisconnect()` handles ragequit detection with 30-second server-side timeout. Leaderboards are paginated via `LobbyServer_GetLeaderboard()` with full RmlUi display in [rmlui_leaderboard.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_leaderboard.cpp). `LeaderboardEntry` includes `rank`, `rating`, `tier`, `grade`, `most_played_char`, `country`, and `disconnects`.

The **connection quality filtering is implemented**: `player_passes_filters()` in [sdl_netplay_ui.cpp](file:///d:/3sxtra/src/port/sdl/netplay/sdl_netplay_ui.cpp#L276-L301) checks three criteria: region lock (`CFG_KEY_NETPLAY_REGION_LOCK`), max ping (`CFG_KEY_NETPLAY_MAX_PING` using P2P RTT from `PingProbe_GetRTT()` with triangulated fallback), and Wi-Fi block (`CFG_KEY_NETPLAY_BLOCK_WIFI`). Invites that fail these checks are auto-declined with a reason logged and shown in the status message. The searching system (`LobbyServer_StartSearching/StopSearching/GetSearching`), region detection, Wi-Fi/wired detection ([net_detect.c](file:///d:/3sxtra/src/netplay/net_detect.c)), and P2P ping probing ([ping_probe.c](file:///d:/3sxtra/src/netplay/ping_probe.c)) are all operational.

The anti-spam system works: declined invites trigger a local cooldown (configurable, default 30s) via `add_declined_player()`, and `LobbyServer_DeclineInvite()` reports to the server for rate limiting.

**What the vision calls for:**
**Character Lock** (lock character for duration of win streak, with bonuses/penalties), **Character Ban** (ban up to 2 characters with ranked point penalty multiplier, rank-gated in ranked mode), **ranked matchmaking queue** (auto-match based on skill, not manual server browsing), **hidden names pre-match** (only show connection quality and mode to prevent dodging), **win streak bonuses** (ranked point multiplier growing with streak, unique-opponent counter, streak-breaker bonus), **priority matchmaking** (long-streak players matched against other streak holders), **graceful quit** (hold-Start protocol for mutual no-penalty quit during round 1 with reason display), **jail system** (disconnect pattern detection → jail pool, restricted to casual + opted-in opponents, redemption path), **skill range matching** (Low/Same/High targeting).

**The gap:**
Every gap here is a **client-side gameplay feature** or a **matchmaking algorithm** — the server plumbing is ready. Specifically:

- **Character Lock/Ban**: Pure client-side state management + UI. The character select flow needs to read lock/ban config, hide banned characters, and enforce lock between matches. The ranked point modifier needs a multiplier field in the match report.
- **Ranked matchmaking queue**: Currently players browse a list and manually connect. True matchmaking means the server pairs players automatically based on rating proximity. This requires a new server endpoint (e.g., `POST /matchmaking/enqueue` + SSE event for match found) and a client UI that shows "Searching..." instead of a player list.
- **Hidden names**: Simple — the match proposal already shows P1/P2 info. Omit the `display_name` field until post-accept.
- **Win streak bonuses/priority matching**: The win counter exists (`WGJ_Win`), but there's no streak-aware ranked point modifier and no priority matching logic on the server.
- **Graceful quit**: An entirely new in-game protocol — hold-Start detection, reason cycling, mutual confirmation, penalty exemption. Needs hooks in the game loop and the netplay protocol.
- **Jail system**: Server already tracks `disconnects`. Adding jail means: a `jailed` flag in player state, server logic to auto-jail on disconnect threshold, the `jailed_players` search filter, and a rehabilitation counter.
- **Skill range matching**: Requires server-side matchmaking (not just filtering) that considers rating bands.

---

### §9. Attract Mode & Information Bar

**What exists:**
The attract mode (CPU vs. CPU demo fights) is original arcade code: [demo01.c](file:///d:/3sxtra/src/sf33rd/Source/Game/demo/demo01.c) handles title screen and attract-mode title sequences, [demo02.c](file:///d:/3sxtra/src/sf33rd/Source/Game/demo/demo02.c) runs the in-game demo (character select + gameplay). The demo data in [demo_dat.c](file:///d:/3sxtra/src/sf33rd/Source/Game/demo/demo_dat.c) provides controller input-replay data for demo sequences, and [demo_states.h](file:///d:/3sxtra/src/sf33rd/Source/Game/demo/demo_states.h) defines the state machine (quick title, quick-start attract, full attract with char select).

An **HD attract overlay** exists in [rmlui_attract_overlay.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_attract_overlay.cpp) — it shows an HD `logo_small.png` and a blinking "PRESS START BUTTON" prompt during Loop_Demo cases 3/5, with controlled show/hide of the logo triggered from `SF3_logo()` in `sc_sub.c`.

Ranking/high-score tables exist in [sys_ranking.c](file:///d:/3sxtra/src/sf33rd/Source/Game/system/sys_ranking.c).

**What the vision calls for:**
An **online-aware attract mode** that replaces CPU demo fights with downloaded online match replays (player names hidden), displays live server stats (daily active players, most-played characters, total matches today), and shows a live win streak leaderboard (top 10-20 currently active streaks). Graceful offline fallback. Plus an **information bar** — a persistent bottom-of-screen bar that adapts content by context: menu item descriptions in options, scrolling ticker with announcements/server status in attract, room/player counts in lobby.

**The gap:**
The attract mode plays hardcoded CPU input sequences — it can't play back online replays because the replay download API doesn't exist yet on the server (replay *upload* works, but there's no `GET /replay/random` endpoint). Wiring it in would mean: (1) new server endpoint to serve random replays, (2) download and inject into the existing replay playback system, (3) suppress the "PRESS START" overlay during online replays. The leaderboard data is already fetchable via `LobbyServer_GetLeaderboard()` — it just needs to be rendered during attract mode instead of only in the network menu.

The information bar is a standalone new RmlUi component — a persistent overlay document with context-aware bindings. No code exists for it. The main challenge is deciding which RmlUi context to use (it spans game and menu contexts) and how to source context descriptions for menu items (the existing menu system doesn't expose human-readable help text for its items).

---

### §10. Additional Ideas

**What exists:**
**Spectator mode** is functional in [netplay.c](file:///d:/3sxtra/src/netplay/netplay.c) — it creates a spectate-only GekkoNet session with 15-frame delay, supports up to 4 spectators, handles connected/disconnected/paused/unpaused events, and processes advance + load game events (no saves). The casual lobby in `rmlui_casual_lobby.cpp` has a spectate button gated on `can_spectate` (not playing, match active, not already spectating). The network adapter in [sdl_net_adapter.c](file:///d:/3sxtra/src/netplay/sdl_net_adapter.c) caches up to 8 unique peers (1v1 + spectators).

**Input display** is a full RmlUi data-bound overlay in [rmlui_input_display.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_input_display.cpp) (335 lines) with per-frame input history tracking and show/hide via the mods menu toggle `mods_menu_input_display_enabled`.

**Frame data display** exists in [rmlui_frame_display.cpp](file:///d:/3sxtra/src/port/sdl/rmlui/rmlui_frame_display.cpp) (342 lines) — frame meter with startup/active/recovery/hitstun/blockstun/down states, stats text (startup F / total F / advantage), per-player colour-coded cell arrays, idle detection (clear after 90 frames of double-idle), training mode only.

**Player stats on server**: `LobbyServer_GetPlayerStats()` returns wins/losses/disconnects/rating/rd/tier. `LeaderboardEntry` has rank, most-played character, country, grade. Country is server-derived from IP (ISO 3166-1 alpha-2). Player identity is managed via [identity.c](file:///d:/3sxtra/src/netplay/identity.c) with SHA256-based ID generation.

**What the vision calls for:**
Spectator enhancements: spectator count shown to players, rewind without affecting live stream, input display for spectators, commentary mode. Ranked/rating: character-specific ratings, seasonal resets with placement matches. Community: player profiles (avatar, bio, stats, match history), friends list, clan tags. Quality of life: connection quality indicators in lobby list, auto-region detection, notification sounds, idle timeout.

**The gap:**
- **Spectator** works but is bare — no spectator count display (the data is available server-side in `RoomState` but not surfaced), no rewind (would require circular buffer of game states on the spectator side), no commentary system.
- **Rating system**: Glicko-2 exists server-side but is global (not per-character). Adding character-specific ratings needs a `{player_id, character}` composite key in the server DB. Seasonal resets and placement matches are server config changes.
- **Social features** (profiles, friends, clans): Nothing exists. These are full-stack features needing server storage, client UI, and presence systems.
- **QoL**: Connection quality indicators partially exist — `rtt_ms` and `connection_type` are in `LobbyPlayer`, `PingProbe_GetRTT()` provides P2P measurements. But the lobby room list UI doesn't render this data as colored indicators. Auto-region detection is not implemented (region is manually set via `CFG_KEY_LOBBY_REGION`). Notification sounds and idle timeout don't exist.

---

### Summary

```
Feature Area                  What Exists                           What's Missing
──────────────────────────────────────────────────────────────────────────────────────
§1 Mod Menu          Mods/shader/stage menus (separate)     Unified tree, profiles,
                     F-key toggles, phase3 per-component    discovery, previews,
                                                            per-char FX overrides

§2 Replays & Chat    Auto-save + upload, 20-slot picker,    Metadata tagging, named
                     lobby chat (SSE, 50-msg buffer)        files, bookmarks, in-match
                                                            quick chat, emotes

§3 Tournaments       Nothing                                Everything — brackets, API,
                                                            auto-matching, TO tools

§4 Dynamic Bezel     40+ char bezels, auto-swap             Opponent-aware compositing,
                                                            stats-in-bezel, animation,
                                                            spectator frame

§5 KOTH              Rotation + queue + streak count        KOTH room type, session
                     (server-side, casual lobby)            stats, queue viz, dethroned
                                                            anim, CPU fill

§6 Private Rooms     Room codes, create/join                Password, hidden visibility,
                                                            allow/blocklists, persistence

§7 Match Flow        Char select (HD portraits, SA,         Blind pick sync, stage
                     timer, stage select), VS screen,       bans, match flow state
                     accept/decline proposals               machine, ranked/tournament
                                                            game flow modes

§8 Fight Requests    Glicko-2, match reporting,             Character lock/ban,
                     disconnect tracking, leaderboards,     ranked auto-match, hidden
                     region/ping/WiFi filtering,            names, streak bonuses,
                     searching, anti-spam                   graceful quit, jail,
                                                            skill range pairing

§9 Attract Mode      CPU demos, HD overlay,                 Online replay playback,
                     arcade ranking tables                  live stats, live streaks,
                                                            information bar

§10 Additional       Spectator (4 max, 15f delay),          Spectator count/rewind,
                     input display, frame data,             per-char rating, seasonal
                     player stats/identity                  resets, profiles, friends,
                                                            clans, match history UI
```

> [!IMPORTANT]
> **Key finding:** The **server infrastructure and netplay plumbing are significantly ahead of the client-side UX**. Match reporting with cross-validation, Glicko-2 stats, disconnect tracking, P2P ping probing, Wi-Fi detection, connection quality filtering, replay auto-save + upload, winner-stays-on rotation, spectator support, and the full SSE streaming system are all operational. The remaining work falls into three categories:
>
> 1. **New gameplay protocols** — blind picks, character lock/ban, graceful quit, KOTH mode (require netcode + game loop changes)
> 2. **Server-side matchmaking** — ranked queue, skill range pairing, jail system, priority matching (server algorithm work)
> 3. **Client-side UX** — unified menus, information bar, tournament UI, private room dialogs, KOTH stats, connection indicators in lobby (RmlUi screens)

---

## 1. Mod Menu & FX Screens via Game Menus

**Original idea:** Expose mod configuration and visual-effects screens through the in-game menu system.

### What this means
- Unify the **F3 mods menu** and shader/FX controls into a single, discoverable in-game menu tree accessible from the main menu or pause menu.
- Let players toggle **HD stages**, **sprite overrides**, **bezel packs**, **shader presets**, and **audio mods** without memorizing hotkeys.
- Present live previews: show a thumbnail or looping clip of the active shader/stage/bezel before committing.

### Augmented scope
| Sub-feature | Details |
|---|---|
| **Per-character FX overrides** | Individual hit-spark and super-flash styles per character (e.g., classic CPS3 sparks vs. HD redrawn). |
| **Mod profiles / presets** | Save and name combinations of visual settings ("Tournament Clean", "Maximum Drip"). One-click swap. |
| **Mod discovery** | Scan `assets/` at startup, auto-populate the menu. Show installed vs. missing mod packs. |
| **Hot-reload** | Apply mod/shader changes without restarting the game. Already partially supported via F2/F4 — extend to all mod types. |

---

## 2. Replay Autosaving & In-Match Chat

### Replay Autosaving
**Original idea:** Automatically save replays without manual action.

- **Auto-save every completed match** to `replays/` with zero user interaction.
- Configurable retention policy: keep last N replays, or last N days, or unlimited.
- Auto-tag replays with metadata: characters, stage, winner, match duration, date, netplay opponent name.
- **Highlight bookmarks** — press a hotkey mid-match to bookmark a moment; replay viewer jumps to bookmarks.
- **Replay naming** — auto-generate descriptive filenames: `2026-03-14_Ken-vs-Chun_ranked_W.rep`

### In-Match Chat
**Original idea:** Chat during matches.

- **Pre-set quick messages** (stickers/emotes): "GG", "Rematch?", "One more", "BRB", custom messages.
- Quick-chat wheel activated by a hotkey — select with D-pad, no typing mid-match.
- **Between-rounds text chat** — small text input during the "Ready" countdown or post-match results screen.
- **Chat history** — scrollable log in the lobby and post-match screen.
- Profanity filter toggle (opt-in).
- Visual indicator: small speech bubble above player name when opponent sends a message.

---

## 3. Tournaments

**Original idea:** Tournament support.

### Bracket System
| Feature | Details |
|---|---|
| **Single elimination** | Classic bracket, seeded or random. |
| **Double elimination** | Winners/losers bracket with grand finals reset. |
| **Round robin** | League-style, everyone plays everyone. Points-based ranking. |
| **Swiss format** | Pair players with similar records each round. Good for large pools. |

### Tournament Flow
- **Creation:** Host creates tournament from the Network menu — set name, format, max players, game settings (best-of-3/5, stage select rules, etc.).
- **Registration:** Players join via lobby code or direct link. Show bracket live as players register.
- **Auto-matching:** When a bracket match is ready, both players get a notification and auto-connect via the existing STUN/UPnP netplay.
- **Results reporting:** Match results auto-reported from game state. Manual override for TOs (tournament organizers).
- **Spectator queue:** Non-active players can spectate current matches (see §4).

### Infrastructure
- Extend the existing Node.js lobby server with tournament state management.
- Store bracket state server-side; clients poll/subscribe for updates.
- REST API endpoints: `POST /tournament/create`, `POST /tournament/join`, `GET /tournament/bracket`, `POST /tournament/report`.

---

## 4. Dynamic Bezel During Netplay

**Original idea:** Dynamic bezel during netplay.

### Current State
40+ per-character arcade bezels already auto-swap on character selection. This idea extends bezels to be **context-aware during netplay**.

### Proposed Enhancements
| Feature | Details |
|---|---|
| **Opponent-aware bezels** | Show P1's character on the left bezel panel, P2's on the right. Both players see a combined bezel. |
| **Netplay stats overlay on bezel** | Display ping, rollback frames, and connection quality as part of the bezel artwork — e.g., a small LED-style indicator embedded in the cabinet art. |
| **Win/loss streak** | Show session record (W-L) integrated into the bezel — like a coin-op win counter. |
| **Animated bezels** | Subtle animations: flickering arcade cabinet lights, scrolling marquee text with player names, glowing buttons that pulse on input. |
| **Regional/seasonal bezels** | Themed bezels for events, holidays, or regions. Auto-select based on lobby region. |
| **Spectator bezel** | When spectating, show both player names + connection info in a broadcast-style frame. |

---

## 5. King of the Hill Mode with Stats

**Original idea:** King of the Hill mode with stats.

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

**Original idea:** Private or hidden rooms.

### Implementation
| Feature | Details |
|---|---|
| **Password-protected rooms** | Host sets a password; joiners must enter it. Uses HMAC-SHA256 (already in the lobby server). |
| **Invite-only (hidden)** | Room does not appear in the public lobby list. Share a room code or direct-join link. |
| **Room codes** | Short, human-readable codes (e.g., `HADOKEN-42`). Easy to share over Discord/voice chat. |
| **Allowlist / Blocklist** | Host can pre-approve specific client IDs, or ban problem players from their room. |
| **Persistent rooms** | Optionally keep the room alive even if the host disconnects briefly (grace period before dissolving). |

### Integration
- Add `password` and `visibility` fields to the lobby server's room creation API.
- Client-side: password entry dialog in the RmlUi lobby screen.
- Room codes generated server-side, stored alongside room metadata.
- Lobby list filters: "Public", "Private (joined)", "My Rooms".

---

## 7. Match Flow / Game Flow & Blind Picks

**Original idea:** Match flow / game flow, blind picks.

### Match Flow System
A structured pre-match flow that mirrors tournament/competitive standards:

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
| **Lock-in confirmation** | Visual + audio cue when a player has locked their pick. Opponent sees "READY" but not the character. |
| **Reveal animation** | Dramatic character reveal once both players lock in — VS splash screen. |
| **Timer** | 30-second pick timer. If time expires, last highlighted character is locked. Random if no selection. |
| **Super Art selection** | Integrated into the blind pick — select character + SA in one flow. |
| **Pick history** | Show what opponent picked in previous games of a set (for counter-picking in later games if blind pick is off). |

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
| **Jailed Players** | On / Off *(casual only)* | Whether to include jailed players in the search pool (see §Jail System). |

### Character Lock

| Setting | Behaviour |
|---|---|
| **On** | Enables **win streak display and bonuses**. On first search (zero wins), the character select screen appears. After getting a win the character is locked — the player keeps using the same character for the duration of the streak. Turning this off **resets any existing win streak**. |
| **Off** | Win streak display is hidden, no streak bonuses. Player can freely re-select their character between matches. If the opponent has Character Lock **on**, only the locked player's character is fixed — the unlocked player may still pick freely. |

> [!IMPORTANT]
> Character Lock creates a risk/reward dynamic: locking in earns streak bonuses but removes the ability to counter-pick. This naturally rewards character loyalty and mastery.

### Character Ban

Available in **all Casual matches** and in **Ranked up to a configurable rank threshold** (e.g., Gold and below).

| Rule | Details |
|---|---|
| **Ban up to 2 characters** | Each banned character applies a **ranked-point penalty multiplier** to that player's wins — the ban holder is penalized, not the opponent. |
| **Stacking bans** | If both players ban, up to 4 characters are removed from the pool. |
| **Streak interaction** | Win streak display and bonuses are **inactive** while character bans are enabled. |
| **Rank gating** | Players above the rank threshold can only enable character bans in casual mode. This prevents high-rank players from gaming the system — you cannot climb to the top while avoiding matchups. |

> [!NOTE]
> The penalty multiplier means a player banning 2 characters earns, say, 0.7× ranked points per win. This lets lower-skill players enjoy the game while naturally capping their rank progression.

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
| **VS Screen fight card** | The VS screen displays both players' streak info, unique opponents, and highest streak broken — creating a hype moment before the match. |
| **Streak-breaker bonus** | Ending someone's win streak awards bonus ranked points proportional to the streak length. |
| **Priority matchmaking** | After reaching a threshold number of wins (e.g., 10+), matchmaking prioritizes other players who also have an active streak or are higher-ranked. This keeps long streaks honest — you have to beat increasingly strong opponents. |

### Ranked Point System

The ranked point calculation includes multiple modifiers to incentivize fair play:

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

1. **Request:** Hold Start button → a quit request message appears at the top of the screen while the game continues playing.
2. **Reason toggle:** Tap Start to cycle through reasons: *Bad Connection*, *Wrong Character*, *Controller Issue*, etc.
3. **Cancel:** Hold Start again to cancel the request.
4. **Mutual quit:** If the opponent also holds Start, the match ends with **no disconnect penalties** for either player.
5. **One-sided quit:** If a player quits without mutual consent, normal disconnect penalties apply.

> [!TIP]
> The reason display serves double duty: it communicates intent to the opponent (so they know it's not rage-quitting) and provides analytics data for matchmaking quality improvements.

### Jail System 💀

Anti-abuse mechanism for the casual pool:

| Trigger | Consequence |
|---|---|
| **Consistent disconnect pattern** | Player is flagged and moved to the Jail pool. |
| **Suspected cheating** | Player is jailed pending review. |
| **Jail restrictions** | Jailed players **cannot** enter the ranked pool. They can only match with other jailed players, or with casual players who have "Jailed Players: On" in their search settings. |
| **Redemption** | Complete N matches without disconnecting to leave Jail. Repeat offenses increase the threshold. |

---

## 9. Attract Mode & Information Bar

### Online-Aware Attract Mode

When the game is idle at the title screen and an internet connection is available, replace the traditional arcade attract mode with live online content:

| Classic (Offline) | Online-Enhanced |
|---|---|
| CPU vs. CPU demo fights | **Download and replay a random recorded online match** (player names hidden). Option to "like" the replay if a replay rating system is implemented. |
| High-score table | **Live server stats** — daily active players, most-played characters, total matches today, server uptime. |
| Win streaks screen | **Live win streak leaderboard** — top 10–20 players currently on an active win streak, similar to SF6's Battle Ground arena. Shows character, streak count, and region. |

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

## 10. Additional Ideas (Augmented)

These are natural extensions that complement the above features:

### Spectator Mode
- **Live spectating** of any public match from the lobby.
- **Spectator count** shown to players ("3 watching").
- **Rewind** — spectators can rewind and rewatch moments without affecting the live stream.
- Input display overlay for spectators (see what both players are pressing).
- **Commentary mode** — designated spectator can overlay text/voice (stretch goal).

### Ranked / Rating System
- **ELO or Glicko-2** rating per player.
- **Rank tiers** with icons/borders: Bronze → Silver → Gold → Platinum → Diamond → Master → Legend.
- **Seasonal resets** — soft reset each season, placement matches.
- **Character-specific rating** — separate rank per character to encourage variety.
- **Leaderboards** — global, regional, and per-character.

### Community & Social
- **Player profiles** — avatar, bio, main character, stats, match history.
- **Friends list** — add players from lobbies, see online status, challenge directly.
- **Clan / Team tags** — display `[TEAM]PlayerName` in lobbies and matches.
- **Match history** — searchable log of recent matches with opponent, result, rating change.

### Quality of Life
- **Connection quality indicator** in lobby list (green/yellow/red based on region + estimated ping).
- **Auto-region detection** — detect player region and default to optimal lobby server.
- **Notification sounds** — when a match is found, when a challenger enters KOTH queue, etc.
- **Idle timeout** — auto-kick idle players from lobbies to keep rooms active.

---

## Priority Matrix

> Suggested prioritization based on community impact and implementation complexity.

| Priority | Feature | Effort | Impact |
|---|---|---|---|
| 🔴 High | Fight Requests / Matchmaking | High | Critical — core online experience |
| 🔴 High | Private / Hidden Rooms | Low | High — most requested for friend groups |
| 🔴 High | Replay Autosaving | Low | High — data preservation, community clips |
| 🔴 High | Match Flow / Blind Picks | Medium | High — competitive integrity |
| 🟡 Medium | King of the Hill | Medium | High — social/arcade atmosphere |
| 🟡 Medium | Attract Mode & Info Bar | Low | Medium — polish, community engagement |
| 🟡 Medium | Dynamic Bezel (Netplay) | Low | Medium — polish and immersion |
| 🟡 Medium | In-Match Chat | Medium | Medium — social connectivity |
| 🟡 Medium | Mod Menu via Game Menus | Medium | Medium — discoverability |
| 🟢 Low | Tournaments | High | High — marquee feature but complex |
| 🟢 Low | Ranked / Rating System | High | High — requires server infrastructure |
| 🟢 Low | Spectator Mode | High | Medium — complex netcode extension |

---

*Last updated: March 23, 2026*
