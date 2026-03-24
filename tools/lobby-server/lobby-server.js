#!/usr/bin/env node
/**
 * @file lobby-server.js
 * @brief Minimal lobby/matchmaking server for 3SX P2P netplay.
 *
 * Zero external dependencies — uses only node:http and node:crypto.
 * Players register presence, mark themselves as "searching", and exchange
 * STUN room codes to establish P2P connections via hole-punching.
 *
 * Security: HMAC-SHA256 request signing.
 *   - Every request must include X-Timestamp and X-Signature headers.
 *   - Signature = HMAC-SHA256(secret, timestamp + method + path + body)
 *   - Requests with stale timestamps (>60s) or bad signatures are rejected.
 *
 * Features:
 *   - GeoIP region + country detection via geoip-lite
 *   - Connection type tracking (wifi/wired/unknown)
 *   - Anti-spam invite rate limiting with exponential backoff
 *
 * Environment variables:
 *   LOBBY_SECRET  — shared HMAC key (required)
 *   LOBBY_PORT    — HTTP port (default: 3000)
 *
 * Usage:
 *   LOBBY_SECRET="your-secret-key" node lobby-server.js
 */

const http = require('node:http');
const crypto = require('node:crypto');
const path = require('node:path');

// Try to load geoip-lite for country/region detection
let geoip = null;
try {
    geoip = require('geoip-lite');
    console.log('GeoIP: loaded (geoip-lite)');
} catch {
    console.warn('GeoIP: geoip-lite not installed — region/country detection disabled');
    console.warn('  Install with: npm install geoip-lite');
}

// Try to load bad-words for chat filtering
let Filter = null;
let badWordsFilter = null;
try {
    Filter = require('bad-words');
    badWordsFilter = new Filter();
    console.log('Profanity Filter: loaded (bad-words)');
} catch {
    console.warn('Profanity Filter: bad-words not installed — chat filtering disabled');
    console.warn('  Install with: npm install bad-words');
}

const PORT = parseInt(process.env.LOBBY_PORT || '3000', 10);
const SECRET = process.env.LOBBY_SECRET || '';
const MAX_BODY_SIZE = 65536; // 64 KB

if (!SECRET) {
    console.error('ERROR: LOBBY_SECRET environment variable is required.');
    process.exit(1);
}

// ---- Region Mapping ----

// Map geoip-lite country codes to game regions
const COUNTRY_TO_REGION = {};
// North America (default NA-E; refined to NA-W by timezone in detectRegionAndCountry)
['US', 'CA'].forEach(c => COUNTRY_TO_REGION[c] = 'NA-E');
// Europe West
['GB', 'IE', 'FR', 'ES', 'PT', 'NL', 'BE', 'DE', 'AT', 'CH', 'IT', 'DK', 'NO', 'SE', 'FI', 'IS', 'LU'].forEach(c => COUNTRY_TO_REGION[c] = 'EU-W');
// Europe East
['PL', 'CZ', 'SK', 'HU', 'RO', 'BG', 'HR', 'RS', 'UA', 'LT', 'LV', 'EE', 'GR', 'TR', 'RU'].forEach(c => COUNTRY_TO_REGION[c] = 'EU-E');
// Asia
['JP', 'KR', 'CN', 'TW', 'HK', 'SG', 'MY', 'TH', 'PH', 'ID', 'VN'].forEach(c => COUNTRY_TO_REGION[c] = 'ASIA');
// Middle East (+ South Asia)
['AE', 'SA', 'QA', 'KW', 'BH', 'OM', 'IQ', 'IR', 'JO', 'LB', 'SY', 'YE', 'IL', 'PS', 'IN', 'PK'].forEach(c => COUNTRY_TO_REGION[c] = 'MIDE');
// South America
['BR', 'AR', 'CL', 'CO', 'PE', 'VE', 'EC', 'UY', 'PY', 'BO'].forEach(c => COUNTRY_TO_REGION[c] = 'SA');
// Oceania
['AU', 'NZ'].forEach(c => COUNTRY_TO_REGION[c] = 'OCE');
// Africa
['ZA', 'NG', 'EG', 'KE', 'MA', 'TN', 'GH'].forEach(c => COUNTRY_TO_REGION[c] = 'AF');

// US/CA timezones that map to NA-West (Pacific, Mountain, Alaska, Hawaii)
const NA_WEST_TIMEZONES = new Set([
    'America/Los_Angeles', 'America/Vancouver', 'America/Denver', 'America/Edmonton',
    'America/Phoenix', 'America/Boise', 'America/Anchorage', 'America/Juneau',
    'Pacific/Honolulu', 'America/Tijuana', 'America/Dawson', 'America/Whitehorse',
    'America/Yellowknife', 'America/Regina', 'America/Swift_Current'
]);

function detectRegionAndCountry(ip) {
    if (!geoip) return { country: '', region: '' };
    // Strip IPv6 prefix from IPv4-mapped addresses
    const cleanIp = ip.replace(/^::ffff:/, '');
    const geo = geoip.lookup(cleanIp);
    if (!geo) return { country: '', region: '' };
    const country = geo.country || '';
    let region = COUNTRY_TO_REGION[country] || '';
    // Refine NA-E → NA-W for US/CA west coast using geoip timezone
    if (region === 'NA-E' && geo.timezone && NA_WEST_TIMEZONES.has(geo.timezone)) {
        region = 'NA-W';
    }
    return { country, region };
}

// ---- Data Store ----

/** @type {Map<string, {display_name: string, region: string, country: string, room_code: string, connect_to: string, status: string, connection_type: string, rtt_ms: number, ft: number, last_seen: number, last_chat_time: number}>} */
const players = new Map();

/** @type {Map<string, {count: number, until: number}>}  Key = "from_id->to_id" */
const declineCooldowns = new Map();

// ---- SQLite Database ----

let db = null;
try {
    const Database = require('better-sqlite3');
    const dbPath = path.join(__dirname, 'lobby.db');
    db = new Database(dbPath);
    db.pragma('journal_mode = WAL');  // Better concurrent read performance
    db.pragma('busy_timeout = 5000');

    // Create tables
    db.exec(`
        CREATE TABLE IF NOT EXISTS players_db (
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

        CREATE TABLE IF NOT EXISTS matches (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            p1_id TEXT NOT NULL,
            p2_id TEXT NOT NULL,
            winner_id TEXT NOT NULL,
            p1_char INTEGER,
            p2_char INTEGER,
            rounds INTEGER,
            created_at TEXT DEFAULT (datetime('now')),
            has_replay INTEGER DEFAULT 0
        );
        CREATE INDEX IF NOT EXISTS idx_matches_p1 ON matches(p1_id);
        CREATE INDEX IF NOT EXISTS idx_matches_p2 ON matches(p2_id);
    `);

    // Migration: add has_replay column if the table existed before this column was added
    try { db.exec('ALTER TABLE matches ADD COLUMN has_replay INTEGER DEFAULT 0;'); } catch { /* exists */ }
    
    // Add disconnects column to players_db if it doesn't exist
    try {
        db.exec('ALTER TABLE players_db ADD COLUMN disconnects INTEGER DEFAULT 0;');
    } catch { /* ignore if already exists */ }

    // Add country column to players_db for persistent flag display
    try {
        db.exec("ALTER TABLE players_db ADD COLUMN country TEXT DEFAULT '';");
    } catch { /* ignore if already exists */ }

    db.exec(`
        CREATE TABLE IF NOT EXISTS pending_results (
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
    `);
    // Migration: add new columns if table already existed
    try { db.exec('ALTER TABLE pending_results ADD COLUMN source TEXT DEFAULT \'ranked\';'); } catch { /* exists */ }
    try { db.exec('ALTER TABLE pending_results ADD COLUMN ft INTEGER DEFAULT 1;'); } catch { /* exists */ }
    try { db.exec('ALTER TABLE pending_results ADD COLUMN p1_session_wins INTEGER DEFAULT 0;'); } catch { /* exists */ }
    try { db.exec('ALTER TABLE pending_results ADD COLUMN p2_session_wins INTEGER DEFAULT 0;'); } catch { /* exists */ }
    console.log(`SQLite: initialized at ${dbPath}`);
} catch (err) {
    console.warn(`SQLite: not available (${err.message}) — match reporting disabled`);
    console.warn('  Install with: npm install better-sqlite3');
}

// ---- Room Tracking (Casual Lobbies) ----
// Structure: Room { id: string, name: string, host: string, players: string[], state: 'waiting'|'playing', permanent: boolean }
const rooms = new Map();

// Grace timers for implicit-disconnect-as-leave.
// Key: "roomCode:playerId", Value: setTimeout handle.
// When an SSE connection closes, a 5s timer starts. If the player
// reconnects SSE before it fires, the timer is cancelled.
const sseGraceTimers = new Map();

// Global SSE connections for players not in a room (used for QR join relay).
// Key: player_id, Value: http.ServerResponse (SSE stream)
const globalSseClients = new Map();

function createPermanentRooms() {
    const permanentRooms = [
        { id: 'OPEN', name: 'Open Arena',         regions: [], max_players: 16 },
        { id: 'NAEA', name: 'NA-East Public',     regions: ['NA-E'] },
        { id: 'NAWE', name: 'NA-West Public',     regions: ['NA-W'] },
        { id: 'EURO', name: 'Europe Public',      regions: ['EU-W', 'EU-E'] },
        { id: 'ASIA', name: 'Asia Public',        regions: ['ASIA'] },
        { id: 'MIDE', name: 'Middle East Public', regions: ['MIDE'] },
        { id: 'OCEA', name: 'Oceania Public',     regions: ['OCE'] },
        { id: 'BRAZ', name: 'Brazil Public',      regions: ['SA'] }
    ];

    for (const roomData of permanentRooms) {
        rooms.set(roomData.id, {
            id: roomData.id,
            name: roomData.name,
            host: 'server',
            players: [],
            queue: [],
            match: null,
            chat: [],
            spectators: new Set(),
            sseClients: new Set(),
            permanent: true,
            regions: roomData.regions,  // Restrict joins to these GeoIP regions (empty = open)
            max_players: roomData.max_players || 8,
            ft: 1  // Public region rooms default to FT1 (unranked)
        });
        const regionStr = roomData.regions.length > 0 ? ` (regions: ${roomData.regions.join(', ')})` : ' (open)';
        console.log(`[room] initialized permanent room ${roomData.id}: ${roomData.name}${regionStr}`);
    }
}
createPermanentRooms();

// Per-pair decline cooldown to prevent infinite re-proposal loops
// Key: "roomCode:idA-idB" (sorted), Value: expiry timestamp
const matchDeclineCooldowns = new Map();

function makeMatchPairKey(roomCode, id1, id2) {
    const sorted = [id1, id2].sort();
    return `${roomCode}:${sorted[0]}-${sorted[1]}`;
}

function generateRoomCode() {
    const chars = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789'; // Exclude I,O,1,0
    let code;
    do {
        code = '';
        for (let i = 0; i < 4; i++) {
            code += chars.charAt(Math.floor(Math.random() * chars.length));
        }
    } while (rooms.has(code));
    return code;
}

function getPlayerName(id) {
    const p = players.get(id);
    return p ? p.display_name : id;
}

function getRoomState(room) {
    const state = {
        id: room.id,
        name: room.name,
        host: room.host,
        ft: room.ft || 1,
        room_type: room.room_type || 0,
        visibility: room.visibility || 0,
        password_set: room.password ? 1 : 0,
        players: room.players.map(id => {
            const p = players.get(id);
            return { player_id: id, display_name: p ? p.display_name : id, region: p ? p.region : '', country: p ? p.country : '' };
        }),
        queue: room.queue,
        match: room.match,
        chat: [], // Chat is ephemeral; do not send history
        spectator_count: room.spectators ? room.spectators.size : 0
    };
    return state;
}

/**
 * Remove a player from a room (shared by /room/leave and implicit SSE disconnect).
 * Handles match forfeiture, host migration, room cleanup, and queue rotation.
 */
function removePlayerFromRoom(room, playerId) {
    if (!room.players.includes(playerId)) return; // Already gone

    // If leaving player is in an active or proposed match, handle it
    if (room.match && (room.match.state === 'playing' || room.match.state === 'proposed') &&
        (room.match.p1 === playerId || room.match.p2 === playerId)) {
        const other = room.match.p1 === playerId ? room.match.p2 : room.match.p1;
        if (room.match.state === 'proposed') {
            room.match = null;
            room.queue.unshift(other);
            broadcastRoomEvent(room, 'match_decline', {
                p1: { id: playerId, name: getPlayerName(playerId) },
                p2: { id: other, name: getPlayerName(other) },
                decliner_id: playerId, reason: 'disconnect'
            });
        } else {
            broadcastRoomEvent(room, 'match_end', {
                winner_id: other, winner_name: getPlayerName(other),
                loser_id: playerId, reason: 'disconnect'
            });
            room.queue.unshift(other);
            room.match = null;
        }
    }

    room.players = room.players.filter(p => p !== playerId);
    room.queue = room.queue.filter(p => p !== playerId);

    // Remove from spectators if applicable
    if (room.spectators && room.spectators.delete(playerId)) {
        broadcastRoomEvent(room, 'spectator_update', { count: room.spectators.size });
    }

    if (room.players.length === 0) {
        if (room.permanent) {
            room.match = null;
            room.queue = [];
        } else {
            rooms.delete(room.id);
        }
    } else {
        if (room.host === playerId) {
            room.host = room.players[0];
            broadcastRoomEvent(room, 'host_migrated', { host: room.host });
        }
        broadcastRoomEvent(room, 'leave', { player_id: playerId });
        tryStartMatch(room);
    }
}

/**
 * Try to start a match if conditions are met: no active/proposed match and ≥2 in queue.
 * Phase 6: Two-phase flow — first proposes (both must accept), then starts.
 * Returns true if a match was proposed.
 */
function tryStartMatch(room) {
    if (room.match && (room.match.state === 'playing' || room.match.state === 'proposed')) return false;
    if (room.queue.length < 2) return false;

    const now = Date.now();

    // Find first viable pair (skip pairs on decline cooldown or missing STUN)
    for (let i = 0; i < room.queue.length - 1; i++) {
        for (let j = i + 1; j < room.queue.length; j++) {
            const id1 = room.queue[i];
            const id2 = room.queue[j];

            // Skip pairs where either player has no STUN endpoint —
            // P2P connection is impossible without a room_code.
            const p1_data = players.get(id1);
            const p2_data = players.get(id2);
            if (!p1_data?.room_code || !p2_data?.room_code) continue;

            const pairKey = makeMatchPairKey(room.id, id1, id2);
            const cd = matchDeclineCooldowns.get(pairKey);
            if (cd && now < cd) continue; // still on cooldown

            // Found a viable pair — remove them from queue
            room.queue.splice(j, 1); // remove j first (higher index)
            room.queue.splice(i, 1);

            const p1 = id1;
            const p2 = id2;

            room.match = {
                p1, p2,
                state: 'proposed',
                accepts: { [p1]: false, [p2]: false },
                proposed_at: now
            };

            broadcastRoomEvent(room, 'match_propose', {
                ft: room.ft || 1,
                p1: {
                    id: p1, name: getPlayerName(p1),
                    connection_type: p1_data ? p1_data.connection_type : 'unknown',
                    rtt_ms: p1_data ? p1_data.rtt_ms : -1,
                    region: p1_data ? p1_data.region : '',
                    room_code: p1_data ? p1_data.room_code : ''
                },
                p2: {
                    id: p2, name: getPlayerName(p2),
                    connection_type: p2_data ? p2_data.connection_type : 'unknown',
                    rtt_ms: p2_data ? p2_data.rtt_ms : -1,
                    region: p2_data ? p2_data.region : '',
                    room_code: p2_data ? p2_data.room_code : ''
                }
            });
            broadcastRoomEvent(room, 'queue_update', { queue: room.queue });
            console.log(`[room] match proposed in ${room.id}: ${getPlayerName(p1)} vs ${getPlayerName(p2)}`);
            return true;
        }
    }

    return false; // no viable pairs
}

/**
 * Confirm a proposed match after both players accepted.
 * Transitions state from 'proposed' to 'playing' and broadcasts match_start.
 */
function confirmMatch(room) {
    if (!room.match || room.match.state !== 'proposed') return;
    room.match.state = 'playing';
    delete room.match.accepts;
    delete room.match.proposed_at;

    broadcastRoomEvent(room, 'match_start', {
        p1: { id: room.match.p1, name: getPlayerName(room.match.p1) },
        p2: { id: room.match.p2, name: getPlayerName(room.match.p2) }
    });
    console.log(`[room] match confirmed in ${room.id}: ${getPlayerName(room.match.p1)} vs ${getPlayerName(room.match.p2)}`);
}

/**
 * Cancel a proposed match (decline or timeout). Moves players back to queue.
 * @param {string} declinerId - Player who declined, or null for timeout.
 */
function cancelProposal(room, declinerId) {
    if (!room.match || room.match.state !== 'proposed') return;
    const { p1, p2 } = room.match;
    const reason = declinerId ? 'declined' : 'timeout';

    room.match = null;

    // Record cooldown to prevent instant re-proposal of same pair
    // Decline: 30s, Timeout: 15s (shorter, they may want to retry)
    const pairKey = makeMatchPairKey(room.id, p1, p2);
    const cooldownMs = declinerId ? 30_000 : 15_000;
    matchDeclineCooldowns.set(pairKey, Date.now() + cooldownMs);

    if (declinerId) {
        // Decliner to back of queue; other player stays at front
        const other = declinerId === p1 ? p2 : p1;
        room.queue.unshift(other);
        room.queue.push(declinerId);
    } else {
        // Timeout: both to back of queue
        room.queue.push(p1);
        room.queue.push(p2);
    }

    broadcastRoomEvent(room, 'match_decline', {
        p1: { id: p1, name: getPlayerName(p1) },
        p2: { id: p2, name: getPlayerName(p2) },
        decliner_id: declinerId || '',
        reason
    });
    broadcastRoomEvent(room, 'queue_update', { queue: room.queue });
    console.log(`[room] match proposal ${reason} in ${room.id}`);

    // Try to propose next pair
    tryStartMatch(room);
}

function broadcastRoomEvent(room, type, data) {
    const payload = JSON.stringify({ type, data });
    for (const client of room.sseClients) {
        client.write(`data: ${payload}\n\n`);
    }
}

// ---- Glicko-2 Rating System ----
// Reference: http://www.glicko.net/glicko/glicko2.pdf

const GLICKO2_SCALE = 173.7178;  // 400 / ln(10)
const TAU = 0.5;                  // system volatility constraint (lower = more conservative)
const EPSILON = 0.000001;         // convergence threshold
const DEFAULT_RATING = 1500.0;
const DEFAULT_RD = 350.0;
const DEFAULT_VOL = 0.06;

function g(phi) {
    return 1.0 / Math.sqrt(1.0 + 3.0 * phi * phi / (Math.PI * Math.PI));
}

function E(mu, mu_j, phi_j) {
    return 1.0 / (1.0 + Math.exp(-g(phi_j) * (mu - mu_j)));
}

/**
 * Compute Glicko-2 update for a single match.
 * @param {object} winner - {rating, rd, volatility}
 * @param {object} loser  - {rating, rd, volatility}
 * @returns {{winner: {rating, rd, vol}, loser: {rating, rd, vol}}}
 */
function glicko2Update(winner, loser) {
    // Step 1: Convert to Glicko-2 scale
    const mu_w = (winner.rating - 1500) / GLICKO2_SCALE;
    const phi_w = winner.rd / GLICKO2_SCALE;
    const sigma_w = winner.volatility;

    const mu_l = (loser.rating - 1500) / GLICKO2_SCALE;
    const phi_l = loser.rd / GLICKO2_SCALE;
    const sigma_l = loser.volatility;

    // Update winner (s = 1)
    const newW = updateSingle(mu_w, phi_w, sigma_w, mu_l, phi_l, 1.0);
    // Update loser (s = 0)
    const newL = updateSingle(mu_l, phi_l, sigma_l, mu_w, phi_w, 0.0);

    return {
        winner: {
            rating: Math.max(100, newW.mu * GLICKO2_SCALE + 1500),
            rd: Math.min(350, Math.max(30, newW.phi * GLICKO2_SCALE)),
            vol: newW.sigma,
        },
        loser: {
            rating: Math.max(100, newL.mu * GLICKO2_SCALE + 1500),
            rd: Math.min(350, Math.max(30, newL.phi * GLICKO2_SCALE)),
            vol: newL.sigma,
        },
    };
}

function updateSingle(mu, phi, sigma, mu_opp, phi_opp, score) {
    // Step 2: Variance
    const g_opp = g(phi_opp);
    const e_val = E(mu, mu_opp, phi_opp);
    const v = 1.0 / (g_opp * g_opp * e_val * (1.0 - e_val));

    // Step 3: Delta
    const delta = v * g_opp * (score - e_val);

    // Step 4: New volatility (Illinois algorithm)
    const a = Math.log(sigma * sigma);
    const phi2 = phi * phi;
    const delta2 = delta * delta;
    const tau2 = TAU * TAU;

    function f(x) {
        const ex = Math.exp(x);
        const num1 = ex * (delta2 - phi2 - v - ex);
        const den1 = 2.0 * (phi2 + v + ex) * (phi2 + v + ex);
        return num1 / den1 - (x - a) / tau2;
    }

    let A = a;
    let B;
    if (delta2 > phi2 + v) {
        B = Math.log(delta2 - phi2 - v);
    } else {
        let k = 1;
        while (f(a - k * TAU) < 0) k++;
        B = a - k * TAU;
    }

    let fA = f(A);
    let fB = f(B);
    while (Math.abs(B - A) > EPSILON) {
        const C = A + (A - B) * fA / (fB - fA);
        const fC = f(C);
        if (fC * fB <= 0) {
            A = B;
            fA = fB;
        } else {
            fA /= 2.0;
        }
        B = C;
        fB = fC;
    }
    const newSigma = Math.exp(A / 2.0);

    // Step 5: Update RD
    const phiStar = Math.sqrt(phi2 + newSigma * newSigma);
    const newPhi = 1.0 / Math.sqrt(1.0 / (phiStar * phiStar) + 1.0 / v);

    // Step 6: Update rating
    const newMu = mu + newPhi * newPhi * g_opp * (score - e_val);

    return { mu: newMu, phi: newPhi, sigma: newSigma };
}

function getTier(rating) {
    if (rating >= 2100) return 'diamond';
    if (rating >= 1800) return 'platinum';
    if (rating >= 1500) return 'gold';
    if (rating >= 1200) return 'silver';
    return 'bronze';
}

// Numeric grade (0-11) derived from rating — matches arcade grading scale
function getGrade(rating) {
    if (rating >= 2200) return 11; // S
    if (rating >= 2100) return 10; // A+
    if (rating >= 2000) return 9;  // A
    if (rating >= 1900) return 8;  // B+
    if (rating >= 1800) return 7;  // B
    if (rating >= 1700) return 6;  // C+
    if (rating >= 1600) return 5;  // C
    if (rating >= 1500) return 4;  // D+
    if (rating >= 1400) return 3;  // D
    if (rating >= 1300) return 2;  // E+
    if (rating >= 1200) return 1;  // E
    return 0;                      // F
}


// Cleanup stale players and expired decline cooldowns every 5 seconds
const cleanupTimer = setInterval(() => {
    const now = Date.now();
    const evictedIds = [];
    for (const [id, p] of players) {
        if (now - p.last_seen > 10_000) {
            players.delete(id);
            evictedIds.push(id);
        }
    }
    // Clean expired decline records
    for (const [key, cd] of declineCooldowns) {
        if (now > cd.until) {
            declineCooldowns.delete(key);
        }
    }
    // Clean expired match-pair decline cooldowns
    for (const [key, expiry] of matchDeclineCooldowns) {
        if (now > expiry) {
            matchDeclineCooldowns.delete(key);
        }
    }
    // Auto-record stale pending match results (>30s) as disconnects.
    // If only one player reported within 30s, trust that report and
    // increment the absent player's disconnect counter.
    if (db) {
        try {
            const stalePending = db.prepare(
                `SELECT * FROM pending_results WHERE datetime(created_at) < datetime('now', '-30 seconds')`
            ).all();
            if (stalePending.length > 0) {
                // Wrap all writes in a transaction for atomicity — either all
                // match records + rating updates succeed, or none do.
                const recordStaleResults = db.transaction(() => {
                    for (const p of stalePending) {
                        const winnerId = p.winner_id;

                        // Skip disputed/incomplete results — disputes clear winner_id to ''.
                        // Single-reporter results (loser never reported) still have a valid
                        // winner_id and should be trusted.
                        if (!winnerId || !p.p1_id || !p.p2_id) {
                            console.log(`[match] discarded stale disputed/empty pending: ${p.p1_id} vs ${p.p2_id}`);
                            continue;
                        }

                        const loserId = winnerId === p.p1_id ? p.p2_id : p.p1_id;
                        const winnerName = players.get(winnerId)?.display_name || winnerId;
                        const loserName = players.get(loserId)?.display_name || loserId;
                        const source = p.source || 'ranked';

                        // Record the match
                        db.prepare(`INSERT INTO matches (p1_id, p2_id, winner_id, p1_char, p2_char, rounds)
                                    VALUES (?, ?, ?, ?, ?, ?)`)
                          .run(p.p1_id, p.p2_id, winnerId, p.p1_char, p.p2_char, p.rounds || 0);

                        if (source === 'ranked') {
                            // Fetch current stats for Glicko-2
                            const getStats = db.prepare('SELECT rating, rd, volatility FROM players_db WHERE player_id = ?');
                            let wStats = getStats.get(winnerId) || { rating: DEFAULT_RATING, rd: DEFAULT_RD, volatility: DEFAULT_VOL };
                            let lStats = getStats.get(loserId)  || { rating: DEFAULT_RATING, rd: DEFAULT_RD, volatility: DEFAULT_VOL };
                            const newStats = glicko2Update(wStats, lStats);

                            // Upsert winner (win + rating)
                            db.prepare(`INSERT INTO players_db (player_id, display_name, wins, losses, disconnects, rating, rd, volatility)
                                VALUES (?, ?, 1, 0, 0, ?, ?, ?)
                                ON CONFLICT(player_id) DO UPDATE SET
                                    display_name = excluded.display_name, wins = wins + 1,
                                    rating = excluded.rating, rd = excluded.rd, volatility = excluded.volatility,
                                    last_match = datetime('now')
                            `).run(winnerId, winnerName, newStats.winner.rating, newStats.winner.rd, newStats.winner.vol);

                            // Upsert loser (loss + disconnect + rating)
                            db.prepare(`INSERT INTO players_db (player_id, display_name, wins, losses, disconnects, rating, rd, volatility)
                                VALUES (?, ?, 0, 1, 1, ?, ?, ?)
                                ON CONFLICT(player_id) DO UPDATE SET
                                    display_name = excluded.display_name, losses = losses + 1, disconnects = disconnects + 1,
                                    rating = excluded.rating, rd = excluded.rd, volatility = excluded.volatility,
                                    last_match = datetime('now')
                            `).run(loserId, loserName, newStats.loser.rating, newStats.loser.rd, newStats.loser.vol);
                        } else {
                            // Casual: record win/loss + disconnect but skip Glicko-2
                            db.prepare(`INSERT INTO players_db (player_id, display_name, wins, losses, disconnects)
                                VALUES (?, ?, 1, 0, 0)
                                ON CONFLICT(player_id) DO UPDATE SET
                                    display_name = excluded.display_name, wins = wins + 1,
                                    last_match = datetime('now')
                            `).run(winnerId, winnerName);
                            db.prepare(`INSERT INTO players_db (player_id, display_name, wins, losses, disconnects)
                                VALUES (?, ?, 0, 1, 1)
                                ON CONFLICT(player_id) DO UPDATE SET
                                    display_name = excluded.display_name, losses = losses + 1, disconnects = disconnects + 1,
                                    last_match = datetime('now')
                            `).run(loserId, loserName);
                        }

                        console.log(`[match] auto-recorded stale pending (${source}): ${winnerName} beat ${loserName} (disconnect)`);
                    }
                    db.prepare(`DELETE FROM pending_results WHERE datetime(created_at) < datetime('now', '-30 seconds')`).run();
                });
                recordStaleResults();
            }
        } catch { /* ignore */ }
    }
    // Phase 6: Timeout proposed matches (>10s without both accepts)
    for (const [, room] of rooms) {
        if (room.match && room.match.state === 'proposed' &&
            now - room.match.proposed_at > 30_000) {
            console.log(`[room] match proposal timed out in ${room.id}`);
            cancelProposal(room, null);
        }
    }
    // Evict stale players from rooms and clean up empty rooms
    if (evictedIds.length > 0) {
        for (const [code, room] of rooms) {
            const before = room.players.length;
            for (const staleId of evictedIds) {
                if (!room.players.includes(staleId)) continue;

                // Don't evict players who are in an active or proposed match —
                // their presence heartbeat stops during gameplay but they're
                // still connected via SSE and will rejoin presence afterward.
                if (room.match && (room.match.state === 'playing' || room.match.state === 'proposed') &&
                    (room.match.p1 === staleId || room.match.p2 === staleId)) {
                    continue;
                }

                room.players = room.players.filter(p => p !== staleId);
                room.queue = room.queue.filter(p => p !== staleId);
                broadcastRoomEvent(room, 'leave', { player_id: staleId });
            }

            // Detect zombie matches: match players evicted or gone
            if (room.match && (room.match.state === 'playing' || room.match.state === 'proposed')) {
                const p1Gone = !room.players.includes(room.match.p1);
                const p2Gone = !room.players.includes(room.match.p2);
                if (p1Gone || p2Gone) {
                    console.log(`[room] clearing zombie match in ${code}: ` +
                        `p1=${room.match.p1} (${p1Gone ? 'gone' : 'here'}), ` +
                        `p2=${room.match.p2} (${p2Gone ? 'gone' : 'here'})`);
                    if (!p1Gone) room.queue.unshift(room.match.p1);
                    if (!p2Gone) room.queue.unshift(room.match.p2);
                    room.match = null;
                    broadcastRoomEvent(room, 'match_end', {
                        winner_id: '', reason: 'abandoned'
                    });
                }
            }

            if (room.players.length === 0) {
                if (room.permanent) {
                    room.match = null;
                    room.queue = [];
                    console.log(`[room] ${code} empty but kept (permanent)`);
                } else {
                    console.log(`[room] ${code} auto-closed (all players stale)`);
                    rooms.delete(code);
                }
            } else if (room.players.length < before) {
                // Host migration if host was evicted
                if (!room.players.includes(room.host)) {
                    room.host = room.players[0];
                    broadcastRoomEvent(room, 'host_migrated', { host: room.host });
                }
                tryStartMatch(room);
            }
        }
    }
}, 5_000);

// ---- Tournament Bracket Logic ----

/**
 * Seed players for bracket generation.
 * @param {string[]} playerIds
 * @param {string} method - 'rating', 'join_order', or 'random'
 * @returns {string[]} Seeded player IDs
 */
function seedPlayers(playerIds, method) {
    const ids = [...playerIds];
    if (method === 'random') {
        // Fisher-Yates shuffle
        for (let i = ids.length - 1; i > 0; i--) {
            const j = Math.floor(Math.random() * (i + 1));
            [ids[i], ids[j]] = [ids[j], ids[i]];
        }
    } else if (method === 'rating' && db) {
        // Sort by Glicko-2 rating (descending) — highest seed = best player
        const getStats = db.prepare('SELECT rating FROM players_db WHERE player_id = ?');
        ids.sort((a, b) => {
            const ra = getStats.get(a)?.rating || 1500;
            const rb = getStats.get(b)?.rating || 1500;
            return rb - ra; // Descending
        });
    }
    // 'join_order' = keep as-is (players array order)
    return ids;
}

/**
 * Standard bracket seeding order for power-of-2 sizes.
 * For 8 players: [1,8,4,5,2,7,3,6] — ensures top seeds meet latest.
 */
function bracketSeedOrder(size) {
    if (size <= 1) return [0];
    const order = [0, 1]; // Seeds 1 and 2
    let step = 2;
    while (step < size) {
        const next = [];
        for (const s of order) {
            next.push(s);
            next.push(step * 2 - 1 - s);
        }
        order.length = 0;
        order.push(...next);
        step *= 2;
    }
    return order;
}

/**
 * Generate a single-elimination bracket.
 * Returns { bracket: BracketEntry[], total_rounds: number }
 */
function generateSingleElimBracket(seededPlayers) {
    const n = seededPlayers.length;
    if (n < 2) return { bracket: [], total_rounds: 0 };

    // Round up to next power of 2
    let size = 1;
    while (size < n) size *= 2;
    const totalRounds = Math.log2(size);

    // Place players into seed slots (extras get BYE)
    const slots = new Array(size).fill(null);
    const seedOrder = bracketSeedOrder(size);
    for (let i = 0; i < n; i++) {
        slots[seedOrder[i]] = seededPlayers[i];
    }

    const bracket = [];

    // Round 0: initial matchups
    for (let pos = 0; pos < size / 2; pos++) {
        const p1 = slots[pos * 2] || '';
        const p2 = slots[pos * 2 + 1] || '';
        const isBye = !p1 || !p2;
        const winner = isBye ? (p1 || p2) : ''; // BYE auto-advances
        bracket.push({
            round: 0,
            position: pos,
            player1_id: p1,
            player1_name: p1 ? getPlayerName(p1) : 'BYE',
            player2_id: p2,
            player2_name: p2 ? getPlayerName(p2) : 'BYE',
            winner_id: winner,
            completed: isBye ? 1 : 0
        });
    }

    // Subsequent rounds: empty entries (filled as matches complete)
    let matchesInRound = size / 4;
    for (let round = 1; round < totalRounds; round++) {
        for (let pos = 0; pos < matchesInRound; pos++) {
            bracket.push({
                round,
                position: pos,
                player1_id: '',
                player1_name: '',
                player2_id: '',
                player2_name: '',
                winner_id: '',
                completed: 0
            });
        }
        matchesInRound = Math.max(1, matchesInRound / 2);
    }

    return { bracket, total_rounds: totalRounds };
}

/**
 * Generate round-robin pairings for a given round using the circle algorithm.
 * Returns array of {p1, p2} pairs.
 */
function generateRoundRobinPairings(playerIds, round) {
    const ids = [...playerIds];
    if (ids.length % 2 !== 0) ids.push(null); // BYE for odd count
    const n = ids.length;
    const totalRounds = n - 1;
    const half = n / 2;

    // Rotate all but first element
    const rotated = [ids[0]];
    const rest = ids.slice(1);
    const currentRound = round % totalRounds;
    for (let i = 0; i < rest.length; i++) {
        rotated.push(rest[(i + currentRound) % rest.length]);
    }

    const pairs = [];
    for (let i = 0; i < half; i++) {
        const p1 = rotated[i];
        const p2 = rotated[n - 1 - i];
        if (p1 && p2) { // Skip BYE pairings
            pairs.push({ p1, p2 });
        }
    }
    return pairs;
}

/**
 * Generate Swiss pairings: pair players with similar records, avoid rematches.
 * @param {string[]} playerIds
 * @param {Map<string,{wins:number,losses:number}>} standings
 * @param {Array<{p1:string,p2:string}>} previousPairings - all past pairings
 */
function generateSwissPairings(playerIds, standings, previousPairings) {
    // Sort by wins descending, then losses ascending
    const sorted = [...playerIds].sort((a, b) => {
        const sa = standings.get(a) || { wins: 0, losses: 0 };
        const sb = standings.get(b) || { wins: 0, losses: 0 };
        if (sb.wins !== sa.wins) return sb.wins - sa.wins;
        return sa.losses - sb.losses;
    });

    const pastPairSet = new Set(previousPairings.map(p => {
        const s = [p.p1, p.p2].sort();
        return s[0] + ':' + s[1];
    }));

    const paired = new Set();
    const pairs = [];
    for (let i = 0; i < sorted.length; i++) {
        if (paired.has(sorted[i])) continue;
        for (let j = i + 1; j < sorted.length; j++) {
            if (paired.has(sorted[j])) continue;
            const key = [sorted[i], sorted[j]].sort().join(':');
            if (!pastPairSet.has(key)) {
                pairs.push({ p1: sorted[i], p2: sorted[j] });
                paired.add(sorted[i]);
                paired.add(sorted[j]);
                break;
            }
        }
    }
    return pairs;
}

/**
 * Advance a winner in a single-elim bracket to the next round.
 * Returns true if the current round is now fully complete.
 */
function advanceSingleElimBracket(tournament, matchRound, matchPosition, winnerId) {
    const bracket = tournament.bracket;

    // Mark match complete
    const entry = bracket.find(b => b.round === matchRound && b.position === matchPosition);
    if (!entry) return false;
    entry.winner_id = winnerId;
    entry.completed = 1;

    // Place winner in next round
    const nextRound = matchRound + 1;
    if (nextRound < tournament.total_rounds) {
        const nextPos = Math.floor(matchPosition / 2);
        const nextEntry = bracket.find(b => b.round === nextRound && b.position === nextPos);
        if (nextEntry) {
            if (matchPosition % 2 === 0) {
                nextEntry.player1_id = winnerId;
                nextEntry.player1_name = getPlayerName(winnerId);
            } else {
                nextEntry.player2_id = winnerId;
                nextEntry.player2_name = getPlayerName(winnerId);
            }

            // Auto-complete BYE: if opponent slot is empty and this is the only feeder
            // (This handles cascading BYEs in subsequent rounds)
            if (nextEntry.player1_id && !nextEntry.player2_id) {
                // Check if the other feeder match was a BYE (already completed)
                const otherFeederPos = matchPosition % 2 === 0 ? matchPosition + 1 : matchPosition - 1;
                const otherFeeder = bracket.find(b => b.round === matchRound && b.position === otherFeederPos);
                if (otherFeeder && otherFeeder.completed && !otherFeeder.player1_id && !otherFeeder.player2_id) {
                    // Other feeder was empty BYE, auto-advance
                    nextEntry.winner_id = nextEntry.player1_id;
                    nextEntry.completed = 1;
                    // Recursively advance
                    advanceSingleElimBracket(tournament, nextRound, nextPos, nextEntry.player1_id);
                }
            } else if (!nextEntry.player1_id && nextEntry.player2_id) {
                const otherFeederPos = matchPosition % 2 === 0 ? matchPosition + 1 : matchPosition - 1;
                const otherFeeder = bracket.find(b => b.round === matchRound && b.position === otherFeederPos);
                if (otherFeeder && otherFeeder.completed && !otherFeeder.player1_id && !otherFeeder.player2_id) {
                    nextEntry.winner_id = nextEntry.player2_id;
                    nextEntry.completed = 1;
                    advanceSingleElimBracket(tournament, nextRound, nextPos, nextEntry.player2_id);
                }
            }
        }
    }

    // Check if current round is fully complete
    const roundMatches = bracket.filter(b => b.round === matchRound);
    return roundMatches.every(b => b.completed);
}

/**
 * Generate a double-elimination bracket.
 *
 * Structure:
 *   - Winners bracket (bracket_side='W'): identical to single-elim
 *   - Losers bracket (bracket_side='L'): 2*(W_rounds-1) rounds
 *       - Odd L rounds receive drop-downs from winners bracket
 *       - Even L rounds are internal losers bracket matches
 *   - Grand Finals (bracket_side='GF'): W champion vs L champion
 *
 * Returns { bracket: BracketEntry[], total_rounds: number }
 * total_rounds = W_rounds + L_rounds + 1 (GF)
 */
function generateDoubleElimBracket(seededPlayers) {
    const n = seededPlayers.length;
    if (n < 2) return { bracket: [], total_rounds: 0 };

    // Generate winners bracket first (same structure as single-elim)
    let wSize = 1;
    while (wSize < n) wSize *= 2;
    const wRounds = Math.log2(wSize);

    const slots = new Array(wSize).fill(null);
    const seedOrder = bracketSeedOrder(wSize);
    for (let i = 0; i < n; i++) {
        slots[seedOrder[i]] = seededPlayers[i];
    }

    const bracket = [];

    // Winners round 0
    for (let pos = 0; pos < wSize / 2; pos++) {
        const p1 = slots[pos * 2] || '';
        const p2 = slots[pos * 2 + 1] || '';
        const isBye = !p1 || !p2;
        bracket.push({
            round: 0, position: pos, bracket_side: 'W',
            player1_id: p1, player1_name: p1 ? getPlayerName(p1) : 'BYE',
            player2_id: p2, player2_name: p2 ? getPlayerName(p2) : 'BYE',
            winner_id: isBye ? (p1 || p2) : '', completed: isBye ? 1 : 0
        });
    }

    // Winners subsequent rounds
    let matchesInRound = wSize / 4;
    for (let round = 1; round < wRounds; round++) {
        for (let pos = 0; pos < matchesInRound; pos++) {
            bracket.push({
                round, position: pos, bracket_side: 'W',
                player1_id: '', player1_name: '', player2_id: '', player2_name: '',
                winner_id: '', completed: 0
            });
        }
        matchesInRound = Math.max(1, matchesInRound / 2);
    }

    // Losers bracket: 2 * (wRounds - 1) rounds
    // Structure per pair of L rounds:
    //   L_even: drop-downs from winners arrive + existing losers advance (reduces by half)
    //   L_odd:  internal match between remaining losers (no new drop-downs)
    // Exception: L0 has wSize/4 matches (losers from W0 paired up),
    //            then L1 receives W1 losers to play L0 winners
    const lRounds = 2 * (wRounds - 1);
    for (let lr = 0; lr < lRounds; lr++) {
        // Calculate match count for this losers round
        let matchCount;
        if (lr === 0) {
            matchCount = wSize / 4; // W0 losers paired (half of W0 matches / 2)
        } else {
            // After L0 (wSize/4 slots), each pair of rounds halves:
            // L0: wSize/4 matches → wSize/4 winners
            // L1: wSize/4 matches (L0 winners vs W1 dropdowns) → wSize/4 winners
            // L2: wSize/8 matches (L1 winners pair internally) → wSize/8 winners
            // L3: wSize/8 matches (L2 winners vs W2 dropdowns) → wSize/8 winners
            // Pattern: matches halve every 2 rounds starting from L2
            const phase = Math.floor((lr + 1) / 2); // 0-indexed phase
            matchCount = Math.max(1, wSize / (4 * Math.pow(2, phase)));
            // Odd rounds (drop-down rounds) keep same size as previous even round
            if (lr % 2 === 1 && lr > 0) {
                const prevCount = Math.max(1, wSize / (4 * Math.pow(2, Math.floor(lr / 2))));
                matchCount = prevCount;
            }
        }

        for (let pos = 0; pos < matchCount; pos++) {
            bracket.push({
                round: lr, position: pos, bracket_side: 'L',
                player1_id: '', player1_name: '',
                player2_id: '', player2_name: '',
                winner_id: '', completed: 0
            });
        }
    }

    // Grand Finals: 1 entry
    bracket.push({
        round: 0, position: 0, bracket_side: 'GF',
        player1_id: '', player1_name: '', // Winners champion
        player2_id: '', player2_name: '', // Losers champion
        winner_id: '', completed: 0
    });

    const totalRounds = wRounds + lRounds + 1;
    return { bracket, total_rounds: totalRounds, w_rounds: wRounds, l_rounds: lRounds };
}

/**
 * Advance a winner in a double-elim bracket.
 * Routes:
 *   - Winners bracket loser → losers bracket
 *   - Winners bracket winner → next winners round (or GF if finals)
 *   - Losers bracket winner → next losers round (or GF if losers finals)
 *   - Losers bracket loser → eliminated
 *   - GF winner → champion
 *
 * Returns true if the match's bracket round is now fully complete.
 */
function advanceDoubleElimBracket(tournament, side, matchRound, matchPosition, winnerId) {
    const bracket = tournament.bracket;

    // Find and mark the match
    const entry = bracket.find(b =>
        b.bracket_side === side && b.round === matchRound && b.position === matchPosition
    );
    if (!entry) return false;

    const loserId = (entry.player1_id === winnerId) ? entry.player2_id : entry.player1_id;
    entry.winner_id = winnerId;
    entry.completed = 1;

    const wRounds = tournament.w_rounds;
    const lRounds = tournament.l_rounds;

    if (side === 'W') {
        // --- Winners bracket advancement ---
        const nextWRound = matchRound + 1;

        if (nextWRound < wRounds) {
            // Advance winner to next winners round
            const nextPos = Math.floor(matchPosition / 2);
            const nextEntry = bracket.find(b => b.bracket_side === 'W' && b.round === nextWRound && b.position === nextPos);
            if (nextEntry) {
                if (matchPosition % 2 === 0) {
                    nextEntry.player1_id = winnerId;
                    nextEntry.player1_name = getPlayerName(winnerId);
                } else {
                    nextEntry.player2_id = winnerId;
                    nextEntry.player2_name = getPlayerName(winnerId);
                }
                // Auto-complete if opponent is BYE (empty other feeder completed without players)
                _autoCompleteBye(bracket, nextEntry, 'W', matchRound, matchPosition);
            }
        } else {
            // Winners finals winner → Grand Finals player1
            const gf = bracket.find(b => b.bracket_side === 'GF');
            if (gf) {
                gf.player1_id = winnerId;
                gf.player1_name = getPlayerName(winnerId);
                _autoCompleteGF(bracket, gf);
            }
        }

        // Drop loser to losers bracket (skip BYE losers)
        if (loserId) {
            _dropToLosers(bracket, wRounds, matchRound, matchPosition, loserId);
        }
    } else if (side === 'L') {
        // --- Losers bracket advancement ---
        const nextLRound = matchRound + 1;

        if (nextLRound < lRounds) {
            // Advance winner to next losers round
            // Losers bracket: even→odd keeps same count, odd→even halves
            let nextPos;
            if (matchRound % 2 === 0) {
                // Even round → odd round (drop-down round): same position count
                nextPos = matchPosition;
            } else {
                // Odd round → even round (internal): halve
                nextPos = Math.floor(matchPosition / 2);
            }
            const nextEntry = bracket.find(b => b.bracket_side === 'L' && b.round === nextLRound && b.position === nextPos);
            if (nextEntry) {
                // In drop-down rounds (odd), the losers bracket player goes to p1, dropdown to p2
                // In internal rounds (even), standard top/bottom
                if (matchRound % 2 === 0) {
                    // Winner of even round → p1 of next odd round (will face dropdown in p2)
                    nextEntry.player1_id = winnerId;
                    nextEntry.player1_name = getPlayerName(winnerId);
                } else {
                    // Winner of odd round → feed into next even round
                    if (matchPosition % 2 === 0) {
                        nextEntry.player1_id = winnerId;
                        nextEntry.player1_name = getPlayerName(winnerId);
                    } else {
                        nextEntry.player2_id = winnerId;
                        nextEntry.player2_name = getPlayerName(winnerId);
                    }
                }
            }
        } else {
            // Losers finals winner → Grand Finals player2
            const gf = bracket.find(b => b.bracket_side === 'GF');
            if (gf) {
                gf.player2_id = winnerId;
                gf.player2_name = getPlayerName(winnerId);
                _autoCompleteGF(bracket, gf);
            }
        }
        // Loser is eliminated (nothing to do)
    } else if (side === 'GF') {
        // Grand finals complete — winner is tournament champion
        // Nothing to advance
    }

    // Check if all matches on this side+round are complete
    const roundMatches = bracket.filter(b => b.bracket_side === side && b.round === matchRound);
    return roundMatches.every(b => b.completed);
}

/**
 * Drop a loser from winners bracket round N into the appropriate losers bracket slot.
 *
 * Mapping (standard double-elim drop pattern):
 *   W0 loser → L0 (initial losers round)
 *   W1 loser → L1 (faces L0 winner)
 *   W2 loser → L3 (faces L2 winner)
 *   W(N) loser → L(2N-1) for N≥1
 */
function _dropToLosers(bracket, wRounds, wRound, wPosition, loserId) {
    let targetLRound, targetPos;

    if (wRound === 0) {
        // W0 losers pair up in L0
        targetLRound = 0;
        targetPos = Math.floor(wPosition / 2);
        const lEntry = bracket.find(b => b.bracket_side === 'L' && b.round === 0 && b.position === targetPos);
        if (lEntry) {
            if (wPosition % 2 === 0) {
                lEntry.player1_id = loserId;
                lEntry.player1_name = getPlayerName(loserId);
            } else {
                lEntry.player2_id = loserId;
                lEntry.player2_name = getPlayerName(loserId);
            }
            // Auto-complete if both slots filled from BYE wins
            if (lEntry.player1_id && lEntry.player2_id && !lEntry.completed) {
                // Both fed — ready for match (don't auto-complete, let it be proposed)
            } else if (lEntry.player1_id && !lEntry.player2_id) {
                // Check if other feeder is a BYE (no player in that W0 slot)
                const otherWPos = wPosition % 2 === 0 ? wPosition + 1 : wPosition - 1;
                const otherW = bracket.find(b => b.bracket_side === 'W' && b.round === 0 && b.position === otherWPos);
                if (otherW && otherW.completed && (!otherW.player1_id || !otherW.player2_id)) {
                    // Other W0 match was a BYE — no loser drops. Auto-advance this loser.
                    lEntry.winner_id = lEntry.player1_id;
                    lEntry.completed = 1;
                    advanceDoubleElimBracket({ bracket, w_rounds: wRounds, l_rounds: 2 * (wRounds - 1) }, 'L', 0, targetPos, lEntry.player1_id);
                }
            }
        }
    } else {
        // W(N≥1) losers drop into L(2N-1) as player2 (facing the losers bracket survivor)
        targetLRound = 2 * wRound - 1;
        targetPos = wPosition; // Same position (1:1 mapping at this stage)
        const lEntry = bracket.find(b => b.bracket_side === 'L' && b.round === targetLRound && b.position === targetPos);
        if (lEntry) {
            lEntry.player2_id = loserId;
            lEntry.player2_name = getPlayerName(loserId);
        }
    }
}

/**
 * Auto-complete BYE entries in winners bracket (same logic as single-elim).
 */
function _autoCompleteBye(bracket, nextEntry, side, sourceRound, sourcePosition) {
    if (nextEntry.player1_id && !nextEntry.player2_id) {
        const otherFeederPos = sourcePosition % 2 === 0 ? sourcePosition + 1 : sourcePosition - 1;
        const otherFeeder = bracket.find(b => b.bracket_side === side && b.round === sourceRound && b.position === otherFeederPos);
        if (otherFeeder && otherFeeder.completed && !otherFeeder.player1_id && !otherFeeder.player2_id) {
            nextEntry.winner_id = nextEntry.player1_id;
            nextEntry.completed = 1;
        }
    } else if (!nextEntry.player1_id && nextEntry.player2_id) {
        const otherFeederPos = sourcePosition % 2 === 0 ? sourcePosition + 1 : sourcePosition - 1;
        const otherFeeder = bracket.find(b => b.bracket_side === side && b.round === sourceRound && b.position === otherFeederPos);
        if (otherFeeder && otherFeeder.completed && !otherFeeder.player1_id && !otherFeeder.player2_id) {
            nextEntry.winner_id = nextEntry.player2_id;
            nextEntry.completed = 1;
        }
    }
}

/**
 * Auto-complete Grand Finals if only one player made it (shouldn't happen normally).
 */
function _autoCompleteGF(bracket, gf) {
    // If both slots are filled but one is empty string, auto-advance the other
    // (This only triggers in degenerate cases like 2-player double-elim)
}

/**
 * Get tournament state as JSON for SSE broadcast / API response.
 */
function getTournamentState(room) {
    if (!room.tournament) return null;
    const t = room.tournament;
    return {
        format: t.format,
        round: t.round,
        total_rounds: t.total_rounds,
        started: t.started ? 1 : 0,
        paused: t.paused ? 1 : 0,
        matches: t.matches.map(m => ({
            p1: m.p1,
            p2: m.p2,
            active: m.active ? 1 : 0,
            round: m.bracket_round,
            position: m.bracket_position,
            match_index: m.match_index
        })),
        bracket: t.bracket
    };
}

/**
 * Propose a single tournament match. Shared by single-elim and double-elim paths.
 * Creates the match object, pushes it to t.matches, and broadcasts match_propose.
 */
function _proposeTournamentMatch(room, t, entry, matchIndex) {
    const match = {
        p1: entry.player1_id,
        p2: entry.player2_id,
        active: false,
        bracket_round: entry.round,
        bracket_position: entry.position,
        bracket_side: entry.bracket_side || '',
        match_index: matchIndex,
        state: 'proposed',
        accepts: { [entry.player1_id]: false, [entry.player2_id]: false },
        proposed_at: Date.now()
    };
    t.matches.push(match);

    const p1_data = players.get(entry.player1_id);
    const p2_data = players.get(entry.player2_id);
    broadcastRoomEvent(room, 'match_propose', {
        ft: room.ft || 1,
        match_index: matchIndex,
        bracket_side: entry.bracket_side || '',
        p1: {
            id: entry.player1_id, name: entry.player1_name || getPlayerName(entry.player1_id),
            connection_type: p1_data ? p1_data.connection_type : 'unknown',
            rtt_ms: p1_data ? p1_data.rtt_ms : -1,
            region: p1_data ? p1_data.region : '',
            room_code: p1_data ? p1_data.room_code : ''
        },
        p2: {
            id: entry.player2_id, name: entry.player2_name || getPlayerName(entry.player2_id),
            connection_type: p2_data ? p2_data.connection_type : 'unknown',
            rtt_ms: p2_data ? p2_data.rtt_ms : -1,
            region: p2_data ? p2_data.region : '',
            room_code: p2_data ? p2_data.room_code : ''
        }
    });
    const sideLabel = entry.bracket_side ? `[${entry.bracket_side}]` : '';
    console.log(`[tournament] match proposed in ${room.id}: ${getPlayerName(entry.player1_id)} vs ${getPlayerName(entry.player2_id)} (${sideLabel} round ${entry.round}, pos ${entry.position})`);
}

/**
 * Fire match proposals for all unplayed matches in the current tournament round.
 * For single/double elim: proposes all non-BYE, non-completed matches in current round.
 * For round-robin/Swiss: proposes the pairings generated for this round.
 */
function tryStartTournamentRound(room) {
    const t = room.tournament;
    if (!t || !t.started || t.paused) return;

    // Clear previous round's matches
    t.matches = [];

    if (t.format === 0) {
        // Single Elim: propose all non-completed matches in current round
        const roundMatches = t.bracket.filter(b => b.round === t.round && !b.completed);
        let matchIndex = 0;
        for (const entry of roundMatches) {
            if (!entry.player1_id || !entry.player2_id) continue;
            _proposeTournamentMatch(room, t, entry, matchIndex++);
        }

        // If no matches to propose (all BYEs), auto-advance round
        if (t.matches.length === 0 && t.round < t.total_rounds - 1) {
            t.round++;
            console.log(`[tournament] auto-advancing to round ${t.round} (all BYEs)`);
            broadcastRoomEvent(room, 'round_advance', { round: t.round });
            tryStartTournamentRound(room);
        }
    } else if (t.format === 1) {
        // Double Elim: propose ALL ready matches across W, L, and GF brackets
        const readyMatches = t.bracket.filter(b =>
            !b.completed && b.player1_id && b.player2_id
        );
        let matchIndex = 0;
        for (const entry of readyMatches) {
            _proposeTournamentMatch(room, t, entry, matchIndex++);
        }
    } else if (t.format === 2) {
        // Round Robin: generate pairings for current round
        const activePlayers = room.players.filter(p => !t.dq_players.includes(p));
        const pairs = generateRoundRobinPairings(activePlayers, t.round);
        let matchIndex = 0;
        for (const pair of pairs) {
            const entry = {
                round: t.round,
                position: matchIndex,
                player1_id: pair.p1,
                player1_name: getPlayerName(pair.p1),
                player2_id: pair.p2,
                player2_name: getPlayerName(pair.p2),
                winner_id: '',
                completed: 0
            };
            t.bracket.push(entry);

            const match = {
                p1: pair.p1, p2: pair.p2,
                active: false,
                bracket_round: t.round,
                bracket_position: matchIndex,
                match_index: matchIndex,
                state: 'proposed',
                accepts: { [pair.p1]: false, [pair.p2]: false },
                proposed_at: Date.now()
            };
            t.matches.push(match);

            const p1_data = players.get(pair.p1);
            const p2_data = players.get(pair.p2);
            broadcastRoomEvent(room, 'match_propose', {
                ft: room.ft || 1,
                match_index: matchIndex,
                p1: { id: pair.p1, name: getPlayerName(pair.p1), connection_type: p1_data?.connection_type || 'unknown', rtt_ms: p1_data?.rtt_ms || -1, region: p1_data?.region || '', room_code: p1_data?.room_code || '' },
                p2: { id: pair.p2, name: getPlayerName(pair.p2), connection_type: p2_data?.connection_type || 'unknown', rtt_ms: p2_data?.rtt_ms || -1, region: p2_data?.region || '', room_code: p2_data?.room_code || '' }
            });
            matchIndex++;
        }
    } else if (t.format === 3) {
        // Swiss: generate pairings based on standings
        const activePlayers = room.players.filter(p => !t.dq_players.includes(p));
        const standings = new Map();
        for (const p of activePlayers) {
            const wins = t.bracket.filter(b => b.winner_id === p).length;
            const losses = t.bracket.filter(b => b.completed && (b.player1_id === p || b.player2_id === p) && b.winner_id !== p).length;
            standings.set(p, { wins, losses });
        }
        const previousPairings = t.bracket.map(b => ({ p1: b.player1_id, p2: b.player2_id }));
        const pairs = generateSwissPairings(activePlayers, standings, previousPairings);
        let matchIndex = 0;
        for (const pair of pairs) {
            const entry = {
                round: t.round, position: matchIndex,
                player1_id: pair.p1, player1_name: getPlayerName(pair.p1),
                player2_id: pair.p2, player2_name: getPlayerName(pair.p2),
                winner_id: '', completed: 0
            };
            t.bracket.push(entry);

            const match = {
                p1: pair.p1, p2: pair.p2, active: false,
                bracket_round: t.round, bracket_position: matchIndex, match_index: matchIndex,
                state: 'proposed', accepts: { [pair.p1]: false, [pair.p2]: false }, proposed_at: Date.now()
            };
            t.matches.push(match);

            const p1_data = players.get(pair.p1);
            const p2_data = players.get(pair.p2);
            broadcastRoomEvent(room, 'match_propose', {
                ft: room.ft || 1, match_index: matchIndex,
                p1: { id: pair.p1, name: getPlayerName(pair.p1), connection_type: p1_data?.connection_type || 'unknown', rtt_ms: p1_data?.rtt_ms || -1, region: p1_data?.region || '', room_code: p1_data?.room_code || '' },
                p2: { id: pair.p2, name: getPlayerName(pair.p2), connection_type: p2_data?.connection_type || 'unknown', rtt_ms: p2_data?.rtt_ms || -1, region: p2_data?.region || '', room_code: p2_data?.room_code || '' }
            });
            matchIndex++;
        }
    }

    // Broadcast updated bracket state
    broadcastRoomEvent(room, 'bracket_update', getTournamentState(room));
}

// ---- Auth ----

function verifyRequest(method, path, body, headers) {
    const timestamp = headers['x-timestamp'];
    const signature = headers['x-signature'];

    if (!timestamp || !signature) {
        return { ok: false, reason: 'Missing auth headers' };
    }

    // Reject stale timestamps (>60s drift)
    const ts = parseInt(timestamp, 10);
    const drift = Math.abs(Date.now() / 1000 - ts);
    if (isNaN(ts) || drift > 60) {
        return { ok: false, reason: 'Stale timestamp' };
    }

    // Verify HMAC — use .update() chaining to handle both string and Buffer bodies
    const hmac = crypto.createHmac('sha256', SECRET);
    hmac.update(timestamp + method + path);
    if (Buffer.isBuffer(body)) {
        hmac.update(body); // Binary body: feed directly as Buffer (no UTF-8 mangling)
    } else {
        hmac.update(body); // String body: works correctly as-is
    }
    const expected = hmac.digest('hex');

    // Validate signature is valid hex of correct length before timingSafeEqual
    if (!/^[0-9a-f]{64}$/i.test(signature)) {
        return { ok: false, reason: 'Bad signature' };
    }

    if (!crypto.timingSafeEqual(Buffer.from(signature, 'hex'), Buffer.from(expected, 'hex'))) {
        return { ok: false, reason: 'Bad signature' };
    }

    return { ok: true };
}

// ---- Helpers ----

function json(res, code, obj) {
    const body = JSON.stringify(obj);
    res.writeHead(code, {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(body),
    });
    res.end(body);
}

function readBody(req) {
    return new Promise((resolve, reject) => {
        let data = '';
        let size = 0;
        req.on('data', chunk => {
            size += chunk.length;
            if (size > MAX_BODY_SIZE) {
                req.destroy();
                reject(new Error('Body too large'));
                return;
            }
            data += chunk;
        });
        req.on('end', () => resolve(data));
        req.on('error', reject);
    });
}

/**
 * Parse JSON body, returning the parsed object or null on failure.
 * Sends a 400 response if parsing fails.
 */
function parseJsonBody(res, bodyStr) {
    try {
        return JSON.parse(bodyStr);
    } catch {
        json(res, 400, { error: 'Invalid JSON' });
        return null;
    }
}

/**
 * Check if a decline cooldown is active from one player to another.
 * Returns the remaining cooldown in seconds, or 0 if none.
 */
function getDeclineCooldown(fromId, toId) {
    const key = `${fromId}->${toId}`;
    const cd = declineCooldowns.get(key);
    if (!cd) return 0;
    const remaining = Math.max(0, Math.ceil((cd.until - Date.now()) / 1000));
    return remaining;
}

/**
 * Server-side connect matching: if player A wants to connect to player B,
 * automatically set B's connect_to = A's room_code so both sides see the
 * mutual intent on their next poll.
 *
 * Anti-spam: checks decline cooldowns before allowing the match.
 */
function resolveConnectMatch(player_id, display_name, room_code, connect_to) {
    if (!connect_to || !room_code) return;
    for (const [otherId, other] of players) {
        if (otherId === player_id) continue;
        if (other.room_code === connect_to) {
            // Anti-spam: check if target has declined this player recently
            const cooldown = getDeclineCooldown(otherId, player_id);
            if (cooldown > 0) {
                console.log(`[spam] blocked: ${display_name} -> ${other.display_name} (cooldown ${cooldown}s remaining)`);
                break;
            }

            // Safety: if this player already has a connect_to set to a *different* room,
            // don't overwrite it — could be a CGNAT/duplicate room_code collision.
            if (other.connect_to && other.connect_to !== room_code && other.connect_to !== '') {
                console.warn(`[match] skipped: ${other.display_name} already connecting to ${other.connect_to}`);
                break;
            }
            other.connect_to = String(room_code).slice(0, 63);
            console.log(`[match] ${display_name} -> ${other.display_name} (mutual connect_to set)`);
            break;
        }
    }
}

// ---- QR Join Helpers ----

/** Escape HTML special characters for safe output in generated pages. */
function escHtml(s) {
    return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

/**
 * Generate a styled mobile-friendly HTML page for the QR-join flow.
 * @param {string} title - Page heading
 * @param {string} message - HTML body content
 * @param {'success'|'error'|'picker'} type - Visual theme
 */
function qrJoinHtml(title, message, type) {
    const colors = {
        success: { bg: '#0d1117', accent: '#3fb950', border: '#238636' },
        error:   { bg: '#0d1117', accent: '#f85149', border: '#da3633' },
        picker:  { bg: '#0d1117', accent: '#58a6ff', border: '#1f6feb' },
    };
    const c = colors[type] || colors.error;
    return `<!DOCTYPE html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>3SXtra — ${escHtml(title)}</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:${c.bg};color:#c9d1d9;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Helvetica,Arial,sans-serif;
  display:flex;align-items:center;justify-content:center;min-height:100vh;padding:24px}
.card{background:#161b22;border:1px solid ${c.border};border-radius:12px;padding:32px;max-width:420px;width:100%;text-align:center}
h1{color:${c.accent};font-size:1.5rem;margin-bottom:12px}
p{font-size:1rem;line-height:1.5;color:#8b949e}
p strong{color:#c9d1d9}
.picker{display:flex;flex-direction:column;gap:10px;margin-top:20px}
.btn{display:block;padding:14px 20px;background:${c.border};color:#fff;text-decoration:none;
  border-radius:8px;font-size:1.1rem;font-weight:600;transition:background .15s}
.btn:hover{background:${c.accent}}
.logo{font-size:2rem;margin-bottom:8px}
</style></head><body>
<div class="card">
<div class="logo">🕹️</div>
<h1>${title}</h1>
<p>${message}</p>
</div></body></html>`;
}

/**
 * Push a REMOTE_JOIN SSE event to a specific player.
 * Tries the global SSE channel first, then falls back to any room SSE.
 */
function sendRemoteJoinEvent(playerId, roomCode, password) {
    const payload = JSON.stringify({ type: 'REMOTE_JOIN', data: { room: roomCode, password: password || '' } });
    const line = `data: ${payload}\n\n`;

    // Try global SSE first
    const globalRes = globalSseClients.get(playerId);
    if (globalRes) {
        try { globalRes.write(line); return; } catch { /* stale */ }
    }

    console.warn(`[qr-join] no global SSE for ${playerId} — event lost`);
}

// ---- Routes ----

async function handleRequest(req, res) {
    const url = new URL(req.url, `http://${req.headers.host}`);
    const urlPath = url.pathname;
    const fullPath = req.url; // includes query string — used for HMAC
    const method = req.method;

    // Read body for POST (except for replay uploads which we handle raw)
    let body;
    if (method === 'POST' && urlPath === '/match_result/replay') {
        try {
            // Read body as a raw Buffer for binary file upload
            body = await new Promise((resolve, reject) => {
                let chunks = [];
                let size = 0;
                req.on('data', chunk => {
                    size += chunk.length;
                    if (size > 1024 * 1024) { // 1MB max for replays
                        req.destroy();
                        reject(new Error('Replay file too large'));
                    }
                    chunks.push(chunk);
                });
                req.on('end', () => resolve(Buffer.concat(chunks)));
                req.on('error', reject);
            });
        } catch (err) {
            return json(res, 413, { error: 'Replay file too large' });
        }
    } else {
        try {
            body = method === 'POST' ? await readBody(req) : '';
        } catch (err) {
            return json(res, 413, { error: 'Request body too large' });
        }
    }

    // --- Health endpoint (no auth required) ---
    if (method === 'GET' && urlPath === '/') {
        return json(res, 200, {
            service: '3sx-lobby',
            players_online: players.size,
            players_searching: [...players.values()].filter(p => p.status === 'searching').length,
        });
    }

    // --- SSE Event Stream (no auth required, secured by unguessable room code) ---
    if (method === 'GET' && urlPath === '/room/events') {
        const roomCode = url.searchParams.get('room_code');
        const playerId = url.searchParams.get('player_id') || '';
        const room = rooms.get(roomCode);
        if (!room) return json(res, 404, { error: 'Room not found' });

        // Cancel any pending grace timer for this player (SSE reconnected in time)
        if (playerId) {
            const timerKey = `${roomCode}:${playerId}`;
            const existing = sseGraceTimers.get(timerKey);
            if (existing) {
                clearTimeout(existing);
                sseGraceTimers.delete(timerKey);
                console.log(`[room] SSE reconnected: ${playerId} in ${roomCode} (grace timer cancelled)`);
            }
        }

        res.writeHead(200, {
            'Content-Type': 'text/event-stream',
            'Cache-Control': 'no-cache',
            'Connection': 'keep-alive'
        });
        res.write(`data: ${JSON.stringify({ type: 'sync', data: getRoomState(room) })}\n\n`);

        room.sseClients.add(res);
        req.on('close', () => {
            room.sseClients.delete(res);

            // Implicit-disconnect-as-leave: start a 5s grace timer.
            // If the player doesn't reconnect SSE within this window,
            // treat it as an implicit leave from the room.
            if (playerId && room.players.includes(playerId)) {
                const timerKey = `${roomCode}:${playerId}`;
                const timer = setTimeout(() => {
                    sseGraceTimers.delete(timerKey);
                    const currentRoom = rooms.get(roomCode);
                    if (!currentRoom) return;
                    if (!currentRoom.players.includes(playerId)) return; // Already left
                    console.log(`[room] implicit leave: ${playerId} from ${roomCode} (SSE closed)`);
                    removePlayerFromRoom(currentRoom, playerId);
                }, 5000);
                sseGraceTimers.set(timerKey, timer);
            }
        });
        return; // Keep connection open
    }

    // --- Room State (read-only, no auth — secured by unguessable room code) ---
    if (method === 'GET' && urlPath === '/room/state') {
        const roomCode = url.searchParams.get('room_code');
        const room = rooms.get(roomCode);
        if (!room) return json(res, 404, { error: 'Room not found' });
        return json(res, 200, getRoomState(room));
    }

    // --- Room List (read-only, no auth — returns public room summaries) ---
    if (method === 'GET' && urlPath === '/rooms/list') {
        const list = [];
        for (const [code, room] of rooms) {
            if (room.visibility === 1) continue;

            list.push({
                code: room.id,
                name: room.name,
                player_count: room.players.length,
                max_players: room.max_players || 8,
                region_locked: !!(room.regions && room.regions.length > 0),
                ft: room.ft || 1,
                room_type: room.room_type || 0,
                password_required: room.password ? 1 : 0,
                visibility: room.visibility || 0,
                spectator_count: room.spectators ? room.spectators.size : 0
            });
        }
        return json(res, 200, { rooms: list });
    }

    // --- Global SSE (no auth — player_id is unguessable) ---
    // Provides a persistent event channel to clients not yet in a room.
    // Used by the QR-Join relay to push REMOTE_JOIN events.
    if (method === 'GET' && urlPath === '/sse') {
        const playerId = url.searchParams.get('player_id') || '';
        if (!playerId) return json(res, 400, { error: 'Missing player_id' });

        // Close any previous global SSE for this player (reconnect)
        const prev = globalSseClients.get(playerId);
        if (prev) { try { prev.end(); } catch { /* ignore */ } }

        res.writeHead(200, {
            'Content-Type': 'text/event-stream',
            'Cache-Control': 'no-cache',
            'Connection': 'keep-alive'
        });
        // Heartbeat comment to confirm connection
        res.write(': connected\n\n');

        globalSseClients.set(playerId, res);
        console.log(`[sse] global connected: ${playerId}`);

        req.on('close', () => {
            // Only delete if this response is still the current one
            if (globalSseClients.get(playerId) === res) {
                globalSseClients.delete(playerId);
                console.log(`[sse] global disconnected: ${playerId}`);
            }
        });
        return; // Keep connection open
    }

    // --- QR Join Relay (no auth — served to phones via scanned URL) ---
    if (method === 'GET' && urlPath === '/qr-join') {
        const roomCode = url.searchParams.get('room') || '';
        const password = url.searchParams.get('pw') || '';
        const phoneIp = (req.socket.remoteAddress || '').replace(/^::ffff:/, '');

        if (!roomCode) {
            res.writeHead(400, { 'Content-Type': 'text/html' });
            return res.end(qrJoinHtml('Error', 'Missing room code in URL.', 'error'));
        }

        // Find 3SXtra clients whose stored IP matches the phone's public IP
        const matchingClients = [];
        for (const [id, p] of players) {
            if (p.ip === phoneIp) {
                matchingClients.push({ id, display_name: p.display_name });
            }
        }

        if (matchingClients.length === 0) {
            console.log(`[qr-join] no match for IP ${phoneIp} (room=${roomCode})`);
            res.writeHead(200, { 'Content-Type': 'text/html' });
            return res.end(qrJoinHtml(
                'Could not find your PC',
                'Make sure your phone is connected to the <strong>same Wi-Fi</strong> as your PC running 3SXtra, then scan again.',
                'error'
            ));
        }

        if (matchingClients.length === 1) {
            const target = matchingClients[0];
            sendRemoteJoinEvent(target.id, roomCode, password);
            console.log(`[qr-join] direct relay to ${target.display_name} (${target.id}), room=${roomCode}`);
            res.writeHead(200, { 'Content-Type': 'text/html' });
            return res.end(qrJoinHtml(
                'Success!',
                `Joining room <strong>${roomCode}</strong> on <strong>${target.display_name}</strong>&rsquo;s PC. Check your screen!`,
                'success'
            ));
        }

        // Multiple clients on the same IP — show picker
        const picked = url.searchParams.get('pick');
        if (picked) {
            const target = matchingClients.find(c => c.id === picked);
            if (target) {
                sendRemoteJoinEvent(target.id, roomCode, password);
                console.log(`[qr-join] picked relay to ${target.display_name} (${target.id}), room=${roomCode}`);
                res.writeHead(200, { 'Content-Type': 'text/html' });
                return res.end(qrJoinHtml(
                    'Success!',
                    `Joining room <strong>${roomCode}</strong> on <strong>${target.display_name}</strong>&rsquo;s PC.`,
                    'success'
                ));
            }
        }

        // Disambiguation: ask which client
        const buttons = matchingClients.map(c =>
            `<a class="btn" href="/qr-join?room=${encodeURIComponent(roomCode)}&pw=${encodeURIComponent(password)}&pick=${encodeURIComponent(c.id)}">${escHtml(c.display_name)}</a>`
        ).join('');
        console.log(`[qr-join] ${matchingClients.length} clients on IP ${phoneIp} — showing picker`);
        res.writeHead(200, { 'Content-Type': 'text/html' });
        return res.end(qrJoinHtml(
            'Multiple players found',
            `We found ${matchingClients.length} players on your network. Which screen are you on?<div class="picker">${buttons}</div>`,
            'picker'
        ));
    }

    // Auth check (all other endpoints)
    const auth = verifyRequest(method, fullPath, body, req.headers);
    if (!auth.ok) {
        return json(res, 403, { error: auth.reason });
    }

    // --- Casual Rooms (8-Player) ---

    if (method === 'POST' && urlPath === '/room/create') {
        const data = parseJsonBody(res, body);
        if (!data) return;
        const code = generateRoomCode();
        const roomName = String(data.name || `${data.display_name}'s Room`).slice(0, 31);
        const hostId = data.player_id;
        const roomType = typeof data.room_type === 'number' ? data.room_type : 0;

        const roomFt = typeof data.ft === 'number' ? Math.max(1, Math.min(10, data.ft)) : 2;
        const maxPlayers = typeof data.max_players === 'number' ? Math.max(2, Math.min(16, data.max_players)) : 8;
        const password = typeof data.password === 'string' ? data.password : '';
        const visibility = typeof data.visibility === 'number' ? data.visibility : 0;

        const roomObj = {
            id: code,
            name: roomName,
            host: hostId,
            players: [hostId],
            queue: [], // Next in line for cabinet
            match: null, // { p1: 'id', p2: 'id', state: 'playing' }
            chat: [],
            spectators: new Set(),
            sseClients: new Set(),
            ft: roomFt,
            room_type: roomType,
            max_players: maxPlayers,
            password: password,
            visibility: visibility
        };

        // Tournament-specific fields
        if (roomType === 2) {
            roomObj.tournament = {
                format: typeof data.tournament_format === 'number' ? data.tournament_format : 0,
                seeding: data.seeding || 'rating',
                started: false,
                paused: false,
                round: 0,
                total_rounds: 0,
                bracket: [],   // BracketEntry[]
                matches: [],   // active RoomMatch[] for current round
                dq_players: [] // DQ'd player IDs
            };
        }

        rooms.set(code, roomObj);
        const typeLabel = ['casual', 'koth', 'tournament'][roomType] || 'casual';
        console.log(`[room] ${hostId} created ${typeLabel} room ${code}: ${roomName} (FT${roomFt}, max=${maxPlayers})`);
        return json(res, 200, { ok: true, room_code: code });
    }

    if (method === 'POST' && urlPath === '/room/join') {
        const data = parseJsonBody(res, body);
        if (!data) return;
        const room = rooms.get(data.room_code);
        if (!room) return json(res, 404, { error: 'Room not found' });
        if (room.players.length >= (room.max_players || 8)) return json(res, 400, { error: 'Room is full' });

        if (room.password) {
            const joinPassword = typeof data.password === 'string' ? data.password : '';
            if (joinPassword !== room.password) {
                return json(res, 403, { error: 'Wrong password' });
            }
        }

        // Region gate: permanent rooms restrict to their declared regions
        if (room.regions && room.regions.length > 0) {
            const clientIp = req.socket.remoteAddress || '';
            const { region: playerRegion } = detectRegionAndCountry(clientIp);
            if (!playerRegion || !room.regions.includes(playerRegion)) {
                console.log(`[room] region rejected: ${data.player_id} (${playerRegion || 'unknown'}) tried to join ${room.id} (${room.regions.join(',')})`);
                return json(res, 403, {
                    error: 'Region restricted',
                    your_region: playerRegion || 'unknown',
                    allowed_regions: room.regions
                });
            }
        }

        if (!room.players.includes(data.player_id)) {
            room.players.push(data.player_id);
            broadcastRoomEvent(room, 'join', { player_id: data.player_id, display_name: data.display_name });
            console.log(`[room] ${data.player_id} joined room ${room.id}`);
        }
        return json(res, 200, { ok: true, room });
    }

    if (method === 'POST' && urlPath === '/room/leave') {
        const data = parseJsonBody(res, body);
        if (!data) return;
        const room = rooms.get(data.room_code);
        if (!room) return json(res, 404, { error: 'Room not found' });

        // If leaving player is in an active or proposed match, handle it
        if (room.match && (room.match.state === 'playing' || room.match.state === 'proposed') &&
            (room.match.p1 === data.player_id || room.match.p2 === data.player_id)) {
            const other = room.match.p1 === data.player_id ? room.match.p2 : room.match.p1;
            if (room.match.state === 'proposed') {
                // Cancel proposal — other player stays at front of queue
                room.match = null;
                room.queue.unshift(other);
                broadcastRoomEvent(room, 'match_decline', {
                    p1: { id: data.player_id, name: getPlayerName(data.player_id) },
                    p2: { id: other, name: getPlayerName(other) },
                    decliner_id: data.player_id, reason: 'disconnect'
                });
                console.log(`[room] proposal cancelled in ${room.id}: ${data.player_id} left`);
            } else {
                broadcastRoomEvent(room, 'match_end', {
                    winner_id: other, winner_name: getPlayerName(other),
                    loser_id: data.player_id, reason: 'disconnect'
                });
                // Winner goes to front of queue
                room.queue.unshift(other);
                room.match = null;
                console.log(`[room] match forfeited in ${room.id}: ${data.player_id} left`);
            }
        }

        room.players = room.players.filter(p => p !== data.player_id);
        room.queue = room.queue.filter(p => p !== data.player_id);

        if (room.players.length === 0) {
            if (room.permanent) {
                console.log(`[room] ${room.id} empty but kept (permanent)`);
            } else {
                console.log(`[room] ${room.id} closed (empty)`);
                rooms.delete(room.id);
            }
        } else {
            if (room.host === data.player_id) {
                room.host = room.players[0]; // Migrate host
                broadcastRoomEvent(room, 'host_migrated', { host: room.host });
            }
            broadcastRoomEvent(room, 'leave', { player_id: data.player_id });
            console.log(`[room] ${data.player_id} left room ${room.id}`);
            // Try to start next match if previous was forfeited
            tryStartMatch(room);
        }
        return json(res, 200, { ok: true });
    }

    if (method === 'POST' && urlPath === '/room/chat') {
        const data = parseJsonBody(res, body);
        if (!data) return;
        const room = rooms.get(data.room_code);
        if (!room) return json(res, 404, { error: 'Room not found' });

        const player = players.get(data.player_id);
        if (!player) return json(res, 404, { error: 'Player not found' });

        const now = Date.now();
        // 3-second rate limit
        if (player.last_chat_time && (now - player.last_chat_time < 3000)) {
            return json(res, 429, { error: 'Rate limit exceeded (1 message every 3 seconds)' });
        }
        player.last_chat_time = now;

        let chatText = String(data.text).slice(0, 120);
        
        // Apply profanity filter if loaded
        if (badWordsFilter) {
            try {
                chatText = badWordsFilter.clean(chatText);
            } catch (err) {
                // Ignore parsing errors and allow original string if clean fails
                console.error('[chat] Profanity filter failed:', err);
            }
        }

        const msg = {
            id: now,
            sender_id: data.player_id,
            sender_name: data.display_name,
            text: chatText
        };
        
        // Instead of storing in room.chat, it is fully ephemeral.
        broadcastRoomEvent(room, 'chat', msg);
        return json(res, 200, { ok: true });
    }

    if (method === 'POST' && urlPath === '/room/queue/join') {
        const data = parseJsonBody(res, body);
        if (!data) return;
        const room = rooms.get(data.room_code);
        if (!room) return json(res, 404, { error: 'Room not found' });

        // Don't allow joining queue if already in active or proposed match
        if (room.match && (room.match.state === 'playing' || room.match.state === 'proposed') &&
            (room.match.p1 === data.player_id || room.match.p2 === data.player_id)) {
            return json(res, 400, { error: 'Cannot join queue while in active match' });
        }

        if (!room.queue.includes(data.player_id)) {
            room.queue.push(data.player_id);
            broadcastRoomEvent(room, 'queue_update', { queue: room.queue });
            console.log(`[room] ${data.player_id} joined queue in room ${room.id}`);

            // Auto-start match if conditions are met
            tryStartMatch(room);
        }
        return json(res, 200, { ok: true });
    }

    if (method === 'POST' && urlPath === '/room/queue/leave') {
        const data = parseJsonBody(res, body);
        if (!data) return;
        const room = rooms.get(data.room_code);
        if (!room) return json(res, 404, { error: 'Room not found' });

        if (room.queue.includes(data.player_id)) {
            room.queue = room.queue.filter(p => p !== data.player_id);
            broadcastRoomEvent(room, 'queue_update', { queue: room.queue });
            console.log(`[room] ${data.player_id} left queue in room ${room.id}`);
        }
        return json(res, 200, { ok: true });
    }

    // --- POST /room/match/accept --- Phase 6: Accept a proposed match
    if (method === 'POST' && urlPath === '/room/match/accept') {
        const data = parseJsonBody(res, body);
        if (!data) return;
        const room = rooms.get(data.room_code);
        if (!room) return json(res, 404, { error: 'Room not found' });

        const { player_id } = data;

        // Tournament rooms: accept by match_index (parallel matches)
        if (room.room_type === 2 && room.tournament && room.tournament.matches.length > 0) {
            const matchIndex = typeof data.match_index === 'number' ? data.match_index : 0;
            const tMatch = room.tournament.matches.find(m => m.match_index === matchIndex);
            if (!tMatch || tMatch.state !== 'proposed') {
                return json(res, 400, { error: 'No proposed match at this index' });
            }
            if (player_id !== tMatch.p1 && player_id !== tMatch.p2) {
                return json(res, 400, { error: 'Not a match participant' });
            }
            tMatch.accepts[player_id] = true;
            console.log(`[tournament] ${getPlayerName(player_id)} accepted match ${matchIndex} in ${room.id}`);

            let match_started = false;
            if (tMatch.accepts[tMatch.p1] && tMatch.accepts[tMatch.p2]) {
                tMatch.state = 'playing';
                tMatch.active = true;
                broadcastRoomEvent(room, 'match_start', {
                    match_index: matchIndex,
                    p1: { id: tMatch.p1, name: getPlayerName(tMatch.p1) },
                    p2: { id: tMatch.p2, name: getPlayerName(tMatch.p2) }
                });
                console.log(`[tournament] match ${matchIndex} confirmed in ${room.id}`);
                match_started = true;
            }
            return json(res, 200, { ok: true, match_started });
        }

        // Casual/KOTH rooms: single match
        if (!room.match || room.match.state !== 'proposed') {
            return json(res, 400, { error: 'No proposed match' });
        }
        if (player_id !== room.match.p1 && player_id !== room.match.p2) {
            return json(res, 400, { error: 'Not a match participant' });
        }

        room.match.accepts[player_id] = true;
        console.log(`[room] ${getPlayerName(player_id)} accepted match in ${room.id}`);

        // Check if both accepted
        let match_started = false;
        if (room.match.accepts[room.match.p1] && room.match.accepts[room.match.p2]) {
            confirmMatch(room);
            match_started = true;
        }
        return json(res, 200, { ok: true, match_started });
    }

    // --- POST /room/match/decline --- Phase 6: Decline a proposed match
    if (method === 'POST' && urlPath === '/room/match/decline') {
        const data = parseJsonBody(res, body);
        if (!data) return;
        const room = rooms.get(data.room_code);
        if (!room) return json(res, 404, { error: 'Room not found' });
        if (!room.match || room.match.state !== 'proposed') {
            return json(res, 400, { error: 'No proposed match' });
        }

        const { player_id } = data;
        if (player_id !== room.match.p1 && player_id !== room.match.p2) {
            return json(res, 400, { error: 'Not a match participant' });
        }

        cancelProposal(room, player_id);
        return json(res, 200, { ok: true });
    }

    // --- POST /room/match/end --- "Winner Stays On" rotation / Tournament bracket advancement
    if (method === 'POST' && urlPath === '/room/match/end') {
        const data = parseJsonBody(res, body);
        if (!data) return;
        const room = rooms.get(data.room_code);
        if (!room) return json(res, 404, { error: 'Room not found' });

        const { winner_id } = data;
        const matchIndex = typeof data.match_index === 'number' ? data.match_index : 0;

        // Tournament rooms: bracket-aware match end
        if (room.room_type === 2 && room.tournament) {
            const t = room.tournament;
            const tMatch = t.matches.find(m => m.match_index === matchIndex);
            if (!tMatch) {
                return json(res, 400, { error: 'No match at this index' });
            }
            if (winner_id !== tMatch.p1 && winner_id !== tMatch.p2) {
                return json(res, 400, { error: 'winner_id must be p1 or p2' });
            }

            const loser_id = winner_id === tMatch.p1 ? tMatch.p2 : tMatch.p1;
            tMatch.state = 'completed';
            tMatch.active = false;

            // Advance bracket
            let roundComplete = false;
            const bracketSide = tMatch.bracket_side || '';

            if (t.format === 0) {
                roundComplete = advanceSingleElimBracket(t, tMatch.bracket_round, tMatch.bracket_position, winner_id);
            } else if (t.format === 1) {
                roundComplete = advanceDoubleElimBracket(t, bracketSide, tMatch.bracket_round, tMatch.bracket_position, winner_id);
            } else {
                // Round-robin / Swiss: just mark the bracket entry
                const entry = t.bracket.find(b => b.round === tMatch.bracket_round && b.position === tMatch.bracket_position);
                if (entry) {
                    entry.winner_id = winner_id;
                    entry.completed = 1;
                }
                const roundEntries = t.bracket.filter(b => b.round === t.round);
                roundComplete = roundEntries.length > 0 && roundEntries.every(b => b.completed);
            }

            broadcastRoomEvent(room, 'match_end', {
                winner_id, winner_name: getPlayerName(winner_id),
                loser_id, loser_name: getPlayerName(loser_id),
                match_index: matchIndex
            });
            broadcastRoomEvent(room, 'bracket_update', getTournamentState(room));
            console.log(`[tournament] match ${matchIndex} ended in ${room.id}: ${getPlayerName(winner_id)} wins`);

            if (t.format === 1) {
                // Double Elim: tournament is over when GF is completed
                const gf = t.bracket.find(b => b.bracket_side === 'GF');
                if (gf && gf.completed) {
                    console.log(`[tournament] TOURNAMENT COMPLETE in ${room.id} (double elim)`);
                } else {
                    // Propose any new ready matches (losers may have been populated)
                    const allMatchesDone = t.matches.every(m => m.state === 'completed');
                    if (allMatchesDone) {
                        tryStartTournamentRound(room);
                    }
                }
            } else if (roundComplete) {
                const allMatchesDone = t.matches.every(m => m.state === 'completed');
                if (allMatchesDone) {
                    const isFinalRound = (t.format === 0)
                        ? t.round >= t.total_rounds - 1
                        : (t.format === 2)
                            ? t.round >= (room.players.filter(p => !t.dq_players.includes(p)).length - 1) - 1
                            : t.round >= Math.ceil(Math.log2(room.players.length)) - 1;

                    if (isFinalRound) {
                        console.log(`[tournament] TOURNAMENT COMPLETE in ${room.id}`);
                    } else {
                        t.round++;
                        console.log(`[tournament] round complete in ${room.id}, advancing to round ${t.round}`);
                        broadcastRoomEvent(room, 'round_advance', { round: t.round });
                        tryStartTournamentRound(room);
                    }
                }
            }

            return json(res, 200, { ok: true });
        }

        // Casual/KOTH rooms: "Winner Stays On" rotation
        if (!room.match || room.match.state !== 'playing') {
            return json(res, 400, { error: 'No active match' });
        }

        const { p1, p2 } = room.match;

        // Validate winner is one of the match players
        if (winner_id !== p1 && winner_id !== p2) {
            return json(res, 400, { error: 'winner_id must be p1 or p2' });
        }

        const loser_id = winner_id === p1 ? p2 : p1;

        // Clear match
        room.match = null;

        // Rotation: winner to front of queue, loser to back
        room.queue.unshift(winner_id);
        room.queue.push(loser_id);

        broadcastRoomEvent(room, 'match_end', {
            winner_id, winner_name: getPlayerName(winner_id),
            loser_id, loser_name: getPlayerName(loser_id)
        });
        console.log(`[room] match ended in ${room.id}: ${getPlayerName(winner_id)} wins (stays on)`);

        // Auto-start next match
        tryStartMatch(room);

        return json(res, 200, { ok: true });
    }

    // --- Tournament Bracket Endpoints ---

    // POST /room/:code/bracket/start — Close registration, generate bracket
    const bracketStartMatch = urlPath.match(/^\/room\/([^/]+)\/bracket\/start$/);
    if (method === 'POST' && bracketStartMatch) {
        const roomCode = bracketStartMatch[1];
        const data = parseJsonBody(res, body);
        if (!data) return;
        const room = rooms.get(roomCode);
        if (!room) return json(res, 404, { error: 'Room not found' });
        if (room.room_type !== 2 || !room.tournament) {
            return json(res, 400, { error: 'Not a tournament room' });
        }
        if (data.player_id !== room.host) {
            return json(res, 403, { error: 'Only the tournament organizer can start the bracket' });
        }
        if (room.tournament.started) {
            return json(res, 400, { error: 'Bracket already started' });
        }
        if (room.players.length < 2) {
            return json(res, 400, { error: 'Need at least 2 players' });
        }

        const t = room.tournament;
        t.started = true;

        // Seed players
        const seeded = seedPlayers(room.players, t.seeding);

        if (t.format === 0) {
            // Single Elim
            const result = generateSingleElimBracket(seeded);
            t.bracket = result.bracket;
            t.total_rounds = result.total_rounds;
            t.round = 0;

            // Advance BYEs in round 0 before proposing matches
            for (const entry of t.bracket.filter(b => b.round === 0 && b.completed && b.winner_id)) {
                advanceSingleElimBracket(t, 0, entry.position, entry.winner_id);
            }
        } else if (t.format === 1) {
            // Double Elim
            const result = generateDoubleElimBracket(seeded);
            t.bracket = result.bracket;
            t.total_rounds = result.total_rounds;
            t.w_rounds = result.w_rounds;
            t.l_rounds = result.l_rounds;
            t.round = 0;

            // Advance BYEs in winners round 0
            for (const entry of t.bracket.filter(b => b.bracket_side === 'W' && b.round === 0 && b.completed && b.winner_id)) {
                advanceDoubleElimBracket(t, 'W', 0, entry.position, entry.winner_id);
            }
        } else if (t.format === 2) {
            // Round Robin: total rounds = N-1 (or N for odd)
            const n = seeded.length % 2 === 0 ? seeded.length : seeded.length + 1;
            t.total_rounds = n - 1;
            t.round = 0;
        } else if (t.format === 3) {
            // Swiss: total rounds = ceil(log2(N))
            t.total_rounds = Math.ceil(Math.log2(seeded.length));
            t.round = 0;
        }

        console.log(`[tournament] bracket started in ${room.id}: ${seeded.length} players, ${t.total_rounds} rounds`);

        // Broadcast bracket state
        broadcastRoomEvent(room, 'bracket_update', getTournamentState(room));

        // Fire first round matches
        tryStartTournamentRound(room);

        return json(res, 200, { ok: true, bracket: getTournamentState(room) });
    }

    // GET /room/:code/bracket — Fetch current bracket state
    const bracketGetMatch = urlPath.match(/^\/room\/([^/]+)\/bracket$/);
    if (method === 'GET' && bracketGetMatch) {
        const roomCode = bracketGetMatch[1];
        const room = rooms.get(roomCode);
        if (!room) return json(res, 404, { error: 'Room not found' });
        if (room.room_type !== 2 || !room.tournament) {
            return json(res, 400, { error: 'Not a tournament room' });
        }
        return json(res, 200, getTournamentState(room));
    }

    // POST /room/:code/bracket/override — TO result override
    const bracketOverrideMatch = urlPath.match(/^\/room\/([^/]+)\/bracket\/override$/);
    if (method === 'POST' && bracketOverrideMatch) {
        const roomCode = bracketOverrideMatch[1];
        const data = parseJsonBody(res, body);
        if (!data) return;
        const room = rooms.get(roomCode);
        if (!room) return json(res, 404, { error: 'Room not found' });
        if (room.room_type !== 2 || !room.tournament) {
            return json(res, 400, { error: 'Not a tournament room' });
        }
        if (data.player_id !== room.host) {
            return json(res, 403, { error: 'Only the tournament organizer can override results' });
        }

        const t = room.tournament;
        const { match_index, winner_id } = data;
        if (typeof match_index !== 'number' || !winner_id) {
            return json(res, 400, { error: 'Missing match_index or winner_id' });
        }

        // Find the bracket entry by scanning for matching round+position or by match index
        // For single elim: match_index maps to bracket entries in current round
        const entry = t.bracket.find((b, idx) => idx === match_index) ||
                      t.bracket.find(b => !b.completed && (b.player1_id === winner_id || b.player2_id === winner_id));
        if (!entry) {
            return json(res, 404, { error: 'Bracket entry not found' });
        }
        if (winner_id !== entry.player1_id && winner_id !== entry.player2_id) {
            return json(res, 400, { error: 'winner_id must be a player in this match' });
        }

        // Apply override
        if (t.format === 0) {
            advanceSingleElimBracket(t, entry.round, entry.position, winner_id);
        } else if (t.format === 1) {
            advanceDoubleElimBracket(t, entry.bracket_side || 'W', entry.round, entry.position, winner_id);
        } else {
            entry.winner_id = winner_id;
            entry.completed = 1;
        }

        // Also mark tournament match as completed if it exists
        const tMatch = t.matches.find(m => m.bracket_round === entry.round && m.bracket_position === entry.position);
        if (tMatch) {
            tMatch.state = 'completed';
            tMatch.active = false;
        }

        broadcastRoomEvent(room, 'bracket_update', getTournamentState(room));
        console.log(`[tournament] TO override in ${room.id}: match at round ${entry.round} pos ${entry.position} -> winner ${getPlayerName(winner_id)}`);
        return json(res, 200, { ok: true });
    }

    // POST /room/:code/bracket/dq — DQ a player
    const bracketDQMatch = urlPath.match(/^\/room\/([^/]+)\/bracket\/dq$/);
    if (method === 'POST' && bracketDQMatch) {
        const roomCode = bracketDQMatch[1];
        const data = parseJsonBody(res, body);
        if (!data) return;
        const room = rooms.get(roomCode);
        if (!room) return json(res, 404, { error: 'Room not found' });
        if (room.room_type !== 2 || !room.tournament) {
            return json(res, 400, { error: 'Not a tournament room' });
        }
        if (data.player_id !== room.host) {
            return json(res, 403, { error: 'Only the tournament organizer can DQ players' });
        }

        const t = room.tournament;
        const targetId = data.target_player_id;
        if (!targetId) {
            return json(res, 400, { error: 'Missing target_player_id' });
        }
        if (t.dq_players.includes(targetId)) {
            return json(res, 400, { error: 'Player already DQ\'d' });
        }

        t.dq_players.push(targetId);

        // Auto-advance opponent in any pending/active match involving the DQ'd player
        for (const entry of t.bracket) {
            if (entry.completed) continue;
            if (entry.player1_id === targetId && entry.player2_id) {
                if (t.format === 0) {
                    advanceSingleElimBracket(t, entry.round, entry.position, entry.player2_id);
                } else if (t.format === 1) {
                    advanceDoubleElimBracket(t, entry.bracket_side || 'W', entry.round, entry.position, entry.player2_id);
                } else {
                    entry.winner_id = entry.player2_id;
                    entry.completed = 1;
                }
            } else if (entry.player2_id === targetId && entry.player1_id) {
                if (t.format === 0) {
                    advanceSingleElimBracket(t, entry.round, entry.position, entry.player1_id);
                } else if (t.format === 1) {
                    advanceDoubleElimBracket(t, entry.bracket_side || 'W', entry.round, entry.position, entry.player1_id);
                } else {
                    entry.winner_id = entry.player1_id;
                    entry.completed = 1;
                }
            }
        }

        // Cancel any proposed tournament match involving this player
        for (const m of t.matches) {
            if (m.state === 'proposed' && (m.p1 === targetId || m.p2 === targetId)) {
                m.state = 'completed';
                m.active = false;
            }
        }

        broadcastRoomEvent(room, 'bracket_update', getTournamentState(room));
        console.log(`[tournament] DQ in ${room.id}: ${getPlayerName(targetId)} disqualified`);
        return json(res, 200, { ok: true });
    }

    // POST /room/:code/bracket/pause — Pause or resume tournament
    const bracketPauseMatch = urlPath.match(/^\/room\/([^/]+)\/bracket\/pause$/);
    if (method === 'POST' && bracketPauseMatch) {
        const roomCode = bracketPauseMatch[1];
        const data = parseJsonBody(res, body);
        if (!data) return;
        const room = rooms.get(roomCode);
        if (!room) return json(res, 404, { error: 'Room not found' });
        if (room.room_type !== 2 || !room.tournament) {
            return json(res, 400, { error: 'Not a tournament room' });
        }
        if (data.player_id !== room.host) {
            return json(res, 403, { error: 'Only the tournament organizer can pause/resume' });
        }

        const pause = data.pause !== undefined ? !!data.pause : true;
        room.tournament.paused = pause;
        broadcastRoomEvent(room, 'bracket_update', getTournamentState(room));
        console.log(`[tournament] ${room.id} ${pause ? 'PAUSED' : 'RESUMED'}`);

        // If resuming, try to start any pending round
        if (!pause && room.tournament.started) {
            const hasActiveMatches = room.tournament.matches.some(m => m.state === 'proposed' || m.state === 'playing');
            if (!hasActiveMatches) {
                tryStartTournamentRound(room);
            }
        }
        return json(res, 200, { ok: true, paused: pause });
    }

    // POST /room/:code/bracket/restart — Restart a match
    const bracketRestartMatch = urlPath.match(/^\/room\/([^/]+)\/bracket\/restart$/);
    if (method === 'POST' && bracketRestartMatch) {
        const roomCode = bracketRestartMatch[1];
        const data = parseJsonBody(res, body);
        if (!data) return;
        const room = rooms.get(roomCode);
        if (!room) return json(res, 404, { error: 'Room not found' });
        if (room.room_type !== 2 || !room.tournament) {
            return json(res, 400, { error: 'Not a tournament room' });
        }
        if (data.player_id !== room.host) {
            return json(res, 403, { error: 'Only the tournament organizer can restart matches' });
        }

        const t = room.tournament;
        const { match_index } = data;
        if (typeof match_index !== 'number') {
            return json(res, 400, { error: 'Missing match_index' });
        }

        const tMatch = t.matches.find(m => m.match_index === match_index);
        if (!tMatch) {
            return json(res, 404, { error: 'Tournament match not found in active pool' });
        }

        const entry = t.bracket.find(b => b.round === tMatch.bracket_round && b.position === tMatch.bracket_position);
        if (!entry) {
            return json(res, 404, { error: 'Bracket entry not found' });
        }

        if (!entry.player1_id || !entry.player2_id) {
            return json(res, 400, { error: 'Cannot restart a match missing players' });
        }

        // Reset the bracket entry
        entry.completed = 0;
        entry.winner_id = '';

        // Reset existing match object
        tMatch.state = 'proposed';
        tMatch.active = false;
        tMatch.accepts = { [entry.player1_id]: false, [entry.player2_id]: false };
        tMatch.proposed_at = Date.now();

        // Read player connection info to build the proposal correctly
        const p1_data = players.get(entry.player1_id);
        const p2_data = players.get(entry.player2_id);
        
        broadcastRoomEvent(room, 'match_propose', {
            ft: room.ft || 1,
            match_index: match_index,
            bracket_side: entry.bracket_side || '',
            p1: {
                id: entry.player1_id, name: entry.player1_name || getPlayerName(entry.player1_id),
                connection_type: p1_data ? p1_data.connection_type : 'unknown',
                rtt_ms: p1_data ? p1_data.rtt_ms : -1,
                region: p1_data ? p1_data.region : '',
                room_code: p1_data ? p1_data.room_code : ''
            },
            p2: {
                id: entry.player2_id, name: entry.player2_name || getPlayerName(entry.player2_id),
                connection_type: p2_data ? p2_data.connection_type : 'unknown',
                rtt_ms: p2_data ? p2_data.rtt_ms : -1,
                region: p2_data ? p2_data.region : '',
                room_code: p2_data ? p2_data.room_code : ''
            }
        });

        broadcastRoomEvent(room, 'bracket_update', getTournamentState(room));
        console.log(`[tournament] match ${match_index} RESTARTED in ${room.id} (${getPlayerName(entry.player1_id)} vs ${getPlayerName(entry.player2_id)})`);
        return json(res, 200, { ok: true });
    }

    // --- Spectator Tracking ---
    const spectateMatch = urlPath.match(/^\/room\/([^/]+)\/spectate$/);
    if (spectateMatch && (method === 'POST' || method === 'DELETE')) {
        const roomCode = spectateMatch[1];
        const data = parseJsonBody(res, body);
        if (!data) return;
        const room = rooms.get(roomCode);
        if (!room) return json(res, 404, { error: 'Room not found' });
        const { player_id } = data;
        if (!player_id) return json(res, 400, { error: 'Missing player_id' });

        if (method === 'POST') {
            room.spectators.add(player_id);
        } else {
            room.spectators.delete(player_id);
        }
        broadcastRoomEvent(room, 'spectator_update', { count: room.spectators.size });
        console.log(`[room] spectator ${method === 'POST' ? 'start' : 'stop'}: ${player_id} in ${roomCode} (count=${room.spectators.size})`);
        return json(res, 200, { ok: true, spectator_count: room.spectators.size });
    }

    // --- / Casual Rooms ---

    // --- POST /presence ---
    if (method === 'POST' && urlPath === '/presence') {
        const data = parseJsonBody(res, body);
        if (!data) return;

        const { player_id, display_name, region, room_code, connect_to, rtt_ms, connection_type, ft } = data;
        if (!player_id || !display_name) {
            return json(res, 400, { error: 'Missing player_id or display_name' });
        }

        // GeoIP: detect country and region from source IP
        const clientIp = req.socket.remoteAddress || '';
        const geo = detectRegionAndCountry(clientIp);

        const existing = players.get(player_id);
        players.set(player_id, {
            display_name: String(display_name).slice(0, 31),
            region: String(region || geo.region || '').slice(0, 7),
            country: geo.country || (existing ? existing.country : ''),
            room_code: String(room_code || '').slice(0, 63),
            connect_to: String(connect_to || '').slice(0, 63),
            status: existing ? existing.status : 'idle',
            connection_type: String(connection_type || 'unknown').slice(0, 7),
            rtt_ms: typeof rtt_ms === 'number' ? Math.max(0, Math.min(9999, rtt_ms)) : (existing ? existing.rtt_ms : -1),
            ft: typeof ft === 'number' ? Math.max(1, Math.min(10, ft)) : (existing ? existing.ft : 2),
            last_seen: Date.now(),
            last_chat_time: existing ? existing.last_chat_time : 0,
            ip: clientIp.replace(/^::ffff:/, ''),
        });

        resolveConnectMatch(player_id, display_name, room_code, connect_to);
        return json(res, 200, { ok: true });
    }

    // --- POST /searching/start ---
    if (method === 'POST' && urlPath === '/searching/start') {
        const data = parseJsonBody(res, body);
        if (!data) return;

        let p = players.get(data.player_id);
        if (!p) {
            // Create minimal entry if presence hasn't arrived yet (race condition fix)
            p = { display_name: data.player_id, region: '', country: '', room_code: '', connect_to: '', status: 'idle', connection_type: 'unknown', rtt_ms: -1, ft: 2, last_seen: Date.now(), last_chat_time: 0 };
            players.set(data.player_id, p);
        }

        p.status = 'searching';
        p.last_seen = Date.now();
        return json(res, 200, { ok: true });
    }

    // --- POST /searching/stop ---
    if (method === 'POST' && urlPath === '/searching/stop') {
        const data = parseJsonBody(res, body);
        if (!data) return;

        const p = players.get(data.player_id);
        if (!p) return json(res, 404, { error: 'Player not found' });

        p.status = 'idle';
        p.last_seen = Date.now();
        return json(res, 200, { ok: true });
    }

    // --- GET /searching ---
    // Returns players that are either searching or have an active connect_to.
    // The connect_to filter is needed so clients can detect incoming invites.
    if (method === 'GET' && urlPath === '/searching') {
        const regionFilter = url.searchParams.get('region');
        const result = [];

        for (const [id, p] of players) {
            // Only include players that are searching OR have an active connection intent
            if (p.status !== 'searching' && !p.connect_to) continue;
            if (regionFilter && p.region !== regionFilter) continue;
            result.push({
                player_id: id,
                display_name: p.display_name,
                region: p.region,
                country: p.country || '',
                room_code: p.room_code,
                connect_to: p.connect_to || '',
                rtt_ms: p.rtt_ms || -1,
                status: p.status || 'idle',
                connection_type: p.connection_type || 'unknown',
                ft: p.ft || 2,
            });
        }

        return json(res, 200, { players: result });
    }

    // --- POST /decline ---
    // Report a declined invite for rate limiting.
    // Implements exponential backoff: 30s -> 60s -> 120s -> 300s max.
    if (method === 'POST' && urlPath === '/decline') {
        const data = parseJsonBody(res, body);
        if (!data) return;

        const { player_id, declined_player_id } = data;
        if (!player_id || !declined_player_id) {
            return json(res, 400, { error: 'Missing player_id or declined_player_id' });
        }

        const key = `${player_id}->${declined_player_id}`;
        const existing = declineCooldowns.get(key);
        const count = existing ? existing.count + 1 : 1;

        // Exponential backoff: 30s, 60s, 120s, 300s max
        const baseCooldown = 30_000;
        const cooldownMs = Math.min(baseCooldown * Math.pow(2, count - 1), 300_000);

        declineCooldowns.set(key, {
            count,
            until: Date.now() + cooldownMs,
        });

        const cooldownSeconds = Math.ceil(cooldownMs / 1000);
        console.log(`[decline] ${player_id} declined ${declined_player_id} (count=${count}, cooldown=${cooldownSeconds}s)`);

        return json(res, 200, { ok: true, cooldown_seconds: cooldownSeconds });
    }

    // --- POST /match_result ---
    if (method === 'POST' && urlPath === '/match_result') {
        if (!db) return json(res, 503, { error: 'Match reporting unavailable (no SQLite)' });

        const data = parseJsonBody(res, body);
        if (!data) return;

        const { player_id, opponent_id, winner_id, player_char, opponent_char, rounds } = data;
        const source = data.source || 'ranked';
        const ft = Math.max(1, Math.min(data.ft || 1, 10));
        if (!player_id || !opponent_id || !winner_id) {
            return json(res, 400, { error: 'Missing player_id, opponent_id, or winner_id' });
        }

        // Canonical match key: sorted IDs joined by ":"
        const ids = [player_id, opponent_id].sort();
        const matchKey = ids.join(':');

        const pending = db.prepare('SELECT * FROM pending_results WHERE match_key = ?').get(matchKey);

        if (!pending) {
            // First report for this game — store claim, session wins start at 0
            // (wins only count after cross-validation by opponent)
            db.prepare(`INSERT OR REPLACE INTO pending_results
                        (match_key, reporter_id, winner_id, p1_id, p2_id, p1_char, p2_char, rounds, source, ft, p1_session_wins, p2_session_wins)
                        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`)
              .run(matchKey, player_id, winner_id, ids[0], ids[1],
                   ids[0] === player_id ? (player_char || 0) : (opponent_char || 0),
                   ids[1] === player_id ? (player_char || 0) : (opponent_char || 0),
                   rounds || 0, source, ft, 0, 0);
            console.log(`[match] pending (${source} FT${ft}): ${player_id} reports winner=${winner_id}`);
            return json(res, 200, { ok: true, status: 'pending' });
        }

        // Second report for this game — cross-validate
        if (pending.reporter_id === player_id) {
            return json(res, 200, { ok: true, status: 'already_pending' });
        }

        if (pending.winner_id !== winner_id) {
            // Dispute — discard this game's report, keep session alive
            db.prepare("UPDATE pending_results SET reporter_id = '', winner_id = '', created_at = datetime('now') WHERE match_key = ?")
              .run(matchKey);
            console.log(`[match] dispute: ${pending.reporter_id} says ${pending.winner_id}, ${player_id} says ${winner_id}`);
            return json(res, 200, { ok: true, status: 'dispute' });
        }

        // Agreement — increment session wins for the winner
        let p1Wins = pending.p1_session_wins || 0;
        let p2Wins = pending.p2_session_wins || 0;
        if (winner_id === pending.p1_id) p1Wins++;
        else p2Wins++;
        const sessionFt = pending.ft || ft;
        const sessionSource = pending.source || source;

        // Check if session is complete
        if (p1Wins < sessionFt && p2Wins < sessionFt) {
            // Session in progress — update wins and reset reporter for next game
            db.prepare("UPDATE pending_results SET p1_session_wins = ?, p2_session_wins = ?, reporter_id = '', winner_id = '', created_at = datetime('now') WHERE match_key = ?")
              .run(p1Wins, p2Wins, matchKey);
            console.log(`[match] session in progress (${sessionSource} FT${sessionFt}): ${pending.p1_id} ${p1Wins}-${p2Wins} ${pending.p2_id}`);
            return json(res, 200, { ok: true, status: 'session_in_progress', p1_wins: p1Wins, p2_wins: p2Wins, ft: sessionFt });
        }

        // Session complete — one player reached FT wins
        const sessionWinnerId = p1Wins >= sessionFt ? pending.p1_id : pending.p2_id;
        const sessionLoserId = sessionWinnerId === pending.p1_id ? pending.p2_id : pending.p1_id;
        const winnerName = players.get(sessionWinnerId)?.display_name || sessionWinnerId;
        const loserName = players.get(sessionLoserId)?.display_name || sessionLoserId;

        // Delete pending session
        db.prepare('DELETE FROM pending_results WHERE match_key = ?').run(matchKey);

        // Record the session result in matches table
        const matchInfo = db.prepare(`INSERT INTO matches (p1_id, p2_id, winner_id, p1_char, p2_char, rounds)
                    VALUES (?, ?, ?, ?, ?, ?)`)
          .run(pending.p1_id, pending.p2_id, sessionWinnerId, pending.p1_char, pending.p2_char, p1Wins + p2Wins);

        if (sessionSource === 'ranked') {
            // Ranked: update Glicko-2 ratings + wins/losses
            const getStats = db.prepare('SELECT rating, rd, volatility FROM players_db WHERE player_id = ?');
            let wStats = getStats.get(sessionWinnerId) || { rating: DEFAULT_RATING, rd: DEFAULT_RD, volatility: DEFAULT_VOL };
            let lStats = getStats.get(sessionLoserId)  || { rating: DEFAULT_RATING, rd: DEFAULT_RD, volatility: DEFAULT_VOL };
            const newStats = glicko2Update(wStats, lStats);

            const upsertPlayer = db.prepare(`
                INSERT INTO players_db (player_id, display_name, wins, losses, rating, rd, volatility, country)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                ON CONFLICT(player_id) DO UPDATE SET
                    display_name = excluded.display_name,
                    wins = wins + excluded.wins,
                    losses = losses + excluded.losses,
                    rating = excluded.rating,
                    rd = excluded.rd,
                    volatility = excluded.volatility,
                    country = CASE WHEN excluded.country != '' THEN excluded.country ELSE players_db.country END,
                    last_match = datetime('now')
            `);
            const winnerCountry = players.get(sessionWinnerId)?.country || '';
            const loserCountry = players.get(sessionLoserId)?.country || '';
            upsertPlayer.run(sessionWinnerId, winnerName, 1, 0, newStats.winner.rating, newStats.winner.rd, newStats.winner.vol, winnerCountry);
            upsertPlayer.run(sessionLoserId, loserName, 0, 1, newStats.loser.rating, newStats.loser.rd, newStats.loser.vol, loserCountry);
        } else {
            // Casual: record win/loss but skip Glicko-2 rating changes
            const winnerCountryCasual = players.get(sessionWinnerId)?.country || '';
            const loserCountryCasual = players.get(sessionLoserId)?.country || '';
            db.prepare(`INSERT INTO players_db (player_id, display_name, wins, losses, country)
                VALUES (?, ?, 1, 0, ?)
                ON CONFLICT(player_id) DO UPDATE SET
                    display_name = excluded.display_name, wins = wins + 1,
                    country = CASE WHEN excluded.country != '' THEN excluded.country ELSE players_db.country END,
                    last_match = datetime('now')
            `).run(sessionWinnerId, winnerName, winnerCountryCasual);
            db.prepare(`INSERT INTO players_db (player_id, display_name, wins, losses, country)
                VALUES (?, ?, 0, 1, ?)
                ON CONFLICT(player_id) DO UPDATE SET
                    display_name = excluded.display_name, losses = losses + 1,
                    country = CASE WHEN excluded.country != '' THEN excluded.country ELSE players_db.country END,
                    last_match = datetime('now')
            `).run(sessionLoserId, loserName, loserCountryCasual);
        }

        const matchId = matchInfo.lastInsertRowid;
        console.log(`[match] session complete (${sessionSource} FT${sessionFt}): ${winnerName} beat ${loserName} ${p1Wins}-${p2Wins} (Match ID: ${matchId})`);
        return json(res, 200, { ok: true, status: 'recorded', match_id: matchId, p1_wins: p1Wins, p2_wins: p2Wins });
    }

    // --- POST /match_result/replay ---
    if (method === 'POST' && urlPath === '/match_result/replay') {
        if (!db) return json(res, 503, { error: 'Replay upload unavailable (no SQLite)' });

        // The room_code query param here is repurposed to pass the match_id
        const matchIdStr = url.searchParams.get('match_id');
        const matchId = parseInt(matchIdStr, 10);

        if (isNaN(matchId)) {
            return json(res, 400, { error: 'Missing or invalid match_id query parameter' });
        }

        // Validate magic header "3SXR" (0x33535852)
        if (!Buffer.isBuffer(body) || body.length < 16) {
            return json(res, 400, { error: 'Invalid replay file: too small' });
        }

        // 3SXR in little-endian is 0x33535852 => 'R' 'X' 'S' '3' or [0x52, 0x58, 0x53, 0x33]
        // But the struct uses u32 magic = 0x33535852. Let's just check the string "3SXR".
        // On x86 little endian, it will be 0x52, 0x58, 0x53, 0x33
        const magic = body.readUInt32LE(0);
        if (magic !== 0x33535852) {
            return json(res, 400, { error: 'Invalid replay file: bad magic header' });
        }

        // Verify the match actually exists and was created recently (e.g., within the last 15 minutes)
        const match = db.prepare(`
            SELECT p1_id, p2_id, has_replay,
                   (julianday('now') - julianday(created_at)) * 24 * 60 AS minutes_ago
            FROM matches WHERE id = ?
        `).get(matchId);

        if (!match) {
            return json(res, 404, { error: 'Match not found' });
        }

        if (match.minutes_ago > 15) {
            return json(res, 403, { error: 'Match is too old to accept a replay upload' });
        }

        if (match.has_replay) {
            return json(res, 200, { ok: true, status: 'already_has_replay' });
        }

        // Ensure the uploading player is one of the match participants
        const uploaderId = req.headers['x-player-id'];
        if (uploaderId && uploaderId !== match.p1_id && uploaderId !== match.p2_id) {
            return json(res, 403, { error: 'Only participants can upload the replay' });
        }

        // Save the file
        const fs = require('node:fs');
        const replaysDir = path.join(__dirname, 'replays');
        if (!fs.existsSync(replaysDir)) {
            fs.mkdirSync(replaysDir);
        }

        const filePath = path.join(replaysDir, `replay_${matchId}.bin`);
        try {
            fs.writeFileSync(filePath, body);

            // Mark the match as having a replay
            db.prepare('UPDATE matches SET has_replay = 1 WHERE id = ?').run(matchId);
            console.log(`[replay] uploaded for match ${matchId} (${body.length} bytes)`);

            return json(res, 200, { ok: true, status: 'uploaded' });
        } catch (err) {
            console.error('[replay] Failed to save replay:', err);
            return json(res, 500, { error: 'Failed to save replay file' });
        }
    }

    // --- POST /match_disconnect ---
    // Report a mid-match disconnect (ragequit). The remaining player calls this
    // when GekkoPlayerDisconnected fires before a natural match conclusion.
    // Increments the opponent's disconnect counter.
    if (method === 'POST' && urlPath === '/match_disconnect') {
        if (!db) return json(res, 503, { error: 'Match reporting unavailable (no SQLite)' });

        const data = parseJsonBody(res, body);
        if (!data) return;

        const { player_id, opponent_id } = data;
        if (!player_id || !opponent_id) {
            return json(res, 400, { error: 'Missing player_id or opponent_id' });
        }

        // Increment the opponent's disconnect counter (the reporter is the one who stayed)
        const upsertDC = db.prepare(`
            INSERT INTO players_db (player_id, display_name, disconnects)
            VALUES (?, ?, 1)
            ON CONFLICT(player_id) DO UPDATE SET
                disconnects = disconnects + 1,
                last_match = datetime('now')
        `);
        const opponentName = players.get(opponent_id)?.display_name || opponent_id;
        upsertDC.run(opponent_id, opponentName);

        console.log(`[match] disconnect reported: ${player_id} reports ${opponentName} disconnected`);
        return json(res, 200, { ok: true, status: 'disconnect_recorded' });
    }

    // --- GET /replays ---
    if (method === 'GET' && urlPath === '/replays') {
        if (!db) return json(res, 503, { error: 'Replays unavailable (no SQLite)' });

        const page = Math.max(0, parseInt(url.searchParams.get('page') || '0', 10));
        const limit = Math.min(50, Math.max(1, parseInt(url.searchParams.get('limit') || '20', 10)));
        const offset = page * limit;

        // Optional filter by player
        const playerId = url.searchParams.get('player_id');

        let query = `
            SELECT m.id, m.p1_id, m.p2_id, m.winner_id, m.p1_char, m.p2_char, m.rounds, m.created_at,
                   p1.display_name AS p1_name, p2.display_name AS p2_name
            FROM matches m
            LEFT JOIN players_db p1 ON m.p1_id = p1.player_id
            LEFT JOIN players_db p2 ON m.p2_id = p2.player_id
            WHERE m.has_replay = 1
        `;
        let params = [];

        if (playerId) {
            query += ` AND (m.p1_id = ? OR m.p2_id = ?)`;
            params.push(playerId, playerId);
        }

        query += ` ORDER BY m.created_at DESC LIMIT ? OFFSET ?`;
        params.push(limit, offset);

        const rows = db.prepare(query).all(...params);

        // Count total
        let countQuery = `SELECT COUNT(*) as cnt FROM matches WHERE has_replay = 1`;
        let countParams = [];
        if (playerId) {
            countQuery += ` AND (p1_id = ? OR p2_id = ?)`;
            countParams.push(playerId, playerId);
        }
        const total = db.prepare(countQuery).get(...countParams).cnt;

        return json(res, 200, {
            replays: rows.map(r => ({
                match_id: r.id,
                p1_id: r.p1_id,
                p1_name: r.p1_name || r.p1_id,
                p2_id: r.p2_id,
                p2_name: r.p2_name || r.p2_id,
                winner_id: r.winner_id,
                p1_char: r.p1_char,
                p2_char: r.p2_char,
                rounds: r.rounds,
                created_at: r.created_at
            })),
            total,
            page
        });
    }

    // --- GET /replays/:id --- (Download raw replay binary)
    const replayDownloadMatch = urlPath.match(/^\/replays\/(\d+)$/);
    if (method === 'GET' && replayDownloadMatch) {
        const matchId = replayDownloadMatch[1];
        const fs = require('node:fs');
        const filePath = path.join(__dirname, 'replays', `replay_${matchId}.bin`);

        if (!fs.existsSync(filePath)) {
            return json(res, 404, { error: 'Replay file not found' });
        }

        const stat = fs.statSync(filePath);
        res.writeHead(200, {
            'Content-Type': 'application/octet-stream',
            'Content-Length': stat.size,
            'Content-Disposition': `attachment; filename="replay_${matchId}.bin"`
        });

        const readStream = fs.createReadStream(filePath);
        readStream.pipe(res);
        return; // Stream handles res.end()
    }

    // --- GET /player/:id/stats ---
    const statsMatch = urlPath.match(/^\/player\/([^/]+)\/stats$/);
    if (method === 'GET' && statsMatch) {
        if (!db) return json(res, 503, { error: 'Stats unavailable (no SQLite)' });

        const playerId = decodeURIComponent(statsMatch[1]);
        const row = db.prepare('SELECT wins, losses, disconnects, rating, rd FROM players_db WHERE player_id = ?').get(playerId);

        if (!row) {
            return json(res, 200, { player_id: playerId, wins: 0, losses: 0, disconnects: 0, rating: 1500.0, rd: 350.0, tier: 'bronze' });
        }

        return json(res, 200, {
            player_id: playerId,
            wins: row.wins,
            losses: row.losses,
            disconnects: row.disconnects,
            rating: row.rating,
            rd: row.rd,
            tier: getTier(row.rating)
        });
    }

    // --- GET /leaderboard ---
    if (method === 'GET' && urlPath === '/leaderboard') {
        if (!db) return json(res, 503, { error: 'Leaderboard unavailable (no SQLite)' });

        const page = Math.max(0, parseInt(url.searchParams.get('page') || '0', 10));
        const limit = Math.min(50, Math.max(1, parseInt(url.searchParams.get('limit') || '20', 10)));
        const offset = page * limit;

        const total = db.prepare('SELECT COUNT(*) as cnt FROM players_db').get().cnt;
        const rows = db.prepare(
            'SELECT player_id, display_name, wins, losses, disconnects, rating, country FROM players_db ORDER BY rating DESC, wins DESC LIMIT ? OFFSET ?'
        ).all(limit, offset);

        // Precompute most-played character for each player in the result set
        const mostPlayedStmt = db.prepare(`
            SELECT char_id, COUNT(*) as cnt FROM (
                SELECT p1_char AS char_id FROM matches WHERE p1_id = ?
                UNION ALL
                SELECT p2_char AS char_id FROM matches WHERE p2_id = ?
            ) GROUP BY char_id ORDER BY cnt DESC LIMIT 1
        `);

        return json(res, 200, {
            players: rows.map((r, i) => {
                // Country: prefer in-memory presence, fall back to DB-stored
                const presence = players.get(r.player_id);
                const country = (presence && presence.country) ? presence.country : (r.country || '');

                // Most-played character from match history
                const charRow = mostPlayedStmt.get(r.player_id, r.player_id);
                const most_played_char = charRow ? charRow.char_id : -1;

                return {
                    rank: offset + i + 1,
                    player_id: r.player_id,
                    display_name: r.display_name || r.player_id,
                    wins: r.wins,
                    losses: r.losses,
                    disconnects: r.disconnects || 0,
                    rating: r.rating,
                    tier: getTier(r.rating),
                    country,
                    grade: getGrade(r.rating),
                    most_played_char,
                };
            }),
            total,
            page,
        });
    }

    // --- POST /leave ---
    if (method === 'POST' && urlPath === '/leave') {
        const data = parseJsonBody(res, body);
        if (!data) return;

        players.delete(data.player_id);
        return json(res, 200, { ok: true });
    }

    return json(res, 404, { error: 'Not found' });
}

// ---- Server ----

const server = http.createServer(async (req, res) => {
    try {
        await handleRequest(req, res);
    } catch (err) {
        console.error('Request error:', err);
        json(res, 500, { error: 'Internal server error' });
    }
});

server.listen(PORT, '0.0.0.0', () => {
    console.log(`3SX Lobby Server listening on port ${PORT}`);
    console.log(`HMAC auth: enabled (key length: ${SECRET.length})`);
    console.log(`GeoIP: ${geoip ? 'enabled' : 'disabled (install geoip-lite for country detection)'}`);
});

// ---- Graceful Shutdown ----

function shutdown(signal) {
    console.log(`\n${signal} received. Shutting down...`);
    clearInterval(cleanupTimer);
    if (db) {
        try { db.close(); console.log('SQLite: closed.'); } catch { /* ignore */ }
    }
    server.close(() => {
        console.log('Server closed.');
        process.exit(0);
    });
    // Force exit after 5s if connections don't drain
    setTimeout(() => process.exit(1), 5000);
}

process.on('SIGTERM', () => shutdown('SIGTERM'));
process.on('SIGINT', () => shutdown('SIGINT'));
