# 3SX Netplay Feature Roadmap

This document tracks the planned online features for 3SX netplay.
Each phase builds on the previous — implement in order.

**Server**: Oracle Cloud VPS "Always Free" tier (1 GB RAM AMD or 4-core ARM Ampere, 10 TB/month bandwidth).
Design choices are constrained to fit within these limits.

---

## Phase 1: Player Identity ✅ **DONE**

**Goal**: Every player gets a stable, auto-generated cryptographic identity that persists across sessions.

### What was built
- Auto-generate SHA-256 identity key on first launch via `SDL_rand_bits()`
- Store `identity-public-key` and `identity-secret-key` (hex) in `config.ini`
- Derive `player_id` from first 16 hex chars (stable, unique)
- Auto-generate `Player-XXXX` display name if none set
- Extracted SHA-256 into shared `sha256.c/h` (was inline in `lobby_server.c`)
- New `identity.c/h` module with init/shutdown lifecycle
- 6 unit tests in `test_identity.c`

### Files
| File | Action |
|------|--------|
| `src/netplay/sha256.c/h` | NEW — shared SHA-256 + HMAC |
| `src/netplay/identity.c/h` | NEW — identity module |
| `src/port/config/config.h` | ADD — `CFG_KEY_IDENTITY_PUBLIC`, `CFG_KEY_IDENTITY_SECRET` |
| `src/port/sdl/netplay/sdl_netplay_ui.cpp` | MODIFY — use `Identity_GetPlayerId()` |
| `src/port/sdl/app/sdl_app.c` | MODIFY — call `Identity_Init()` |
| `src/netplay/lobby_server.c` | MODIFY — use shared `sha256.h` |
| `tests/unit/test_identity.c` | NEW — 6 unit tests |

---

## Phase 2: Match Reporting ✅ **DONE**

**Goal**: Foundation for stats tracking — both clients submit match results to the server.

**Status**: ✅ Deployed (2026-03-12)

### What was built
- SQLite via `better-sqlite3` (WAL mode, graceful shutdown)
- `POST /match_result` — cross-validation (both players must agree on winner)
- `GET /player/:id/stats` — win/loss/rating query
- Pending results auto-expire after 60 seconds
- `LobbyServer_ReportMatch()` C client with HMAC auth
- `LobbyServer_GetPlayerStats()` C client
- Game-end hook: detects `RUNNING→EXITING` transition, reads `Winner_id`/`My_char`/`PL_Wins`
- Async match report via SDL thread (non-blocking)

### Oracle impact
- SQLite adds ~10 MB RAM, ~200 bytes/match storage, negligible bandwidth (one POST per match per player)

### Files
| File | Action |
|------|--------|
| `tools/lobby-server/lobby-server.js` | MODIFY — SQLite, `/match_result`, `/player/:id/stats` |
| `tools/lobby-server/package.json` | MODIFY — added `better-sqlite3` |
| `tools/lobby-server/deploy.sh` | MODIFY — `build-essential` check for native bindings |
| `src/netplay/lobby_server.h` | ADD — `MatchResult`, `PlayerStats` structs |
| `src/netplay/lobby_server.c` | ADD — `ReportMatch()`, `GetPlayerStats()` |
| `src/port/sdl/netplay/sdl_netplay_ui.cpp` | ADD — game-end hook + `AsyncReportMatch()` |

### Audit fixes (2026-03-12)
- **Crash bug**: `LobbyServer_GetPlayerStats()` passed `NULL` body to `http_request()` → `strlen(NULL)` segfault. Fixed: `""` instead of `NULL`.
- **Cosmetic**: `int my_player = Netplay_IsEnabled() ? 0 : 0` → `int my_player = 0`

---

## Phase 3: Leaderboards ✅ **DONE**

**Goal**: Persistent player stats visible in-game.

**Status**: ✅ Deployed (2026-03-12)

### What was built
- `GET /leaderboard?page=0&limit=20` — paginated, sorted by wins (server)
- `LobbyServer_GetLeaderboard()` — C client with JSON array parser
- `rmlui_leaderboard.cpp/h` — RmlUI data model (async SDL thread fetch, `LBItem` data-for)
- `leaderboard.rml` + `leaderboard.rcss` — table layout with pagination, dark/gold aesthetic
- Auto-fetches page 0 on show, supports prev/next navigation

### Files
| File | Action |
|------|--------|
| `tools/lobby-server/lobby-server.js` | MODIFY — `GET /leaderboard` |
| `src/netplay/lobby_server.h` | ADD — `LeaderboardEntry` struct |
| `src/netplay/lobby_server.c` | ADD — `LobbyServer_GetLeaderboard()` |
| `src/port/sdl/rmlui/rmlui_leaderboard.cpp` | NEW — data model + async fetch |
| `src/port/sdl/rmlui/rmlui_leaderboard.h` | NEW — lifecycle API |
| `assets/ui/leaderboard.rml` | NEW — UI template |
| `assets/ui/leaderboard.rcss` | NEW — stylesheet |
| `src/port/sdl/app/sdl_app.c` | MODIFY — `rmlui_leaderboard_init/update` lifecycle |
| `CMakeLists.txt` | MODIFY — added `rmlui_leaderboard.cpp` to sources |

### Audit fixes (2026-03-12)
- **Pagination broken**: `s_total` was never set. Fixed: added `int* out_total` param to `LobbyServer_GetLeaderboard()`, parsed `"total"` from JSON response.
- **Player ID match truncated**: `strncmp(16)` on 64-char SHA-256 hex IDs → false positives. Fixed: `strcmp()`.

### Oracle impact
- `GET /leaderboard` adds one `SELECT COUNT(*)` + one `SELECT ... LIMIT/OFFSET` per page view — negligible
- Client-side async fetch, no polling — zero idle bandwidth

---

## Phase 4: Glicko-2 Rankings ✅ **DONE**

**Goal**: Skill-based rating system that handles uncertainty and inactivity.

**Status**: ✅ Deployed (2026-03-12)

### Oracle constraints
- Glicko-2 calculation is pure math (~1ms per match) — negligible CPU
- 3 floats per player (rating, RD, volatility) — already in `players` table schema
- **Lazy RD decay**: Recalculate RD on next match instead of iterating all players daily

### Key design points
- **Glicko-2** (not Elo) — better for fighting games:
  - Rating Deviation (RD) increases with inactivity
  - Volatility parameter handles inconsistent players
  - More accurate with fewer games played
- Rating starts at 1500 (RD=350, vol=0.06)
- Calculated server-side after each verified match result
- Tiers: Bronze (<1200), Silver (1200-1500), Gold (1500-1800), Platinum (1800-2100), Diamond (2100+)
- Seasonal soft resets (compress toward 1500, inflate RD)

### What was built
- Server-side Glicko-2 math engine (Illinois algorithm)
- Rating calculations applied automatically after match validations
- Rank tiers mapped to server endpoints and client JSON parsers
- RmlUI leaderboard rows colorized via tier data attributes (`.bronze`, `.silver`, etc.)

### Files
| File | Action |
|------|--------|
| `tools/lobby-server/lobby-server.js` | MODIFY — `glicko2Update()`, tier calculation, `GET /leaderboard` ordering |
| `src/netplay/lobby_server.h` | MODIFY — added `tier` to `PlayerStats` and `LeaderboardEntry` |
| `src/netplay/lobby_server.c` | MODIFY — parse `tier` in `GetPlayerStats` and `GetLeaderboard` |
| `src/port/sdl/rmlui/rmlui_leaderboard.cpp` | MODIFY — added `tier` binding to `LBItem` |
| `assets/ui/leaderboard.rml` | MODIFY — `data-class-bronze/silver...` bindings |
| `assets/ui/leaderboard.rcss` | MODIFY — tier-specific coloring |

---

## Phase 5: Casual Lobbies (8-Player Rooms) ✦ **NEXT**

**Goal**: Provide early adopters with a place to congregate and visually prove the game isn't dead when population is low.

**Status**: 🟡 In Progress (server + C client + SSE + RmlUI done, gameplay logic remaining)

### What was built
- Server routes: `POST /room/create`, `/room/join`, `/room/leave`, `/room/chat`, `/room/queue/join`, `/room/queue/leave`
- Server read-only state: `GET /room/state?room_code=` — returns room JSON without side effects
- SSE event stream: `GET /room/events?room_code=` — real-time push via `broadcastRoomEvent()`
- Room code generation (4-char unambiguous charset), 8-player max, host migration on leave
- Chat capped at 50 messages per room
- C client API: `LobbyServer_CreateRoom/JoinRoom/LeaveRoom/JoinQueue/LeaveQueue/SendChat/GetRoomState`
- SSE streaming client: `LobbyServer_SSEConnect/SSEDisconnect/SSEPoll/SSEIsConnected` (background thread)
- `RoomState`, `RoomPlayer`, `ChatMessage`, `SSEEvent` structs + `parse_room_json()` parser
- RmlUI room screen: `rmlui_casual_lobby.cpp/h` — data model with player list, queue, chat, gamepad navigation
- RmlUI templates: `casual_lobby.rml`, `casual_lobby.rcss`, `casual_lobby_chat.rml`
- Lifecycle: init/update in `sdl_app.c`, shutdown calls for data model cleanup
- Test scripts: `__test_room.js`, `__test_sse.js`

### Remaining work
- "Winner Stays On" cabinet rotation logic
- `GekkoSpectateSession` integration for live spectating

### Key design points
- **Casual Lobbies (8-Player Rooms)**: Players can create/join public or private named rooms (e.g., "EU Casuals", "Beginners").
- **Virtual Cabinet & Universal Spectator**: Up to 8 players join, text chat, and queue up for a "Winner Stays On" rotation. *Anyone* not currently playing can spectate the match live via `GekkoSpectateSession`.
- **Server Mechanics**: Server maintains room state and broadcasts changes via Server-Sent Events (SSE).

### Files
| File | Action |
|------|--------|
| `tools/lobby-server/lobby-server.js` | MODIFY — room routes, SSE, `GET /room/state`, `broadcastRoomEvent()` |
| `src/netplay/lobby_server.h` | ADD — `RoomState`, `RoomPlayer`, `ChatMessage`, `SSEEvent` structs, SSE API |
| `src/netplay/lobby_server.c` | ADD — room API, `GetRoomState`, SSE client (background thread) |
| `src/port/sdl/rmlui/rmlui_casual_lobby.cpp` | NEW — RmlUI data model + SSE-driven updates |
| `src/include/port/sdl/rmlui/rmlui_casual_lobby.h` | NEW — lifecycle API |
| `assets/ui/casual_lobby.rml` | NEW — room UI template |
| `assets/ui/casual_lobby.rcss` | NEW — room stylesheet |
| `assets/ui/casual_lobby_chat.rml` | NEW — chat popup |
| `src/port/sdl/app/sdl_app.c` | MODIFY — init/update/shutdown lifecycle |
| `tools/lobby-server/__test_room.js` | NEW — room smoke test |
| `tools/lobby-server/__test_sse.js` | NEW — SSE streaming test |

---

## Phase 6: Match Accept/Decline & Connection Filters

**Goal**: Give players control over who they connect to based on connection quality.

**Status**: ⏳ Planned

### Key design points
- Show a 10-second "Match Found!" RmlUI popup before establishing P2P.
- Display opponent's ping and **Wired vs. Wi-Fi status** (already tracked via Presence API).
- Both players must accept; prevents being trapped in high-latency cross-continent rollback.
- Zero extra server impact (client-side UI and P2P pinging).

---

## Phase 7: Detailed Player Profiles & Character Usage

**Goal**: Expand statistics to track character-specific performance and streaks.

**Status**: ⏳ Planned

### Key design points
- Expand `players_db` and matches to track character usage.
- Clicking a name on the Leaderboard opens a profile popup:
  - Main Character (e.g., "Ryu - 65% usage")
  - Longest Win Streak
  - Win rate per stage/character

---

## Phase 8: Skill-Based Matchmaking (SBMM)

**Goal**: Automatically pair players of similar skill level. Delayed until concurring playerbase can support queue segregation.

**Status**: ⏳ Planned

### Key design points
- Server-side matching score: `100 - (rating_diff*0.5) - (ping_penalty*2) + (wait_bonus*0.3)`
- Rating window starts narrow (±100), expands every 10s of waiting.



---

## Phase 9: Automated Desync & Telemetry Reporting

**Goal**: Robust telemetry for netcode debugging without manual user intervention.

**Status**: ⏳ Planned

### Key design points
- When GekkoNet detects a rollback desync (`dump_desync_state()`), automatically compress and `POST` the desync log to the server.
- Provides a global dashboard to identify the exact frame and variables causing state divergence.
- Low server storage impact since desyncs are infrequent.

---

## Phase 10: "Rival" System (Head-to-head tracking)

**Goal**: Personalize matchmaking by tracking history against specific opponents.

**Status**: ⏳ Planned

### Key design points
- Server tracks W/L history between specific player pairs.
- UI displays Head-to-Head record before match starts (e.g., "You are 3-1 against MasterGouki").

---

## Dependencies

```
Phase 1 (Identity) ✅
  └── Phase 2 (Match Reporting) ✅
        ├── Phase 4 (Glicko-2 Rankings) ✅
        │     └── Phase 8 (SBMM)
        ├── Phase 3 (Leaderboards) ✅
        │     └── Phase 7 (Detailed Profiles)
        └── Phase 5 (Casual Lobbies) 🟡 IN PROGRESS
              └── Phase 6 (Accept/Decline Filters)
        └── Phase 9 (Automated Desync Reporting)
        └── Phase 10 (Rival System)
```

## Server Infrastructure

| Component | Choice | Why |
|-----------|--------|-----|
| **Runtime** | Node.js (single process) | ~80 MB RAM, already deployed |
| **Database** | SQLite via `better-sqlite3` | Zero-config, embedded, no extra process |
| **Auth** | HMAC-SHA256 (existing) | Works, no change needed |
| **Bandwidth** | ETag/304 for polling (future) | Reduces 2KB → 50 bytes when lobby unchanged |

### Resource budget (Oracle Always Free)
| Metric | Budget | Phase 2 impact | Phase 3 impact |
|--------|--------|----------------|----------------|
| RAM | 1 GB | +10 MB (SQLite) | +0 (same SQLite) |
| Storage | 200 GB | +2 MB/day at 10K matches | +0 (reads only) |
| Bandwidth | 10 TB/month | +negligible (one POST per match) | +negligible (on-demand GET) |
| CPU | 1/8 OCPU | +negligible (INSERT + SELECT) | +negligible (SELECT + COUNT) |
