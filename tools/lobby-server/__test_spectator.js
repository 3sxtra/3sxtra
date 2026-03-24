/**
 * @file __test_spectator.js
 * @brief Integration test for spectator count tracking on the lobby server.
 *
 * Tests:
 *   1. Create room → verify initial spectator_count = 0
 *   2. POST /room/:code/spectate → verify count goes up
 *   3. Multiple spectators → verify count increments
 *   4. DELETE /room/:code/spectate → verify count goes down
 *   5. Player leave → verify spectator removed implicitly
 *   6. Verify spectator_count in /rooms/list
 *
 * Usage:
 *   # Terminal 1: Start server
 *   set LOBBY_SECRET=testsecret && node lobby-server.js
 *
 *   # Terminal 2: Run test
 *   set LOBBY_SECRET=testsecret && node __test_spectator.js
 */

const crypto = require('crypto');

const SECRET = process.env.LOBBY_SECRET || 'testsecret';
const BASE = process.env.LOBBY_URL || 'http://localhost:3000';

async function sendreq(path, body, overrideMethod) {
    const timestamp = Math.floor(Date.now() / 1000).toString();
    const bodyStr = body ? JSON.stringify(body) : '';
    const method = overrideMethod || 'POST';
    const hmac = crypto.createHmac('sha256', SECRET);
    hmac.update(timestamp + method + path + bodyStr);

    const reqOpts = {
        method: method,
        headers: {
            'X-Timestamp': timestamp,
            'X-Signature': hmac.digest('hex'),
        }
    };
    if (body) {
        reqOpts.headers['Content-Type'] = 'application/json';
        reqOpts.body = bodyStr;
    }

    const res = await fetch(`${BASE}${path}`, reqOpts);
    return { status: res.status, ...(await res.json()) };
}

async function getState(roomCode) {
    const res = await fetch(`${BASE}/room/state?room_code=${roomCode}`);
    return await res.json();
}

function assert(cond, msg) {
    if (!cond) {
        console.error(`❌ FAIL: ${msg}`);
        process.exit(1);
    }
    console.log(`✅ ${msg}`);
}

async function run() {
    console.log('=== Spectator Count Integration Test ===\n');

    // Register players via presence
    for (const id of ['host', 'p2', 'spec1', 'spec2']) {
        await sendreq('/presence', {
            player_id: id, display_name: id.toUpperCase(),
            room_code: `STUN_${id}`, connection_type: 'wired', rtt_ms: 20
        });
    }

    // --- Test 1: Create room, verify initial count = 0 ---
    console.log('--- Test 1: Initial spectator_count = 0 ---');
    const createRes = await sendreq('/room/create', {
        player_id: 'host', display_name: 'HOST', name: 'Spectator Test', ft: 2
    });
    assert(createRes.ok, 'Room created');
    const code = createRes.room_code;

    let state = await getState(code);
    assert(state.spectator_count === 0, `Initial spectator_count = 0 (got ${state.spectator_count})`);

    // Join a second player
    await sendreq('/room/join', { player_id: 'p2', display_name: 'P2', room_code: code });

    // --- Test 2: POST /room/:code/spectate → count = 1 ---
    console.log('\n--- Test 2: Spectate start ---');
    const spec1 = await sendreq(`/room/${code}/spectate`, { player_id: 'spec1' });
    assert(spec1.ok, 'Spectate start succeeded');
    assert(spec1.spectator_count === 1, `Response spectator_count = 1 (got ${spec1.spectator_count})`);

    state = await getState(code);
    assert(state.spectator_count === 1, `Room state spectator_count = 1 (got ${state.spectator_count})`);

    // --- Test 3: Second spectator → count = 2 ---
    console.log('\n--- Test 3: Multiple spectators ---');
    const spec2 = await sendreq(`/room/${code}/spectate`, { player_id: 'spec2' });
    assert(spec2.ok && spec2.spectator_count === 2, `Two spectators (got ${spec2.spectator_count})`);

    // --- Test 4: DELETE /room/:code/spectate → count back to 1 ---
    console.log('\n--- Test 4: Spectate stop ---');
    const specStop = await sendreq(`/room/${code}/spectate`, { player_id: 'spec2' }, 'DELETE');
    assert(specStop.ok && specStop.spectator_count === 1, `After stop: count = 1 (got ${specStop.spectator_count})`);

    state = await getState(code);
    assert(state.spectator_count === 1, `Room state confirms count = 1 (got ${state.spectator_count})`);

    // --- Test 5: Idempotent (spectating again doesn't double-count) ---
    console.log('\n--- Test 5: Idempotent add ---');
    await sendreq(`/room/${code}/spectate`, { player_id: 'spec1' });
    state = await getState(code);
    assert(state.spectator_count === 1, `Idempotent: still 1 (got ${state.spectator_count})`);

    // --- Test 6: Spectator_count in /rooms/list ---
    console.log('\n--- Test 6: /rooms/list includes spectator_count ---');
    const listRes = await fetch(`${BASE}/rooms/list`);
    const listData = await listRes.json();
    const ourRoom = listData.rooms.find(r => r.code === code);
    assert(ourRoom && typeof ourRoom.spectator_count === 'number',
        `Room listing has spectator_count field (${ourRoom?.spectator_count})`);

    // --- Test 7: Player leave removes from spectators ---
    console.log('\n--- Test 7: Leave removes spectator ---');
    // spec1 is a spectator but also needs to be in the room to leave
    // Actually spec1 is NOT a room player here — they're just a spectator.
    // Let's join spec1 to the room, then spectate, then leave
    await sendreq('/room/join', { player_id: 'spec1', display_name: 'SPEC1', room_code: code });
    await sendreq(`/room/${code}/spectate`, { player_id: 'spec1' });
    state = await getState(code);
    const countBefore = state.spectator_count;

    await sendreq('/room/leave', { player_id: 'spec1', room_code: code });
    state = await getState(code);
    assert(state.spectator_count < countBefore,
        `Spectator removed on leave: ${countBefore} → ${state.spectator_count}`);

    // --- Test 8: 404 for non-existent room ---
    console.log('\n--- Test 8: Error handling ---');
    const badRoom = await sendreq('/room/ZZZZ/spectate', { player_id: 'spec1' });
    assert(badRoom.status === 404, 'Non-existent room returns 404');

    // --- Cleanup ---
    console.log('\n--- Cleanup ---');
    for (const id of ['host', 'p2', 'spec1', 'spec2']) {
        await sendreq('/room/leave', { player_id: id, room_code: code });
    }

    console.log('\n=== All spectator tests passed! ===');
}

run().catch(err => {
    console.error('Test error:', err);
    process.exit(1);
});
