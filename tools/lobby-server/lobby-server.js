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
// North America East
['US', 'CA'].forEach(c => COUNTRY_TO_REGION[c] = 'NA-E'); // Default NA-E; US west coast overridden by timezone if available
// Europe West
['GB', 'IE', 'FR', 'ES', 'PT', 'NL', 'BE', 'DE', 'AT', 'CH', 'IT', 'DK', 'NO', 'SE', 'FI', 'IS', 'LU'].forEach(c => COUNTRY_TO_REGION[c] = 'EU-W');
// Europe East
['PL', 'CZ', 'SK', 'HU', 'RO', 'BG', 'HR', 'RS', 'UA', 'LT', 'LV', 'EE', 'GR', 'TR', 'RU'].forEach(c => COUNTRY_TO_REGION[c] = 'EU-E');
// Asia
['JP', 'KR', 'CN', 'TW', 'HK', 'SG', 'MY', 'TH', 'PH', 'ID', 'VN', 'IN', 'PK'].forEach(c => COUNTRY_TO_REGION[c] = 'ASIA');
// South America
['BR', 'AR', 'CL', 'CO', 'PE', 'VE', 'EC', 'UY', 'PY', 'BO'].forEach(c => COUNTRY_TO_REGION[c] = 'SA');
// Oceania
['AU', 'NZ'].forEach(c => COUNTRY_TO_REGION[c] = 'OCE');
// Africa
['ZA', 'NG', 'EG', 'KE', 'MA', 'TN', 'GH'].forEach(c => COUNTRY_TO_REGION[c] = 'AF');

function detectRegionAndCountry(ip) {
    if (!geoip) return { country: '', region: '' };
    // Strip IPv6 prefix from IPv4-mapped addresses
    const cleanIp = ip.replace(/^::ffff:/, '');
    const geo = geoip.lookup(cleanIp);
    if (!geo) return { country: '', region: '' };
    const country = geo.country || '';
    const region = COUNTRY_TO_REGION[country] || '';
    return { country, region };
}

// ---- Data Store ----

/** @type {Map<string, {display_name: string, region: string, country: string, room_code: string, connect_to: string, status: string, connection_type: string, rtt_ms: number, last_seen: number}>} */
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
            created_at TEXT DEFAULT (datetime('now'))
        );
        CREATE INDEX IF NOT EXISTS idx_matches_p1 ON matches(p1_id);
        CREATE INDEX IF NOT EXISTS idx_matches_p2 ON matches(p2_id);

        CREATE TABLE IF NOT EXISTS pending_results (
            match_key TEXT PRIMARY KEY,
            reporter_id TEXT NOT NULL,
            winner_id TEXT NOT NULL,
            p1_id TEXT NOT NULL,
            p2_id TEXT NOT NULL,
            p1_char INTEGER,
            p2_char INTEGER,
            rounds INTEGER,
            created_at TEXT DEFAULT (datetime('now'))
        );
    `);
    console.log(`SQLite: initialized at ${dbPath}`);
} catch (err) {
    console.warn(`SQLite: not available (${err.message}) — match reporting disabled`);
    console.warn('  Install with: npm install better-sqlite3');
}

// ---- Room Tracking (Casual Lobbies) ----
// Structure: Room { id: string, name: string, host: string, players: string[], state: 'waiting'|'playing' }
const rooms = new Map();

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
    return {
        id: room.id,
        name: room.name,
        host: room.host,
        players: room.players.map(id => {
            const p = players.get(id);
            return { player_id: id, display_name: p ? p.display_name : id, region: p ? p.region : '', country: p ? p.country : '' };
        }),
        queue: room.queue,
        match: room.match,
        chat: room.chat
    };
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

    // Find first viable pair (skip pairs on decline cooldown)
    for (let i = 0; i < room.queue.length - 1; i++) {
        for (let j = i + 1; j < room.queue.length; j++) {
            const id1 = room.queue[i];
            const id2 = room.queue[j];
            const pairKey = makeMatchPairKey(room.id, id1, id2);
            const cd = matchDeclineCooldowns.get(pairKey);
            if (cd && now < cd) continue; // still on cooldown

            // Found a viable pair — remove them from queue
            room.queue.splice(j, 1); // remove j first (higher index)
            room.queue.splice(i, 1);

            const p1 = id1;
            const p2 = id2;
            const p1_data = players.get(p1);
            const p2_data = players.get(p2);

            room.match = {
                p1, p2,
                state: 'proposed',
                accepts: { [p1]: false, [p2]: false },
                proposed_at: now
            };

            broadcastRoomEvent(room, 'match_propose', {
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
    // Clean stale pending match results (>60s)
    if (db) {
        try {
            db.prepare(`DELETE FROM pending_results WHERE datetime(created_at) < datetime('now', '-60 seconds')`).run();
        } catch { /* ignore */ }
    }
    // Phase 6: Timeout proposed matches (>10s without both accepts)
    for (const [, room] of rooms) {
        if (room.match && room.match.state === 'proposed' &&
            now - room.match.proposed_at > 10_000) {
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

                // Handle active/proposed match forfeit
                if (room.match && (room.match.p1 === staleId || room.match.p2 === staleId)) {
                    const other = room.match.p1 === staleId ? room.match.p2 : room.match.p1;
                    if (room.match.state === 'proposed') {
                        room.match = null;
                        room.queue.unshift(other);
                    } else if (room.match.state === 'playing') {
                        broadcastRoomEvent(room, 'match_end', {
                            winner_id: other, winner_name: getPlayerName(other),
                            loser_id: staleId, reason: 'disconnect'
                        });
                        room.queue.unshift(other);
                        room.match = null;
                    }
                }

                room.players = room.players.filter(p => p !== staleId);
                room.queue = room.queue.filter(p => p !== staleId);
                broadcastRoomEvent(room, 'leave', { player_id: staleId });
            }

            if (room.players.length === 0) {
                console.log(`[room] ${code} auto-closed (all players stale)`);
                rooms.delete(code);
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

    // Verify HMAC
    const payload = timestamp + method + path + body;
    const expected = crypto
        .createHmac('sha256', SECRET)
        .update(payload)
        .digest('hex');

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
            other.connect_to = String(room_code).slice(0, 15);
            console.log(`[match] ${display_name} -> ${other.display_name} (mutual connect_to set)`);
            break;
        }
    }
}

// ---- Routes ----

async function handleRequest(req, res) {
    const url = new URL(req.url, `http://${req.headers.host}`);
    const path = url.pathname;
    const fullPath = req.url; // includes query string — used for HMAC
    const method = req.method;

    // Read body for POST
    let body;
    try {
        body = method === 'POST' ? await readBody(req) : '';
    } catch (err) {
        return json(res, 413, { error: 'Request body too large' });
    }

    // --- Health endpoint (no auth required) ---
    if (method === 'GET' && path === '/') {
        return json(res, 200, {
            service: '3sx-lobby',
            players_online: players.size,
            players_searching: [...players.values()].filter(p => p.status === 'searching').length,
        });
    }

    // --- SSE Event Stream (no auth required, secured by unguessable room code) ---
    if (method === 'GET' && path === '/room/events') {
        const roomCode = url.searchParams.get('room_code');
        const room = rooms.get(roomCode);
        if (!room) return json(res, 404, { error: 'Room not found' });

        res.writeHead(200, {
            'Content-Type': 'text/event-stream',
            'Cache-Control': 'no-cache',
            'Connection': 'keep-alive'
        });
        res.write(`data: ${JSON.stringify({ type: 'sync', data: getRoomState(room) })}\n\n`);

        room.sseClients.add(res);
        req.on('close', () => room.sseClients.delete(res));
        return; // Keep connection open
    }

    // --- Room State (read-only, no auth — secured by unguessable room code) ---
    if (method === 'GET' && path === '/room/state') {
        const roomCode = url.searchParams.get('room_code');
        const room = rooms.get(roomCode);
        if (!room) return json(res, 404, { error: 'Room not found' });
        return json(res, 200, getRoomState(room));
    }

    // --- Room List (read-only, no auth — returns public room summaries) ---
    if (method === 'GET' && path === '/rooms/list') {
        const list = [];
        for (const [code, room] of rooms) {
            list.push({
                code: room.id,
                name: room.name,
                player_count: room.players.length
            });
        }
        return json(res, 200, { rooms: list });
    }

    // Auth check (all other endpoints)
    const auth = verifyRequest(method, fullPath, body, req.headers);
    if (!auth.ok) {
        return json(res, 403, { error: auth.reason });
    }

    // --- Casual Rooms (8-Player) ---

    if (method === 'POST' && path === '/room/create') {
        const data = parseJsonBody(res, body);
        if (!data) return;
        const code = generateRoomCode();
        const roomName = String(data.name || `${data.display_name}'s Room`).slice(0, 31);
        const hostId = data.player_id;

        rooms.set(code, {
            id: code,
            name: roomName,
            host: hostId,
            players: [hostId],
            queue: [], // Next in line for cabinet
            match: null, // { p1: 'id', p2: 'id', state: 'playing' }
            chat: [],
            sseClients: new Set()
        });
        console.log(`[room] ${hostId} created room ${code}: ${roomName}`);
        return json(res, 200, { ok: true, room_code: code });
    }

    if (method === 'POST' && path === '/room/join') {
        const data = parseJsonBody(res, body);
        if (!data) return;
        const room = rooms.get(data.room_code);
        if (!room) return json(res, 404, { error: 'Room not found' });
        if (room.players.length >= 8) return json(res, 400, { error: 'Room is full' });

        if (!room.players.includes(data.player_id)) {
            room.players.push(data.player_id);
            broadcastRoomEvent(room, 'join', { player_id: data.player_id, display_name: data.display_name });
            console.log(`[room] ${data.player_id} joined room ${room.id}`);
        }
        return json(res, 200, { ok: true, room });
    }

    if (method === 'POST' && path === '/room/leave') {
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
            console.log(`[room] ${room.id} closed (empty)`);
            rooms.delete(room.id);
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

    if (method === 'POST' && path === '/room/chat') {
        const data = parseJsonBody(res, body);
        if (!data) return;
        const room = rooms.get(data.room_code);
        if (!room) return json(res, 404, { error: 'Room not found' });

        const msg = {
            id: Date.now(),
            sender_id: data.player_id,
            sender_name: data.display_name,
            text: String(data.text).slice(0, 120)
        };
        room.chat.push(msg);
        if (room.chat.length > 50) room.chat.shift();

        broadcastRoomEvent(room, 'chat', msg);
        return json(res, 200, { ok: true });
    }

    if (method === 'POST' && path === '/room/queue/join') {
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

    if (method === 'POST' && path === '/room/queue/leave') {
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
    if (method === 'POST' && path === '/room/match/accept') {
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
    if (method === 'POST' && path === '/room/match/decline') {
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

    // --- POST /room/match/end --- "Winner Stays On" rotation
    if (method === 'POST' && path === '/room/match/end') {
        const data = parseJsonBody(res, body);
        if (!data) return;
        const room = rooms.get(data.room_code);
        if (!room) return json(res, 404, { error: 'Room not found' });
        if (!room.match || room.match.state !== 'playing') {
            return json(res, 400, { error: 'No active match' });
        }

        const { winner_id } = data;
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

    // --- / Casual Rooms ---

    // --- POST /presence ---
    if (method === 'POST' && path === '/presence') {
        const data = parseJsonBody(res, body);
        if (!data) return;

        const { player_id, display_name, region, room_code, connect_to, rtt_ms, connection_type } = data;
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
            room_code: String(room_code || '').slice(0, 15),
            connect_to: String(connect_to || '').slice(0, 15),
            status: existing ? existing.status : 'idle',
            connection_type: String(connection_type || 'unknown').slice(0, 7),
            rtt_ms: typeof rtt_ms === 'number' ? Math.max(0, Math.min(9999, rtt_ms)) : (existing ? existing.rtt_ms : -1),
            last_seen: Date.now(),
        });

        resolveConnectMatch(player_id, display_name, room_code, connect_to);
        return json(res, 200, { ok: true });
    }

    // --- POST /searching/start ---
    if (method === 'POST' && path === '/searching/start') {
        const data = parseJsonBody(res, body);
        if (!data) return;

        let p = players.get(data.player_id);
        if (!p) {
            // Create minimal entry if presence hasn't arrived yet (race condition fix)
            p = { display_name: data.player_id, region: '', country: '', room_code: '', connect_to: '', status: 'idle', connection_type: 'unknown', rtt_ms: -1, last_seen: Date.now() };
            players.set(data.player_id, p);
        }

        p.status = 'searching';
        p.last_seen = Date.now();
        return json(res, 200, { ok: true });
    }

    // --- POST /searching/stop ---
    if (method === 'POST' && path === '/searching/stop') {
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
    if (method === 'GET' && path === '/searching') {
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
            });
        }

        return json(res, 200, { players: result });
    }

    // --- POST /decline ---
    // Report a declined invite for rate limiting.
    // Implements exponential backoff: 30s -> 60s -> 120s -> 300s max.
    if (method === 'POST' && path === '/decline') {
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
    if (method === 'POST' && path === '/match_result') {
        if (!db) return json(res, 503, { error: 'Match reporting unavailable (no SQLite)' });

        const data = parseJsonBody(res, body);
        if (!data) return;

        const { player_id, opponent_id, winner_id, player_char, opponent_char, rounds } = data;
        if (!player_id || !opponent_id || !winner_id) {
            return json(res, 400, { error: 'Missing player_id, opponent_id, or winner_id' });
        }

        // Canonical match key: sorted IDs joined by ":"
        const ids = [player_id, opponent_id].sort();
        const matchKey = ids.join(':');

        const pending = db.prepare('SELECT * FROM pending_results WHERE match_key = ?').get(matchKey);

        if (!pending) {
            // First report — store as pending
            db.prepare(`INSERT OR REPLACE INTO pending_results (match_key, reporter_id, winner_id, p1_id, p2_id, p1_char, p2_char, rounds)
                        VALUES (?, ?, ?, ?, ?, ?, ?, ?)`)
              .run(matchKey, player_id, winner_id, ids[0], ids[1],
                   ids[0] === player_id ? (player_char || 0) : (opponent_char || 0),
                   ids[1] === player_id ? (player_char || 0) : (opponent_char || 0),
                   rounds || 0);
            console.log(`[match] pending: ${player_id} reports winner=${winner_id}`);
            return json(res, 200, { ok: true, status: 'pending' });
        }

        // Second report — cross-validate
        if (pending.reporter_id === player_id) {
            // Same player reporting again — update
            return json(res, 200, { ok: true, status: 'already_pending' });
        }

        // Delete pending regardless of outcome
        db.prepare('DELETE FROM pending_results WHERE match_key = ?').run(matchKey);

        if (pending.winner_id !== winner_id) {
            // Dispute — discard both reports
            console.log(`[match] dispute: ${pending.reporter_id} says ${pending.winner_id}, ${player_id} says ${winner_id}`);
            return json(res, 200, { ok: true, status: 'dispute' });
        }

        // Agreement — record the match
        db.prepare(`INSERT INTO matches (p1_id, p2_id, winner_id, p1_char, p2_char, rounds)
                    VALUES (?, ?, ?, ?, ?, ?)`)
          .run(pending.p1_id, pending.p2_id, winner_id, pending.p1_char, pending.p2_char, pending.rounds || rounds || 0);

        const winnerId = winner_id;
        const loserId = winnerId === player_id ? opponent_id : player_id;
        const winnerName = players.get(winnerId)?.display_name || winnerId;
        const loserName = players.get(loserId)?.display_name || loserId;

        // Fetch current stats to feed into Glicko-2
        const getStats = db.prepare('SELECT rating, rd, volatility FROM players_db WHERE player_id = ?');
        let wStats = getStats.get(winnerId) || { rating: DEFAULT_RATING, rd: DEFAULT_RD, volatility: DEFAULT_VOL };
        let lStats = getStats.get(loserId)  || { rating: DEFAULT_RATING, rd: DEFAULT_RD, volatility: DEFAULT_VOL };

        // Calculate new ratings
        const newStats = glicko2Update(wStats, lStats);

        // Upsert player stats with new ratings
        const upsertPlayer = db.prepare(`
            INSERT INTO players_db (player_id, display_name, wins, losses, rating, rd, volatility)
            VALUES (?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(player_id) DO UPDATE SET
                display_name = excluded.display_name,
                wins = wins + excluded.wins,
                losses = losses + excluded.losses,
                rating = excluded.rating,
                rd = excluded.rd,
                volatility = excluded.volatility,
                last_match = datetime('now')
        `);

        upsertPlayer.run(winnerId, winnerName, 1, 0, newStats.winner.rating, newStats.winner.rd, newStats.winner.vol);
        upsertPlayer.run(loserId, loserName, 0, 1, newStats.loser.rating, newStats.loser.rd, newStats.loser.vol);

        console.log(`[match] recorded: ${winnerName} beat ${loserName}`);
        return json(res, 200, { ok: true, status: 'recorded' });
    }

    // --- GET /player/:id/stats ---
    const statsMatch = path.match(/^\/player\/([^/]+)\/stats$/);
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
    if (method === 'GET' && path === '/leaderboard') {
        if (!db) return json(res, 503, { error: 'Leaderboard unavailable (no SQLite)' });

        const page = Math.max(0, parseInt(url.searchParams.get('page') || '0', 10));
        const limit = Math.min(50, Math.max(1, parseInt(url.searchParams.get('limit') || '20', 10)));
        const offset = page * limit;

        const total = db.prepare('SELECT COUNT(*) as cnt FROM players_db').get().cnt;
        const rows = db.prepare(
            'SELECT player_id, display_name, wins, losses, rating FROM players_db ORDER BY rating DESC, wins DESC LIMIT ? OFFSET ?'
        ).all(limit, offset);

        return json(res, 200, {
            players: rows.map((r, i) => ({
                rank: offset + i + 1,
                player_id: r.player_id,
                display_name: r.display_name || r.player_id,
                wins: r.wins,
                losses: r.losses,
                rating: r.rating,
                tier: getTier(r.rating)
            })),
            total,
            page,
        });
    }

    // --- POST /leave ---
    if (method === 'POST' && path === '/leave') {
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
