# 3SX Netplay — Comprehensive Reference

> Full documentation of all netplay modes, features, and systems (client & server).

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Networking Layers](#networking-layers)
   - [GekkoNet Rollback](#gekkonet-rollback)
   - [STUN / NAT Traversal](#stun--nat-traversal)
   - [UPnP Port Forwarding](#upnp-port-forwarding)
   - [UDP Hole Punching](#udp-hole-punching)
3. [Netplay Modes](#netplay-modes)
   - [LAN / Local Network](#lan--local-network)
   - [Internet / Lobby Server](#internet--lobby-server)
   - [Casual Lobbies (8-Player Rooms)](#casual-lobbies-8-player-rooms)
   - [Spectator Mode](#spectator-mode)
4. [Client-Side Systems](#client-side-systems)
   - [Session State Machine](#session-state-machine)
   - [Discovery (LAN Beacons)](#discovery-lan-beacons)
   - [Lobby Server Client](#lobby-server-client)
   - [SSE Streaming Client](#sse-streaming-client)
   - [Identity System](#identity-system)
   - [Connection Type Detection](#connection-type-detection)
   - [Ping Probes](#ping-probes)
   - [Game State / Rollback](#game-state--rollback)
   - [Dynamic Input Delay](#dynamic-input-delay)
   - [FT (First-To) Negotiation](#ft-first-to-negotiation)
   - [Network Stats HUD](#network-stats-hud)
5. [Server-Side Systems](#server-side-systems)
   - [Lobby Server Overview](#lobby-server-overview)
   - [HMAC Request Signing](#hmac-request-signing)
   - [Presence & Matchmaking](#presence--matchmaking)
   - [Room Management](#room-management)
   - [Match Proposal Flow (Phase 6)](#match-proposal-flow-phase-6)
   - [Match Reporting & Cross-Validation](#match-reporting--cross-validation)
   - [Anti-Ragequit / Disconnect Detection](#anti-ragequit--disconnect-detection)
   - [Replay Upload](#replay-upload)
   - [Glicko-2 Rating System](#glicko-2-rating-system)
   - [Leaderboards](#leaderboards)
   - [GeoIP Region Detection](#geoip-region-detection)
   - [Permanent Region Rooms](#permanent-region-rooms)
   - [Chat & Profanity Filter](#chat--profanity-filter)
   - [Database Schema (SQLite)](#database-schema-sqlite)
6. [Server API Reference](#server-api-reference)
7. [Configuration](#configuration)
8. [Source File Map](#source-file-map)

---

## Architecture Overview

3SX netplay is a **peer-to-peer (P2P) rollback netcode** system built on top of [GekkoNet](https://github.com/nicholaspknight/GekkoNet). The lobby server only handles matchmaking, presence, and stats — actual gameplay traffic flows directly between peers via UDP.

```
┌────────────┐       UDP (GekkoNet)        ┌────────────┐
│  Player 1  │◄──────────────────────────►  │  Player 2  │
│  (3SX)     │     rollback / inputs        │  (3SX)     │
└─────┬──────┘                              └──────┬─────┘
      │  HTTP + HMAC-SHA256                        │
      │  (presence, matches, stats)                │
      └──────────────────┬─────────────────────────┘
                         ▼
              ┌─────────────────────┐
              │   Lobby Server      │
              │   (Node.js + SQLite)│
              └─────────────────────┘
```

**Key design principles:**
- **Zero gameplay traffic through the server** — P2P only
- **Deterministic simulation** — both peers run identical game logic; rollback corrects mispredictions
- **Cross-platform** — Windows (Winsock2/BCrypt), Linux (POSIX sockets/embedded SHA-256), Raspberry Pi
- **Minimal dependencies** — lobby server uses only Node.js built-ins + optional npm packages

---

## Networking Layers

### GekkoNet Rollback

The core netcode library. GekkoNet provides:

| Feature | Detail |
|---------|--------|
| **Session types** | `GekkoGameSession` (player), `GekkoSpectateSession` (spectator) |
| **Input size** | 2 bytes (`u16`) per player per frame |
| **State size** | `sizeof(State)` — the full deterministic game state snapshot |
| **Max spectators** | 4 (game sessions), 1 (spectate sessions) |
| **Input prediction window** | 12 frames |
| **Desync detection** | Enabled for game sessions (checksum comparison) |
| **Spectator delay** | 15 frames (~250ms at 60fps) |

**Event types** processed by the client:

| GekkoNet Event | Client Action |
|----------------|---------------|
| `GekkoAdvanceEvent` | Run one simulation tick with confirmed inputs |
| `GekkoLoadEvent` | Restore game state (rollback) |
| `GekkoSaveEvent` | Snapshot game state |
| `GekkoPlayerSyncing` | Push `NETPLAY_EVENT_SYNCHRONIZING` |
| `GekkoPlayerConnected` | Push `NETPLAY_EVENT_CONNECTED` |
| `GekkoPlayerDisconnected` | Push `NETPLAY_EVENT_DISCONNECTED`, trigger exit |
| `GekkoDesyncDetected` | Show warning, terminate session |

**Source:** `src/netplay/netplay.c`

### STUN / NAT Traversal

Uses Google's public STUN server (`stun.l.google.com:19302`) implementing RFC 5389 Binding Requests.

**`StunResult` contains:**
- `public_ip` / `public_port` — the peer's NAT-mapped external endpoint
- `local_port` — the OS-bound local port
- `socket_fd` — kept open for hole punching (preserves the NAT pinhole)

**Room code encoding:** The 4-byte IP + 2-byte port are encoded into an 8-character alphanumeric room code via `Stun_EncodeEndpoint()` for easy sharing.

**Source:** `src/netplay/stun.c`, `src/netplay/stun.h`

### UPnP Port Forwarding

Optional automatic port forwarding via UPnP-IGD for routers that support it.

| Function | Purpose |
|----------|---------|
| `Upnp_AddMapping()` | Create UDP port mapping |
| `Upnp_RemoveMapping()` | Remove mapping on shutdown |
| `Upnp_GetExternalIP()` | Get public IP without STUN |

**Source:** `src/netplay/upnp.c`, `src/netplay/upnp.h`

### UDP Hole Punching

`Stun_HolePunch()` sends bidirectional UDP packets through both peers' NATs to open symmetric pinholes. Blocks for a configurable `punch_duration_ms` with a cancel flag. After punching succeeds, the socket is set to non-blocking mode and handed to GekkoNet via the custom STUN adapter.

**Custom STUN adapter:** When `stun_socket_fd >= 0`, GekkoNet uses a custom network adapter (`stun_adapter`) that calls `Stun_SocketSendTo()` / `Stun_SocketRecvFrom()` on the pre-punched socket, bypassing the default ASIO adapter (which would create a new socket and lose the NAT pinhole).

---

## Netplay Modes

### LAN / Local Network

**How it works:** UDP broadcast beacons on port 7999, sent every 500ms across all network interfaces.

**Beacon format:**
```
3SX_LOBBY|{instance_id}|{auto_connect}|{ready}|{challenge_target}|{port}|{display_name}|{ft_value}
```

**Connection flow:**
1. Both players enter the LAN lobby (`Netplay_EnterLobby()`)
2. Discovery beacons are broadcast on all NICs (directed subnet broadcasts, not just 255.255.255.255)
3. Players see each other in the peer list
4. Connection is established via:
   - **Auto-connect:** Both have auto-connect enabled → lowest instance ID becomes P1
   - **Challenge:** Player A challenges Player B → B accepts (or has auto-connect on)
   - **Mutual challenge:** Both challenge each other → instance ID tiebreaker picks P1
5. A 1-second handshake hold ensures both peers are ready
6. `Netplay_Begin()` is called → game enters `TRANSITIONING` state

**Key features:**
- Per-interface directed broadcast (works across WiFi + Ethernet adapters)
- Stale peer cleanup after 15 seconds of no beacons
- Max 16 discovered peers
- Display names from Identity module
- FT value transmitted in beacons for pre-match visibility

**Source:** `src/netplay/discovery.c`, `src/netplay/discovery.h`

### Internet / Lobby Server

**How it works:** Players register presence on the HTTP lobby server, discover opponents, and establish P2P connections via STUN hole punching.

**Connection flow:**
1. Client calls `LobbyServer_UpdatePresence()` with player info and STUN room code
2. Client calls `LobbyServer_StartSearching()` to enter the matchmaking pool
3. Client polls `LobbyServer_GetSearching()` to find opponents
4. When a match is found, both peers use each other's STUN room codes to decode IP:port
5. `Stun_HolePunch()` opens the NAT pinhole
6. GekkoNet session starts on the punched socket

**Presence data sent to server:**
- `player_id`, `display_name`, `region`, `room_code` (STUN), `connect_to` (target)
- `rtt_ms` (RTT to lobby server), `connection_type` (wifi/wired/unknown), `ft` (first-to value)

### Casual Lobbies (8-Player Rooms)

**Phase 5 feature** — up to 8 players in a shared room with chat, queue, and automated match rotation.

**Room lifecycle:**
1. A player creates a room (`LobbyServer_CreateRoom()`) or joins a permanent region room
2. Client connects SSE for real-time updates (`LobbyServer_SSEConnect()`)
3. Players join the match queue (`LobbyServer_JoinQueue()`)
4. Server auto-proposes matches from the queue (Phase 6 flow)
5. Both players accept → P2P connection via STUN → GekkoNet match
6. Match ends → winner stays on, loser goes to back of queue → next match proposed

**Room structure (`RoomState`):**
- Up to `MAX_ROOM_PLAYERS` (8) players
- Match queue (FIFO)
- Active match (`match_p1`, `match_p2`, `match_active`)
- Chat history (up to `MAX_CHAT_MESSAGES` = 50)
- Per-room FT setting

**SSE events received by client:**

| Event | Description |
|-------|-------------|
| `sync` | Full room state (on connect) |
| `join` | Player joined the room |
| `leave` | Player left the room |
| `chat` | New chat message |
| `queue_update` | Queue order changed |
| `host_migrated` | Room host changed |
| `match_propose` | Match proposed (Phase 6) — both players must accept |
| `match_start` | Both accepted — match begins |
| `match_decline` | One player declined or proposal timed out |
| `match_end` | Match concluded — winner/loser reported |

**Source:** `src/netplay/lobby_server.c` (SSE client at line 1208+), `src/netplay/lobby_server.h`

### Spectator Mode

Read-only observation of an active match. The spectator connects to the match host and receives game events on a 15-frame delay.

| Function | Purpose |
|----------|---------|
| `Netplay_BeginSpectate(host_ip, host_port)` | Start spectating — creates `GekkoSpectateSession` |
| `Netplay_StopSpectate()` | Cleanly disconnect and return to idle |

**Spectator details:**
- Uses `GekkoSpectator` actor type
- Always renders (no catch-up frame skipping)
- No local input injection
- No desync detection
- Processes Load, Advance, and Save events
- Auto-disconnects on host disconnect

**Session state:** `NETPLAY_SESSION_SPECTATING`

**Source:** `src/netplay/netplay.c` (lines 1153–1201)

---

## Client-Side Systems

### Session State Machine

```
IDLE ──► LOBBY ──► TRANSITIONING ──► CONNECTING ──► RUNNING ──► EXITING ──► IDLE
                                                                     │
                                                                     ├──► LOBBY (if in casual room)
                                                                     │
IDLE ──► SPECTATING ──► IDLE
```

| State | Description |
|-------|-------------|
| `NETPLAY_SESSION_IDLE` | No active session |
| `NETPLAY_SESSION_LOBBY` | In LAN discovery or waiting for match |
| `NETPLAY_SESSION_TRANSITIONING` | Game initializing into character select (pre-GekkoNet) |
| `NETPLAY_SESSION_CONNECTING` | GekkoNet session created, waiting for peer sync |
| `NETPLAY_SESSION_RUNNING` | Active match with rollback |
| `NETPLAY_SESSION_EXITING` | Cleaning up session, destroying GekkoNet |
| `NETPLAY_SESSION_SPECTATING` | Observing a match read-only |

**On exit from a casual room match:** The state returns to `LOBBY` (not `IDLE`) so the player stays in the room. The casual lobby RmlUI overlay is re-shown.

### Discovery (LAN Beacons)

- **Port:** 7999 (UDP broadcast)
- **Interval:** 500ms
- **Max peers:** 16
- **Stale timeout:** 15 seconds
- **Platform:** Windows (`GetAdaptersAddresses` for per-NIC broadcast), Linux (`getifaddrs`)
- **Beacon contains:** instance ID, auto-connect flag, ready flag, challenge target, port, display name, FT value
- Suppresses auto-search/announce when already in a casual room

### Lobby Server Client

Zero-dependency HTTP/1.1 client using raw sockets (no libcurl). All requests are HMAC-SHA256 signed.

**Initialization:** Reads URL + key from `config.ini` → falls back to baked-in defaults.

**Platform-specific HMAC:**
- **Windows:** BCrypt (`BCryptCreateHash` + `BCRYPT_ALG_HANDLE_HMAC_FLAG`)
- **Linux/macOS:** Embedded portable SHA-256 (public domain)

### SSE Streaming Client

Real-time event stream for casual room state changes.

- Spawns a background thread (`SDL_CreateThread`) per SSE connection
- Lock-free ring buffer (`SSE_RING_SIZE` = 16) for main thread polling
- Atomic read/write indices — no mutex needed
- 1-second recv timeout for responsive stop-flag checking
- Force-closes socket on disconnect to unblock `recv()` immediately
- **Auto-reconnection:** On unexpected disconnect, `SSEPoll()` detects the dead thread and spawns a new connection with exponential backoff (2s, 4s, 8s, 16s, 30s max, up to 5 retries). Intentional disconnects via `SSEDisconnect()` clear the room code to suppress reconnection. Reconnect counter resets on successful event read.
- **Scoped JSON parsing:** `json_get_string_in()` / `json_get_int_in()` restrict key searches to bounded substrings, preventing cross-object key collision in nested JSON (e.g., `p1`/`p2` objects in `match_propose`).

### Identity System

Auto-generates a persistent, unique player identity on first launch.

- **Key generation:** Random bytes → SHA-256 hash
- **Player ID:** 16 hex characters (derived from identity key)
- **Display name:** Auto-generated `Player-XXXX` if not set by user
- **Storage:** Persisted in `config.ini`
- **Public key:** 64 hex characters (available for future crypto features)

**Source:** `src/netplay/identity.c`, `src/netplay/identity.h`

### Connection Type Detection

Detects WiFi vs Wired via OS APIs:
- **Windows:** `GetAdaptersAddresses()` + `IfType == IF_TYPE_IEEE80211`
- **Linux:** `ioctl(SIOCGIWNAME)` on active interfaces

Returns one of: `"wifi"`, `"wired"`, `"unknown"`

**Source:** `src/netplay/net_detect.c`, `src/netplay/net_detect.h`

### Ping Probes

Direct peer-to-peer ping measurement using the STUN socket.

| Function | Purpose |
|----------|---------|
| `PingProbe_Init(socket_fd)` | Start probing on an existing UDP socket |
| `PingProbe_AddPeer(ip, port, id)` | Add a peer to probe |
| `PingProbe_Update()` | Send/receive pings (call from background thread) |
| `PingProbe_GetRTT(id)` | Get smoothed RTT in ms (-1 if unknown) |
| `PingProbe_IsReachable(id)` | True if pongs received (timeout after 5 missed) |

**Source:** `src/netplay/ping_probe.c`, `src/netplay/ping_probe.h`

### Game State / Rollback

Full deterministic state snapshot for save/load during rollback.

- **~700+ global variables** saved/loaded per frame via `GS_SAVE` / `GS_LOAD` macros
- Organized by source module: rendering, input, timers, player state, backgrounds, effects, etc.
- Any new global that affects the simulation **must** be added to both `GameState_Save()` and `GameState_Load()`
- **Compile-time guard:** `_Static_assert(sizeof(GameState) == 19376)` — fires if a field is added/removed from `GameState` without updating save/load functions
- **Desync debugging:** Frame-level state dumps with field-by-field offset logging

**Initial sync (`setup_vs_mode`):** Canonicalizes all divergent game globals before the first synced frame — task timers, RNG indices, button config, BG state, pause flags, combat settings, and more.

**Source:** `src/netplay/game_state.c` (~1808 lines), `src/netplay/game_state.h`

### Dynamic Input Delay

Delay frames are computed from ping measurements collected during character select and applied once battle starts.

| Effective RTT (ping + jitter) | Delay Frames |
|-------------------------------|-------------|
| < 30ms | 0 |
| 30–70ms | 1 |
| 70–130ms | 2 |
| 130–200ms | 3 |
| ≥ 200ms | 4 (max) |

Sampling: 30-frame intervals during `CONNECTING` → applied once on `G_No[1] == 2` (battle start).

### FT (First-To) Negotiation

The **challenger dictates** the FT value. The receiver sees it before accepting.

| Path | How FT is conveyed |
|------|-------------------|
| **LAN** | 8th field in discovery beacon (`ft_value` in `NetplayDiscoveredPeer`) |
| **Internet** | `ft` field in `LobbyPlayer` presence data |
| **Casual Room** | Per-room `ft` field set at creation; included in `match_propose` SSE events |

`Netplay_SetNegotiatedFT()` sets the value before `Netplay_Begin()`. Falls back to `config.ini` setting if 0.

**Clamped range:** 1–10. Values < 1 default to FT2.

### Network Stats HUD

Updated every 60 frames:

| Stat | Source |
|------|--------|
| `ping` | `GekkoNetworkStats.avg_ping` |
| `delay` | Dynamic delay value |
| `rollback` | Max rollback depth (smoothly decreases by 1 per period if improving) |

---

## Server-Side Systems

### Lobby Server Overview

**Runtime:** Node.js (zero framework — raw `node:http` + `node:crypto`)  
**Database:** SQLite via `better-sqlite3` (optional; match reporting disabled without it)  
**Location:** `tools/lobby-server/lobby-server.js`

**Environment variables:**
| Variable | Default | Description |
|----------|---------|-------------|
| `LOBBY_SECRET` | *(required)* | HMAC shared key |
| `LOBBY_PORT` | `3000` | HTTP listen port |

**Cleanup timer:** Every 5 seconds — evicts stale players (>10s), expires cooldowns, auto-records stale pending match results (transaction-wrapped for atomicity), times out proposed matches (>30s).

**HTTP buffer size:** 16 KB (`HTTP_BUF_SIZE = 16384`) — large enough for leaderboard pages and full room state payloads.

### HMAC Request Signing

Every request must include:
- `X-Timestamp` — Unix epoch seconds
- `X-Signature` — `HMAC-SHA256(secret, timestamp + method + path + body)`

Requests with >60s clock drift or bad signatures are rejected with HTTP 403.

### Presence & Matchmaking

**Endpoints:**
- `POST /presence` — Register/update player presence
- `POST /searching/start` — Enter matchmaking pool
- `POST /searching/stop` — Leave matchmaking pool
- `GET /searching[?region=XX]` — List searching players (optional region filter)
- `POST /leave` — Remove player entirely
- `POST /decline` — Report declined invite (triggers anti-spam cooldown)

**Server-side connect matching:** When player A sets `connect_to` = player B's room code, the server auto-sets B's `connect_to` = A's room code for mutual discovery.

**Anti-spam:** Exponential backoff on repeated declines between the same pair.

### Room Management

**Endpoints:**
- `POST /room/create` — Create a new room (`name`, `ft`, `player_id`)
- `POST /room/join` — Join by room code
- `POST /room/leave` — Leave a room
- `GET /room/state?room_code=XX` — Read-only room state
- `GET /room/events?room_code=XX` — SSE event stream (no auth)
- `GET /rooms/list` — List all active rooms with player counts

**Host migration:** When the host disconnects, the next player in the list becomes host. All clients are notified via `host_migrated` SSE event.

**Empty room cleanup:** Non-permanent rooms are auto-deleted when all players leave. Permanent rooms are kept.

### Match Proposal Flow (Phase 6)

Two-phase commit for casual room matches:

```
Queue ≥ 2 players
    │
    ▼
Server proposes match (SSE: match_propose)
    │
    ├──► Both accept within 30s ──► SSE: match_start ──► P2P connection
    │
    ├──► One declines ──► SSE: match_decline ──► Players return to queue
    │                     (30s cooldown on same pair)
    │
    └──► Timeout (30s) ──► SSE: match_decline (reason: "timeout")
                           (15s cooldown, both to back of queue)
```

**Pair selection:** Server iterates queue pairs, skipping any on decline cooldown.

### Match Reporting & Cross-Validation

**Both players must submit** a `MatchResult` via `POST /match_result`:
- `player_id`, `opponent_id`, `winner_id`
- `player_char`, `opponent_char` (character indices 0–19)
- `rounds` (total rounds played)
- `source` — `"casual"` or `"ranked"` (default: `"ranked"`)
- `ft` — First-To value for this session (e.g., 2 for FT2; default: 1)

**FT Session Tracking:** Match results are counted per FT session, not per individual game:
1. First player reports → stored as pending (session wins start at 0-0)
2. Second player reports → cross-validated (must agree on winner)
3. If both agree → session win counter incremented for the winner
4. If neither player has reached FT wins → `session_in_progress` returned, session continues
5. When one player reaches FT wins → session finalized, match recorded

**Casual vs Ranked:**
- `source === 'ranked'` → full Glicko-2 rating update + win/loss counters
- `source === 'casual'` → win/loss counters updated, **no Glicko-2 rating change**

**Dispute handling:** If players disagree on the game winner, the report is discarded but the session stays alive for the next game.

**Stale timeout:** If only one player reports within 30 seconds, the server auto-records it, trusts that report, and treats the absent player as a disconnect. Source field is respected (casual stale results skip rating updates).

**Room match end:** `POST /room/match/end` — triggers "winner stays on" rotation in casual rooms.

### Anti-Ragequit / Disconnect Detection

Three layers of protection:

1. **Client-side:** When `GekkoPlayerDisconnected` fires mid-match, the remaining player calls `LobbyServer_ReportDisconnect()` to report the opponent.

2. **Server-side timeout:** If only one player reports a match result within 30 seconds, the server auto-records it and increments the absent player's `disconnects` counter.

3. **Social pressure:** Disconnect count and percentage are visible on leaderboards and in lobby presence data.

**Endpoint:** `POST /match_disconnect` — `{player_id, opponent_id}`

### Replay Upload

Binary replay files are uploaded via `POST /match_result/replay?match_id=XX`:
- Content-Type: `application/octet-stream`
- Max size: 1MB
- HMAC signed (timestamp + "POST" + path + binary body)
- `X-Player-ID` header identifies the uploader
- Server stores as `replays/match_{id}.bin`
- Sets `has_replay = 1` on the match record

### Glicko-2 Rating System

Full implementation of the Glicko-2 algorithm ([reference paper](http://www.glicko.net/glicko/glicko2.pdf)).

| Parameter | Value |
|-----------|-------|
| Default rating | 1500.0 |
| Default RD | 350.0 |
| Default volatility | 0.06 |
| Scale factor | 173.7178 (= 400/ln(10)) |
| System constant (τ) | 0.5 |
| Min rating floor | 100 |
| RD range | 30–350 |

**Tier thresholds:**

| Tier | Rating |
|------|--------|
| Diamond | ≥ 2100 |
| Platinum | ≥ 1800 |
| Gold | ≥ 1500 |
| Silver | ≥ 1200 |
| Bronze | < 1200 |

### Leaderboards

**Endpoint:** `GET /leaderboard?page=N&limit=M`

Returns paginated player rankings sorted by rating. Each entry includes: rank, player_id, display_name, wins, losses, disconnects, rating, tier.

**Client API:** `LobbyServer_GetLeaderboard()` with page and limit parameters.

### GeoIP Region Detection

Server uses `geoip-lite` (optional npm package) to auto-detect player regions from IP address.

**Regions:** NA-East, NA-West, Europe-West, Europe-East, Asia, South America, Oceania, Africa, Middle East.

Country codes are mapped to regions (e.g., US/CA → NA-E, JP/KR → ASIA, BR → SA).

### Permanent Region Rooms

Pre-created rooms that persist even when empty:

| Code | Name | Default FT |
|------|------|-----------|
| `NAEA` | NA-East Public | FT1 |
| `NAWE` | NA-West Public | FT1 |
| `EURO` | Europe Public | FT1 |
| `ASIA` | Asia Public | FT1 |
| `MIDE` | Middle East Public | FT1 |
| `OCEA` | Oceania Public | FT1 |
| `BRAZ` | Brazil Public | FT1 |

Custom rooms use the creator's specified FT value (default FT2).

### Chat & Profanity Filter

- Messages sent via `POST /room/chat`
- Broadcast to all room members via SSE (`chat` event)
- Max 50 messages retained per room (`MAX_CHAT_MESSAGES`)
- Optional profanity filter via `bad-words` npm package
- Chat rate limiting: 1 message per second per player

### Database Schema (SQLite)

```sql
-- Player stats (Glicko-2 rated)
CREATE TABLE players_db (
    player_id TEXT PRIMARY KEY,
    display_name TEXT,
    wins INTEGER DEFAULT 0,
    losses INTEGER DEFAULT 0,
    disconnects INTEGER DEFAULT 0,
    rating REAL DEFAULT 1500.0,
    rd REAL DEFAULT 350.0,
    volatility REAL DEFAULT 0.06,
    last_match TEXT,
    created_at TEXT DEFAULT (datetime('now'))
);

-- Match history
CREATE TABLE matches (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    p1_id TEXT NOT NULL,
    p2_id TEXT NOT NULL,
    winner_id TEXT NOT NULL,
    p1_char INTEGER,
    p2_char INTEGER,
    rounds INTEGER,
    has_replay INTEGER DEFAULT 0,
    created_at TEXT DEFAULT (datetime('now'))
);

-- Pending results (FT session tracking + cross-validation)
CREATE TABLE pending_results (
    match_key TEXT PRIMARY KEY,
    reporter_id TEXT NOT NULL,
    winner_id TEXT NOT NULL,
    p1_id TEXT NOT NULL,
    p2_id TEXT NOT NULL,
    p1_char INTEGER,
    p2_char INTEGER,
    rounds INTEGER,
    source TEXT DEFAULT 'ranked',
    ft INTEGER DEFAULT 1,
    p1_session_wins INTEGER DEFAULT 0,
    p2_session_wins INTEGER DEFAULT 0,
    created_at TEXT DEFAULT (datetime('now'))
);
```

---

## Server API Reference

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| `GET` | `/` | No | Health check (players online, searching count) |
| `POST` | `/presence` | HMAC | Update player presence |
| `POST` | `/searching/start` | HMAC | Enter matchmaking |
| `POST` | `/searching/stop` | HMAC | Leave matchmaking |
| `GET` | `/searching[?region=XX]` | HMAC | List searching players |
| `POST` | `/leave` | HMAC | Remove player |
| `POST` | `/decline` | HMAC | Decline invite (anti-spam) |
| `POST` | `/match_result` | HMAC | Report match result |
| `POST` | `/match_result/replay?match_id=N` | HMAC | Upload replay binary |
| `POST` | `/match_disconnect` | HMAC | Report disconnect |
| `GET` | `/player/:id/stats` | HMAC | Get player stats |
| `GET` | `/leaderboard?page=N&limit=M` | HMAC | Get leaderboard page |
| `GET` | `/rooms/list` | HMAC | List active rooms |
| `POST` | `/room/create` | HMAC | Create room |
| `POST` | `/room/join` | HMAC | Join room |
| `POST` | `/room/leave` | HMAC | Leave room |
| `POST` | `/room/queue/join` | HMAC | Join match queue |
| `POST` | `/room/queue/leave` | HMAC | Leave match queue |
| `POST` | `/room/chat` | HMAC | Send chat message |
| `GET` | `/room/state?room_code=XX` | No | Read-only room state |
| `GET` | `/room/events?room_code=XX` | No | SSE event stream |
| `POST` | `/room/match/end` | HMAC | Report room match end |
| `POST` | `/room/match/accept` | HMAC | Accept proposed match |
| `POST` | `/room/match/decline` | HMAC | Decline proposed match |

---

## Configuration

Relevant `config.ini` keys:

| Key | Default | Description |
|-----|---------|-------------|
| `netplay.port` | `50000` | Local UDP port for GekkoNet |
| `netplay.auto_connect` | `false` | Auto-accept LAN challenges |
| `netplay.ft` | `2` | Default First-To value |
| `lobby_server.url` | `http://152.67.75.184:3000` | Lobby server URL |
| `lobby_server.key` | *(baked-in)* | HMAC shared key |
| `identity.player_id` | *(auto-generated)* | Persistent player ID |
| `identity.display_name` | `Player-XXXX` | Display name |

---

## Source File Map

### Client (`src/netplay/`)

| File | Lines | Purpose |
|------|-------|---------|
| `netplay.c` | 1202 | Core session loop, GekkoNet integration, rollback, input handling |
| `netplay.h` | 76 | Public API: session states, events, FT, spectate |
| `game_state.c` | 1820 | Save/load ~700+ game globals for rollback (compile-time size guard) |
| `lobby_server.c` | 1628 | HTTP client (16KB buffer), HMAC signing, SSE streaming (auto-reconnect), room management |
| `lobby_server.h` | 253 | Lobby API types (MatchResult with source/ft) and function declarations |
| `discovery.c` | 394 | LAN UDP broadcast beacons and peer tracking |
| `discovery.h` | 39 | Discovery API and peer struct |
| `stun.c` | ~500 | STUN binding, hole punching, room codes, socket helpers |
| `stun.h` | 65 | STUN API |
| `identity.c` | ~170 | Persistent identity generation (SHA-256) |
| `identity.h` | 46 | Identity API |
| `ping_probe.c` | ~320 | Direct peer-to-peer RTT measurement |
| `ping_probe.h` | 46 | Ping probe API |
| `upnp.c` | ~180 | UPnP-IGD port mapping |
| `upnp.h` | 37 | UPnP API |
| `net_detect.c` | ~120 | WiFi vs wired detection |
| `net_detect.h` | 31 | Net detect API |
| `sha256.c` | ~165 | Portable SHA-256 (non-Windows HMAC) |
| `sha256.h` | ~30 | SHA-256 API |

### Server (`tools/lobby-server/`)

| File | Lines | Purpose |
|------|-------|---------|
| `lobby-server.js` | 1640 | Full server: matchmaking, rooms, SSE, Glicko-2, FT sessions, SQLite |
| `lobby-server.service` | ~10 | systemd unit file for deployment |
| `deploy.sh` / `deploy.bat` | ~50 | Deployment scripts |

### UI (`assets/ui/` + `src/port/sdl/rmlui/`)

| File | Purpose |
|------|---------|
| `netplay.rml` / `netplay.rcss` | Netplay HUD overlay (stats, connection status) |
| `network_lobby.rml` / `network_lobby.rcss` | Internet lobby browser UI |
| `casual_lobby.rml` / `casual_lobby.rcss` | Casual room UI (chat, queue, player list) |
| `rmlui_network_lobby.cpp` | C++ bindings for network lobby |
| `rmlui_casual_lobby.cpp` | C++ bindings for casual lobby |
| `rmlui_netplay_ui.cpp` | C++ bindings for netplay HUD |
