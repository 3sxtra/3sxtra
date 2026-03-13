const crypto = require('crypto');
const http = require('http');

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
    console.log('=== Match Rotation Test ===\n');

    // 1. Create room
    const createRes = await sendreq('/room/create', { player_id: 'host', display_name: 'Host' });
    assert(createRes.ok, 'Room created');
    const code = createRes.room_code;
    console.log(`   Room: ${code}\n`);

    // 2. Join two more players
    await sendreq('/room/join', { player_id: 'p2', display_name: 'P2', room_code: code });
    await sendreq('/room/join', { player_id: 'p3', display_name: 'P3', room_code: code });

    // 3. Players should NOT be auto-queued
    let state = await getState(code);
    assert(state.queue.length === 0, 'Join does not auto-queue (queue empty)');

    // 4. Two players join queue — match should auto-start
    await sendreq('/room/queue/join', { player_id: 'host', room_code: code });
    state = await getState(code);
    assert(state.queue.length === 1, 'First queue join: queue has 1');
    assert(!state.match, 'No match started yet with 1 in queue');

    await sendreq('/room/queue/join', { player_id: 'p2', room_code: code });
    state = await getState(code);
    assert(state.match && state.match.state === 'playing', 'Match auto-started with 2 in queue');
    assert(state.match.p1 === 'host', 'P1 is "host" (first queued)');
    assert(state.match.p2 === 'p2', 'P2 is "p2" (second queued)');
    assert(state.queue.length === 0, 'Queue drained to start match');

    // 5. Third player joins queue while match is active
    await sendreq('/room/queue/join', { player_id: 'p3', room_code: code });
    state = await getState(code);
    assert(state.queue.length === 1 && state.queue[0] === 'p3', 'P3 in queue while match plays');

    // 6. P2 cannot join queue while in active match
    const queueWhilePlaying = await sendreq('/room/queue/join', { player_id: 'p2', room_code: code });
    assert(queueWhilePlaying.status === 400, 'Cannot join queue while in active match');

    // 7. Match ends — winner stays on
    console.log('\n--- Match 1 ends: host wins ---');
    await sendreq('/room/match/end', { room_code: code, winner_id: 'host' });
    state = await getState(code);
    // Winner (host) should be in match again (front of queue), loser (p2) to back
    assert(state.match && state.match.state === 'playing', 'Next match auto-started');
    assert(state.match.p1 === 'host', 'Winner (host) stays on as P1');
    assert(state.match.p2 === 'p3', 'P3 (next in queue) is P2');
    // Queue should have loser at the back
    assert(state.queue.length === 1 && state.queue[0] === 'p2', 'Loser (p2) at back of queue');

    // 8. Another match end — p3 wins this time
    console.log('\n--- Match 2 ends: p3 wins ---');
    await sendreq('/room/match/end', { room_code: code, winner_id: 'p3' });
    state = await getState(code);
    assert(state.match && state.match.state === 'playing', 'Third match auto-started');
    assert(state.match.p1 === 'p3', 'Winner (p3) stays on as P1');
    assert(state.match.p2 === 'p2', 'p2 is next challenger');
    assert(state.queue.length === 1 && state.queue[0] === 'host', 'Loser (host) at back of queue');

    // 9. Test forfeit: p2 leaves room while in match
    console.log('\n--- Forfeit test: p2 leaves room while playing ---');
    await sendreq('/room/leave', { player_id: 'p2', room_code: code });
    state = await getState(code);
    assert(state.players.length === 2, 'P2 removed from room');
    // p3 should be winner, match should auto-start if enough in queue
    assert(state.match && state.match.p1 === 'p3', 'Winner (p3) stays after forfeit');
    assert(state.match.p2 === 'host', 'Host is nextchallenger');

    // 10. Invalid winner_id
    console.log('\n--- Invalid winner test ---');
    const invalidRes = await sendreq('/room/match/end', { room_code: code, winner_id: 'nobody' });
    assert(invalidRes.status === 400, 'Invalid winner_id rejected');

    // Cleanup
    await sendreq('/room/leave', { player_id: 'host', room_code: code });
    await sendreq('/room/leave', { player_id: 'p3', room_code: code });

    console.log('\n=== All tests passed! ===');
}

run().catch(err => {
    console.error('Test error:', err);
    process.exit(1);
});
