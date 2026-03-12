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

## Phase 2: Match Reporting ✦ **CURRENT**

**Goal**: Foundation for stats tracking — both clients submit match results to the server.

**Status**: 🔧 In Progress

### Oracle constraints
- **DB**: SQLite via `better-sqlite3` (embedded, zero RAM overhead, ~200 bytes/match)
- **Bandwidth**: One POST per match per player (~500 bytes) — negligible
- **Storage**: 10K matches/day = 2 MB/day. Won't matter for years.

### Key design points
- Both players submit `POST /match_result` with player IDs, winner, characters, round count
- Server cross-validates: only records if both players agree on the winner
- Pending results expire after 60 seconds (handles disconnects)
- Match data stored in SQLite `matches` table
- Client API: `LobbyServer_ReportMatch()` — called when netplay session exits
- No game engine hooks yet — match reporting is trigger-ready, will wire to game state later

### Server endpoints
| Endpoint | Method | Purpose |
|----------|--------|---------|
| `POST /match_result` | POST | Submit match result for cross-validation |
| `GET /player/:id/stats` | GET | Get win/loss/match count for a player |

### Database schema
```sql
-- Players table (created on first match result)
CREATE TABLE IF NOT EXISTS players (
    player_id TEXT PRIMARY KEY,
    display_name TEXT,
    wins INTEGER DEFAULT 0,
    losses INTEGER DEFAULT 0,
    disconnects INTEGER DEFAULT 0,
    rating REAL DEFAULT 1500.0,    -- Glicko-2 (Phase 4)
    rd REAL DEFAULT 350.0,         -- Rating Deviation
    volatility REAL DEFAULT 0.06,  -- Glicko-2 volatility
    last_match TEXT,               -- ISO 8601 timestamp
    created_at TEXT DEFAULT (datetime('now'))
);

-- Match history
CREATE TABLE IF NOT EXISTS matches (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    p1_id TEXT NOT NULL,
    p2_id TEXT NOT NULL,
    winner_id TEXT NOT NULL,
    p1_char INTEGER,
    p2_char INTEGER,
    rounds INTEGER,
    duration_seconds INTEGER,
    created_at TEXT DEFAULT (datetime('now'))
);
CREATE INDEX idx_matches_p1 ON matches(p1_id);
CREATE INDEX idx_matches_p2 ON matches(p2_id);

-- Pending results (for cross-validation, ephemeral)
CREATE TABLE IF NOT EXISTS pending_results (
    match_key TEXT PRIMARY KEY,     -- sorted player IDs: "id1:id2"
    reporter_id TEXT NOT NULL,
    winner_id TEXT NOT NULL,
    p1_char INTEGER,
    p2_char INTEGER,
    rounds INTEGER,
    created_at TEXT DEFAULT (datetime('now'))
);
```

### Files involved
| File | Action |
|------|--------|
| `tools/lobby-server/lobby-server.js` | MODIFY — add SQLite, `/match_result`, `/player/:id/stats` |
| `tools/lobby-server/package.json` | NEW — add `better-sqlite3` dependency |
| `src/netplay/lobby_server.c` | ADD — `LobbyServer_ReportMatch()` |
| `src/netplay/lobby_server.h` | ADD — `MatchResult` struct + report function |

---

## Phase 3: Leaderboards

**Goal**: Persistent player stats visible in-game.

**Status**: ⏳ Planned

### Oracle constraints
- Paginated queries only (`LIMIT 20 OFFSET ?`) — never return full table
- Client fetches on-demand (leaderboard screen open), not on a timer
- **Index**: `CREATE INDEX idx_rating ON players(rating DESC)` for fast sorted queries

### Key design points
- Top 100 leaderboard + individual player lookup
- Filter by region, character, time period
- In-game RmlUI screen (accessible from lobby or main menu)

### Server endpoints
- `GET /leaderboard?page=1&limit=20` — paginated by rating
- `GET /player/:id/stats` — individual stats (already added in Phase 2)

---

## Phase 4: Glicko-2 Rankings

**Goal**: Skill-based rating system that handles uncertainty and inactivity.

**Status**: ⏳ Planned

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

---

## Phase 5: Skill-Based Matchmaking (SBMM)

**Goal**: Automatically pair players of similar skill level with good connections.

**Status**: ⏳ Planned

### Oracle constraints
- Only iterate searching players (not all registered players)
- Run matching reactively (on `/searching` POST), not on a timer
- With <100 concurrent searchers, O(n²) matching is fine

### Key design points
- Server-side matching score:
  ```
  score = 100
    - rating_diff * 0.5      (prefer similar skill)
    - ping_penalty * 2       (prefer low ping)
    + wait_bonus * 0.3       (relax criteria over time)
  ```
- Rating window starts narrow (±100), expands every 10s of waiting
- Region preference: same region first, cross-region after 30s
- Hooks into existing `resolveConnectMatch()` in `lobby-server.js`

---

## Dependencies

```
Phase 1 (Identity) ✅
  └── Phase 2 (Match Reporting) ← CURRENT
        ├── Phase 4 (Glicko-2 Rankings)
        │     └── Phase 5 (SBMM)
        └── Phase 3 (Leaderboards)
```

## Server Infrastructure

| Component | Choice | Why |
|-----------|--------|-----|
| **Runtime** | Node.js (single process) | ~80 MB RAM, already deployed |
| **Database** | SQLite via `better-sqlite3` | Zero-config, embedded, no extra process |
| **Auth** | HMAC-SHA256 (existing) | Works, no change needed |
| **Bandwidth** | ETag/304 for polling (future) | Reduces 2KB → 50 bytes when lobby unchanged |

### Resource budget (Oracle Always Free)
| Metric | Budget | Phase 2 impact |
|--------|--------|----------------|
| RAM | 1 GB | +10 MB (SQLite) |
| Storage | 200 GB | +2 MB/day at 10K matches |
| Bandwidth | 10 TB/month | +negligible (one POST per match) |
| CPU | 1/8 OCPU | +negligible (INSERT + SELECT) |
