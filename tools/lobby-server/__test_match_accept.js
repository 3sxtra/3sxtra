const crypto = require('crypto');

const SECRET = process.env.LOBBY_SECRET || 'testsecret';
const BASE = process.env.LOBBY_URL || 'http://localhost:3000';

async function sendreq(path, body) {
    const timestamp = Math.floor(Date.now() / 1000).toString();
    const bodyStr = JSON.stringify(body);
    const hmac = crypto.createHmac('sha256', SECRET);
    hmac.update(timestamp + 'POST' + path + bodyStr);

    const res = await fetch(`${BASE}${path}`, {
        method: 'POST',
        headers: {
            'X-Timestamp': timestamp,
            'X-Signature': hmac.digest('hex'),
            'Content-Type': 'application/json'
        },
        body: bodyStr
    });
    const json = await res.json();
    return { status: res.status, ...json };
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
    console.log('=== Phase 6: Match Accept/Decline Test ===\n');

    // --- Setup: create room with 3 players ---
    const createRes = await sendreq('/room/create', { player_id: 'host', display_name: 'Host' });
    assert(createRes.ok, 'Room created');
    const code = createRes.room_code;
    console.log(`   Room: ${code}\n`);

    await sendreq('/room/join', { player_id: 'p2', display_name: 'P2', room_code: code });
    await sendreq('/room/join', { player_id: 'p3', display_name: 'P3', room_code: code });

    // --- Test 1: Two queue → proposal (not immediate start) ---
    console.log('--- Test 1: Proposal flow ---');
    await sendreq('/room/queue/join', { player_id: 'host', room_code: code });
    await sendreq('/room/queue/join', { player_id: 'p2', room_code: code });
    let state = await getState(code);
    assert(state.match && state.match.state === 'proposed', 'Match state is "proposed" (not "playing")');
    assert(state.match.p1 === 'host', 'P1 is host');
    assert(state.match.p2 === 'p2', 'P2 is p2');
    assert(state.queue.length === 0, 'Queue drained for proposal');

    // --- Test 2: Single accept — match stays proposed ---
    console.log('\n--- Test 2: Single accept ---');
    let acceptRes = await sendreq('/room/match/accept', { room_code: code, player_id: 'host' });
    assert(acceptRes.ok && !acceptRes.match_started, 'Host accepted, match not yet started');
    state = await getState(code);
    assert(state.match.state === 'proposed', 'Match still proposed with 1 accept');

    // --- Test 3: Both accept — match starts ---
    console.log('\n--- Test 3: Both accept ---');
    acceptRes = await sendreq('/room/match/accept', { room_code: code, player_id: 'p2' });
    assert(acceptRes.ok && acceptRes.match_started, 'P2 accepted, match started');
    state = await getState(code);
    assert(state.match.state === 'playing', 'Match state is "playing"');

    // --- Test 4: End match, new proposal, then decline ---
    console.log('\n--- Test 4: Decline flow ---');
    // Add p3 to queue so there's a next pair after decline
    await sendreq('/room/queue/join', { player_id: 'p3', room_code: code });
    // End current match
    await sendreq('/room/match/end', { room_code: code, winner_id: 'host' });
    state = await getState(code);
    // After match end: winner(host) + p3 should be proposed, p2 at back of queue
    assert(state.match && state.match.state === 'proposed', 'New match proposed after match end');
    assert(state.match.p1 === 'host', 'Winner stays on as P1');
    assert(state.match.p2 === 'p3', 'P3 is new P2');

    // Host declines
    const declineRes = await sendreq('/room/match/decline', { room_code: code, player_id: 'host' });
    assert(declineRes.ok, 'Host declined');
    state = await getState(code);
    // After decline: p3 (non-decliner) goes to front, host (decliner) to back
    // Then tryStartMatch fires: p3 vs p2 proposed
    assert(state.match && state.match.state === 'proposed', 'Next pair proposed after decline');
    assert(state.match.p1 === 'p3', 'Non-decliner (p3) at front');
    assert(state.match.p2 === 'p2', 'p2 is next from queue');
    assert(state.queue.length === 1 && state.queue[0] === 'host', 'Decliner (host) at back of queue');

    // --- Test 5: Non-participant cannot accept ---
    console.log('\n--- Test 5: Validation ---');
    const badAccept = await sendreq('/room/match/accept', { room_code: code, player_id: 'host' });
    assert(badAccept.status === 400, 'Non-participant accept rejected');

    const badDecline = await sendreq('/room/match/decline', { room_code: code, player_id: 'host' });
    assert(badDecline.status === 400, 'Non-participant decline rejected');

    // --- Test 6: Leave during proposal cancels it ---
    console.log('\n--- Test 6: Leave during proposal ---');
    // p3 vs p2 is currently proposed. p3 leaves.
    await sendreq('/room/leave', { player_id: 'p3', room_code: code });
    state = await getState(code);
    // p2 should be at front of queue, host at back. No match since only 1 in queue after p3 left.
    // (p2 goes to front via unshift, but p3 is removed from players so no new proposal possible with 2 left)
    assert(!state.match || state.match === null, 'No match after proposee leaves');

    // --- Cleanup ---
    await sendreq('/room/leave', { player_id: 'host', room_code: code });
    await sendreq('/room/leave', { player_id: 'p2', room_code: code });

    console.log('\n=== All Phase 6 tests passed! ===');
}

run().catch(err => {
    console.error('Test error:', err);
    process.exit(1);
});
