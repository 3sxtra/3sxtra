/**
 * @file __test_tournament.js
 * @brief Integration test for tournament bracket management on the lobby server.
 *
 * Tests the full tournament lifecycle:
 *   1. Create tournament room (verify room_type in listing)
 *   2. Join 4 players
 *   3. Start bracket → verify bracket structure (2 rounds for 4 players)
 *   4. Accept first-round matches → verify match_start
 *   5. Report match results → verify bracket advancement
 *   6. Test TO override and DQ
 *   7. Full tournament completion
 *
 * Usage:
 *   # Terminal 1: Start server
 *   set LOBBY_SECRET=testsecret && node lobby-server.js
 *
 *   # Terminal 2: Run test
 *   set LOBBY_SECRET=testsecret && node __test_tournament.js
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

async function sleep(ms) {
    return new Promise(r => setTimeout(r, ms));
}

async function run() {
    console.log('=== Tournament Server Integration Test ===\n');

    // Register all players via presence (required for match proposals)
    for (const id of ['host', 'p2', 'p3', 'p4']) {
        await sendreq('/presence', {
            player_id: id, display_name: id.toUpperCase(),
            room_code: `STUN_${id}`, connection_type: 'wired', rtt_ms: 20
        });
    }

    // --- Test 1: Create tournament room ---
    console.log('--- Test 1: Create tournament room ---');
    const createRes = await sendreq('/room/create', {
        player_id: 'host', display_name: 'Host', name: 'Friday Night FT3',
        room_type: 2, tournament_format: 0, max_players: 8, ft: 3, seeding: 'join_order'
    });
    assert(createRes.ok, 'Tournament room created');
    const code = createRes.room_code;
    console.log(`   Room code: ${code}`);

    // Verify room_type in listing
    const listRes = await fetch(`${BASE}/rooms/list`);
    const listData = await listRes.json();
    const ourRoom = listData.rooms.find(r => r.code === code);
    assert(ourRoom && ourRoom.room_type === 2, 'Room listed with room_type=2 (tournament)');

    // Verify room state
    let state = await getState(code);
    assert(state.room_type === 2, 'getRoomState includes room_type=2');

    // --- Test 2: Join 3 more players (4 total) ---
    console.log('\n--- Test 2: Join players ---');
    for (const id of ['p2', 'p3', 'p4']) {
        const joinRes = await sendreq('/room/join', { player_id: id, display_name: id.toUpperCase(), room_code: code });
        assert(joinRes.ok, `${id} joined`);
    }
    state = await getState(code);
    assert(state.players.length === 4, '4 players in room');

    // --- Test 3: Start bracket ---
    console.log('\n--- Test 3: Start bracket ---');

    // Non-host can't start
    const badStart = await sendreq(`/room/${code}/bracket/start`, { player_id: 'p2' });
    assert(badStart.status === 403, 'Non-host start rejected (403)');

    // Host starts bracket
    const startRes = await sendreq(`/room/${code}/bracket/start`, { player_id: 'host' });
    assert(startRes.ok, 'Bracket started');
    const bracketState = startRes.bracket;
    assert(bracketState.started === 1, 'Tournament marked as started');
    assert(bracketState.total_rounds === 2, '4-player single elim = 2 rounds');
    assert(bracketState.bracket.length >= 3, 'At least 3 bracket entries (2 semis + 1 final)');

    // Can't start again
    const dupStart = await sendreq(`/room/${code}/bracket/start`, { player_id: 'host' });
    assert(dupStart.status === 400, 'Double start rejected');

    // --- Test 4: GET bracket endpoint ---
    console.log('\n--- Test 4: GET bracket ---');
    const getBracket = await sendreq(`/room/${code}/bracket`, null, 'GET');
    assert(getBracket.started === 1, 'GET bracket returns started state');
    assert(getBracket.bracket.length >= 3, 'GET bracket returns bracket entries');
    assert(getBracket.matches.length >= 0, 'GET bracket includes matches');

    // --- Test 5: Accept round 1 matches ---
    console.log('\n--- Test 5: Accept round 1 matches ---');
    // Get current bracket state from GET endpoint to find match pairings
    const currentBracket = await sendreq(`/room/${code}/bracket`, null, 'GET');
    const round0Matches = currentBracket.matches.filter(m => m.round === 0);
    console.log(`   Round 0 matches: ${round0Matches.length}`);

    // Both players accept each match
    for (const m of round0Matches) {
        // P1 accepts
        const acc1 = await sendreq('/room/match/accept', {
            room_code: code, player_id: m.p1, match_index: m.match_index
        });
        assert(acc1.ok, `${m.p1} accepted match ${m.match_index}`);
        assert(!acc1.match_started, 'Match not started yet (1 accept)');

        // P2 accepts
        const acc2 = await sendreq('/room/match/accept', {
            room_code: code, player_id: m.p2, match_index: m.match_index
        });
        assert(acc2.ok && acc2.match_started, `Match ${m.match_index} started after both accept`);
    }

    // --- Test 6: Report match results → bracket advancement ---
    console.log('\n--- Test 6: Report results ---');
    for (const m of round0Matches) {
        const endRes = await sendreq('/room/match/end', {
            room_code: code, winner_id: m.p1, match_index: m.match_index
        });
        assert(endRes.ok, `Match ${m.match_index} result reported (winner: ${m.p1})`);
    }

    // Verify round advanced — check bracket state
    const afterR1 = await sendreq(`/room/${code}/bracket`, null, 'GET');
    assert(afterR1.round === 1, 'Advanced to round 1 (finals)');
    // Round 0 entries should all be completed
    const r0Completed = afterR1.bracket.filter(b => b.round === 0).every(b => b.completed);
    assert(r0Completed, 'All round 0 matches completed in bracket');
    // Finals entry should have both player IDs populated
    const finalsEntry = afterR1.bracket.find(b => b.round === 1);
    assert(finalsEntry && finalsEntry.player1_id && finalsEntry.player2_id, 'Finals match has both players');

    // --- Test 7: Accept and complete finals ---
    console.log('\n--- Test 7: Finals ---');
    const finalMatches = afterR1.matches.filter(m => m.round === 1);
    assert(finalMatches.length === 1, 'One finals match');

    const fm = finalMatches[0];
    await sendreq('/room/match/accept', { room_code: code, player_id: fm.p1, match_index: fm.match_index });
    const finalAcc = await sendreq('/room/match/accept', { room_code: code, player_id: fm.p2, match_index: fm.match_index });
    assert(finalAcc.ok && finalAcc.match_started, 'Finals match started');

    const finalEnd = await sendreq('/room/match/end', {
        room_code: code, winner_id: fm.p1, match_index: fm.match_index
    });
    assert(finalEnd.ok, 'Finals result reported');

    const finalBracket = await sendreq(`/room/${code}/bracket`, null, 'GET');
    const championEntry = finalBracket.bracket.find(b => b.round === 1);
    assert(championEntry && championEntry.winner_id === fm.p1, `Champion: ${fm.p1}`);

    // --- Test 8: Pause/Resume ---
    console.log('\n--- Test 8: Pause/Resume ---');
    // Create a new tournament for pause test
    const pauseRoom = await sendreq('/room/create', {
        player_id: 'host', display_name: 'Host', name: 'Pause Test',
        room_type: 2, tournament_format: 0, max_players: 8, ft: 2, seeding: 'join_order'
    });
    const pauseCode = pauseRoom.room_code;
    await sendreq('/room/join', { player_id: 'p2', display_name: 'P2', room_code: pauseCode });

    const pauseRes = await sendreq(`/room/${pauseCode}/bracket/pause`, { player_id: 'host', pause: true });
    assert(pauseRes.ok && pauseRes.paused === true, 'Tournament paused');

    const resumeRes = await sendreq(`/room/${pauseCode}/bracket/pause`, { player_id: 'host', pause: false });
    assert(resumeRes.ok && resumeRes.paused === false, 'Tournament resumed');

    // Non-host can't pause
    const badPause = await sendreq(`/room/${pauseCode}/bracket/pause`, { player_id: 'p2', pause: true });
    assert(badPause.status === 403, 'Non-host pause rejected');

    // --- Test 9: DQ ---
    console.log('\n--- Test 9: DQ test ---');
    // Create tournament with 4 players, start it, DQ a player
    const dqRoom = await sendreq('/room/create', {
        player_id: 'host', display_name: 'Host', name: 'DQ Test',
        room_type: 2, tournament_format: 0, max_players: 8, ft: 2, seeding: 'join_order'
    });
    const dqCode = dqRoom.room_code;
    for (const id of ['p2', 'p3', 'p4']) {
        await sendreq('/room/join', { player_id: id, display_name: id.toUpperCase(), room_code: dqCode });
    }
    await sendreq(`/room/${dqCode}/bracket/start`, { player_id: 'host' });

    // DQ p3
    const dqRes = await sendreq(`/room/${dqCode}/bracket/dq`, { player_id: 'host', target_player_id: 'p3' });
    assert(dqRes.ok, 'p3 DQ\'d');

    // Verify bracket entry involving p3 auto-advanced
    const dqBracket = await sendreq(`/room/${dqCode}/bracket`, null, 'GET');
    const p3Match = dqBracket.bracket.find(b => b.player1_id === 'p3' || b.player2_id === 'p3');
    assert(p3Match && p3Match.completed === 1, 'p3\'s match auto-completed');
    assert(p3Match.winner_id !== 'p3', 'p3\'s opponent advanced');

    // Can't DQ twice
    const dupDQ = await sendreq(`/room/${dqCode}/bracket/dq`, { player_id: 'host', target_player_id: 'p3' });
    assert(dupDQ.status === 400, 'Duplicate DQ rejected');

    // --- Test 10: Double Elimination Full Lifecycle ---
    console.log('\n--- Test 10: Double Elimination ---');
    const deRoom = await sendreq('/room/create', {
        player_id: 'host', display_name: 'Host', name: 'Double Elim Test',
        room_type: 2, tournament_format: 1, max_players: 8, ft: 2, seeding: 'join_order'
    });
    assert(deRoom.ok, 'DE room created');
    const deCode = deRoom.room_code;
    for (const id of ['p2', 'p3', 'p4']) {
        await sendreq('/room/join', { player_id: id, display_name: id.toUpperCase(), room_code: deCode });
    }

    // Start bracket
    const deStart = await sendreq(`/room/${deCode}/bracket/start`, { player_id: 'host' });
    assert(deStart.ok, 'DE bracket started');
    const deState = deStart.bracket;

    // Verify bracket structure has W, L, and GF entries
    const wEntries = deState.bracket.filter(b => b.bracket_side === 'W');
    const lEntries = deState.bracket.filter(b => b.bracket_side === 'L');
    const gfEntries = deState.bracket.filter(b => b.bracket_side === 'GF');
    assert(wEntries.length >= 3, `Winners bracket has ${wEntries.length} entries (expected ≥3)`);
    assert(lEntries.length >= 2, `Losers bracket has ${lEntries.length} entries (expected ≥2)`);
    assert(gfEntries.length === 1, 'Grand Finals entry exists');
    console.log(`   W:${wEntries.length} L:${lEntries.length} GF:${gfEntries.length}`);

    // Helper to play all proposed matches (accept + end, p1 always wins)
    async function playAllMatches(roomCode) {
        const state = await sendreq(`/room/${roomCode}/bracket`, null, 'GET');
        const proposed = state.matches.filter(m => m.active === 0); // proposed but not active
        let played = 0;
        for (const m of proposed) {
            // Accept
            await sendreq('/room/match/accept', { room_code: roomCode, player_id: m.p1, match_index: m.match_index });
            await sendreq('/room/match/accept', { room_code: roomCode, player_id: m.p2, match_index: m.match_index });
            // End (p1 wins)
            await sendreq('/room/match/end', { room_code: roomCode, winner_id: m.p1, match_index: m.match_index });
            played++;
        }
        return played;
    }

    // Play through all available matches iteratively until GF completes
    let iterations = 0;
    let gfCompleted = false;
    while (iterations < 10 && !gfCompleted) {
        const played = await playAllMatches(deCode);
        if (played === 0) break;
        iterations++;
        // Check if GF is complete
        const checkState = await sendreq(`/room/${deCode}/bracket`, null, 'GET');
        const gf = checkState.bracket.find(b => b.bracket_side === 'GF');
        gfCompleted = gf && gf.completed === 1;
    }

    // Verify tournament completed
    const deFinal = await sendreq(`/room/${deCode}/bracket`, null, 'GET');
    const gfEntry = deFinal.bracket.find(b => b.bracket_side === 'GF');
    assert(gfEntry && gfEntry.completed === 1, 'Grand Finals completed');
    assert(gfEntry.winner_id, `DE Champion: ${gfEntry.winner_id}`);

    // Verify all W bracket matches completed
    const allWDone = deFinal.bracket.filter(b => b.bracket_side === 'W' && b.player1_id && b.player2_id).every(b => b.completed);
    assert(allWDone, 'All winners bracket matches completed');

    console.log(`   DE tournament completed in ${iterations} match waves`);

    // --- Test 11: Swiss Full Lifecycle ---
    console.log('\n--- Test 11: Swiss Format ---');
    const swRoom = await sendreq('/room/create', {
        player_id: 'host', display_name: 'Host', name: 'Swiss Test',
        room_type: 2, tournament_format: 3, max_players: 8, ft: 2, seeding: 'join_order'
    });
    assert(swRoom.ok, 'Swiss room created');
    const swCode = swRoom.room_code;
    for (const id of ['p2', 'p3', 'p4']) {
        await sendreq('/room/join', { player_id: id, display_name: id.toUpperCase(), room_code: swCode });
    }

    // Start bracket
    const swStart = await sendreq(`/room/${swCode}/bracket/start`, { player_id: 'host' });
    assert(swStart.ok, 'Swiss bracket started');
    const swState = swStart.bracket;
    assert(swState.total_rounds === 2, `Swiss rounds = ceil(log2(4)) = 2 (got ${swState.total_rounds})`);
    assert(swState.started === 1, 'Swiss marked as started');

    // Round 0: all players have 0 wins — initial pairings
    let swBracket = await sendreq(`/room/${swCode}/bracket`, null, 'GET');
    let swMatches = swBracket.matches.filter(m => m.round === 0);
    assert(swMatches.length === 2, `Round 0: 2 matches (got ${swMatches.length})`);
    console.log(`   Round 0: ${swMatches.map(m => `${m.p1} vs ${m.p2}`).join(', ')}`);

    // Play round 0: p1 always wins
    for (const m of swMatches) {
        await sendreq('/room/match/accept', { room_code: swCode, player_id: m.p1, match_index: m.match_index });
        await sendreq('/room/match/accept', { room_code: swCode, player_id: m.p2, match_index: m.match_index });
        await sendreq('/room/match/end', { room_code: swCode, winner_id: m.p1, match_index: m.match_index });
    }

    // Verify round advanced to 1
    await sleep(200);
    swBracket = await sendreq(`/room/${swCode}/bracket`, null, 'GET');
    assert(swBracket.round === 1, `Advanced to round 1 (got ${swBracket.round})`);

    // Round 1: winners (1 win) face each other, losers (0 wins) face each other
    let swR1Matches = swBracket.matches.filter(m => m.round === 1);
    assert(swR1Matches.length === 2, `Round 1: 2 matches (got ${swR1Matches.length})`);
    console.log(`   Round 1: ${swR1Matches.map(m => `${m.p1} vs ${m.p2}`).join(', ')}`);

    // Verify standings-based pairing: round 0 winners should play each other
    const r0Winners = swMatches.map(m => m.p1); // p1 always won
    const r1Match1Players = [swR1Matches[0].p1, swR1Matches[0].p2];
    const winnersInMatch1 = r0Winners.filter(w => r1Match1Players.includes(w)).length;
    // Either match should have both winners or both losers, not mixed
    assert(winnersInMatch1 === 2 || winnersInMatch1 === 0,
        `Swiss pairing: R1 match 1 has ${winnersInMatch1} R0-winners (expected 2 or 0 for proper pairing)`);

    // Play round 1
    for (const m of swR1Matches) {
        await sendreq('/room/match/accept', { room_code: swCode, player_id: m.p1, match_index: m.match_index });
        await sendreq('/room/match/accept', { room_code: swCode, player_id: m.p2, match_index: m.match_index });
        await sendreq('/room/match/end', { room_code: swCode, winner_id: m.p1, match_index: m.match_index });
    }

    // Verify all bracket entries completed
    await sleep(200);
    const swFinal = await sendreq(`/room/${swCode}/bracket`, null, 'GET');
    const allSwDone = swFinal.bracket.every(b => b.completed);
    assert(allSwDone, 'All Swiss bracket entries completed');
    console.log(`   Swiss tournament completed (${swFinal.bracket.length} total matches)`);

    // --- Test 12: Restart Match API ---
    console.log('\n--- Test 12: Restart Match API ---');
    const rsRoom = await sendreq('/room/create', {
        player_id: 'host', display_name: 'Host', name: 'Restart Test',
        room_type: 2, tournament_format: 0, max_players: 8, ft: 2, seeding: 'join_order'
    });
    const rsCode = rsRoom.room_code;
    for (const id of ['p2', 'p3', 'p4']) {
        await sendreq('/room/join', { player_id: id, display_name: id.toUpperCase(), room_code: rsCode });
    }
    await sendreq(`/room/${rsCode}/bracket/start`, { player_id: 'host' });

    // End a match
    let rsBracket = await sendreq(`/room/${rsCode}/bracket`, null, 'GET');
    let targetMatch = rsBracket.matches[0];
    await sendreq('/room/match/accept', { room_code: rsCode, player_id: targetMatch.p1, match_index: targetMatch.match_index });
    await sendreq('/room/match/accept', { room_code: rsCode, player_id: targetMatch.p2, match_index: targetMatch.match_index });
    await sendreq('/room/match/end', { room_code: rsCode, winner_id: targetMatch.p1, match_index: targetMatch.match_index });

    // Verify it ended
    rsBracket = await sendreq(`/room/${rsCode}/bracket`, null, 'GET');
    let entry = rsBracket.bracket.find(b => b.round === targetMatch.round && b.position === targetMatch.position);
    assert(entry.completed === 1, 'Match completed in bracket');

    // Restart the match
    const restartRes = await sendreq(`/room/${rsCode}/bracket/restart`, { player_id: 'host', match_index: targetMatch.match_index });
    assert(restartRes.ok, 'Restart match API call succeeded');

    // Verify it reset
    rsBracket = await sendreq(`/room/${rsCode}/bracket`, null, 'GET');
    entry = rsBracket.bracket.find(b => b.round === targetMatch.round && b.position === targetMatch.position);
    assert(entry.completed === 0, 'Bracket entry reset to uncompleted');
    assert(entry.winner_id === '', 'Bracket entry winner cleared');

    targetMatch = rsBracket.matches.find(m => m.match_index === targetMatch.match_index);
    assert(targetMatch, 'Restarted match is back in active pool');
    assert(targetMatch.active === 0, 'Match is ready (proposed)');

    // --- Cleanup ---
    console.log('\n--- Cleanup ---');
    for (const id of ['host', 'p2', 'p3', 'p4']) {
        await sendreq('/room/leave', { player_id: id, room_code: code });
        await sendreq('/room/leave', { player_id: id, room_code: pauseCode });
        await sendreq('/room/leave', { player_id: id, room_code: dqCode });
        await sendreq('/room/leave', { player_id: id, room_code: deCode });
        await sendreq('/room/leave', { player_id: id, room_code: swCode });
        await sendreq('/room/leave', { player_id: id, room_code: rsCode });
    }

    console.log('\n=== All tournament tests passed! ===');
}

run().catch(err => {
    console.error('Test error:', err);
    process.exit(1);
});
