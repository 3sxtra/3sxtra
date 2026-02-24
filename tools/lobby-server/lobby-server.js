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
 * Environment variables:
 *   LOBBY_SECRET  — shared HMAC key (required)
 *   LOBBY_PORT    — HTTP port (default: 3000)
 *
 * Usage:
 *   LOBBY_SECRET="your-secret-key" node lobby-server.js
 */

const http = require('node:http');
const crypto = require('node:crypto');

const PORT = parseInt(process.env.LOBBY_PORT || '3000', 10);
const SECRET = process.env.LOBBY_SECRET || '';

if (!SECRET) {
    console.error('ERROR: LOBBY_SECRET environment variable is required.');
    process.exit(1);
}

// ---- Data Store ----

/** @type {Map<string, {display_name: string, region: string, room_code: string, connect_to: string, status: string, last_seen: number}>} */
const players = new Map();

// Cleanup stale players every 30 seconds
setInterval(() => {
    const now = Date.now();
    for (const [id, p] of players) {
        if (now - p.last_seen > 60_000) {
            players.delete(id);
        }
    }
}, 30_000);

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
        req.on('data', chunk => { data += chunk; });
        req.on('end', () => resolve(data));
        req.on('error', reject);
    });
}

// ---- Routes ----

async function handleRequest(req, res) {
    const url = new URL(req.url, `http://${req.headers.host}`);
    const path = url.pathname;
    const fullPath = req.url; // includes query string — used for HMAC
    const method = req.method;

    // Read body for POST
    const body = method === 'POST' ? await readBody(req) : '';

    // --- Health endpoint (no auth required) ---
    if (method === 'GET' && path === '/') {
        return json(res, 200, {
            service: '3sx-lobby',
            players_online: players.size,
            players_searching: [...players.values()].filter(p => p.status === 'searching').length,
        });
    }

    // Auth check (all other endpoints)
    const auth = verifyRequest(method, fullPath, body, req.headers);
    if (!auth.ok) {
        return json(res, 403, { error: auth.reason });
    }

    // --- POST /presence ---
    if (method === 'POST' && path === '/presence') {
        let data;
        try { data = JSON.parse(body); } catch { return json(res, 400, { error: 'Invalid JSON' }); }

        const { player_id, display_name, region, room_code, connect_to } = data;
        if (!player_id || !display_name) {
            return json(res, 400, { error: 'Missing player_id or display_name' });
        }

        const existing = players.get(player_id);
        players.set(player_id, {
            display_name: String(display_name).slice(0, 31),
            region: String(region || '').slice(0, 7),
            room_code: String(room_code || '').slice(0, 15),
            connect_to: String(connect_to || '').slice(0, 15),
            status: existing ? existing.status : 'idle',
            last_seen: Date.now(),
        });

        // Server-side connect matching: if A wants to connect to B,
        // automatically set B's connect_to to A's room_code so both
        // sides see the mutual intent on their next poll.
        if (connect_to && room_code) {
            for (const [otherId, other] of players) {
                if (otherId === player_id) continue;
                if (other.room_code === connect_to) {
                    other.connect_to = String(room_code).slice(0, 15);
                    console.log(`[match] ${display_name} -> ${other.display_name} (mutual connect_to set)`);
                    break;
                }
            }
        }

        return json(res, 200, { ok: true });
    }

    // --- POST /searching/start ---
    if (method === 'POST' && path === '/searching/start') {
        let data;
        try { data = JSON.parse(body); } catch { return json(res, 400, { error: 'Invalid JSON' }); }

        const p = players.get(data.player_id);
        if (!p) return json(res, 404, { error: 'Player not found. Call /presence first.' });

        p.status = 'searching';
        p.last_seen = Date.now();
        return json(res, 200, { ok: true });
    }

    // --- POST /searching/stop ---
    if (method === 'POST' && path === '/searching/stop') {
        let data;
        try { data = JSON.parse(body); } catch { return json(res, 400, { error: 'Invalid JSON' }); }

        const p = players.get(data.player_id);
        if (!p) return json(res, 404, { error: 'Player not found' });

        p.status = 'idle';
        p.last_seen = Date.now();
        return json(res, 200, { ok: true });
    }

    // --- GET /searching ---
    if (method === 'GET' && path === '/searching') {
        const regionFilter = url.searchParams.get('region');
        const result = [];

        for (const [id, p] of players) {
            if (p.status !== 'searching') continue;
            if (regionFilter && p.region !== regionFilter) continue;
            result.push({
                player_id: id,
                display_name: p.display_name,
                region: p.region,
                room_code: p.room_code,
                connect_to: p.connect_to || '',
            });
        }

        return json(res, 200, { players: result });
    }

    // --- POST /leave ---
    if (method === 'POST' && path === '/leave') {
        let data;
        try { data = JSON.parse(body); } catch { return json(res, 400, { error: 'Invalid JSON' }); }

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
});
