const crypto = require('crypto');

async function sendreq(path, body, overrideMethod) {
    const timestamp = Math.floor(Date.now() / 1000).toString();
    const bodyStr = body ? JSON.stringify(body) : "";
    const method = overrideMethod || 'POST';
    const hmac = crypto.createHmac('sha256', process.env.LOBBY_SECRET || 'test');
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

    const res = await fetch(`http://localhost:3000${path}`, reqOpts);
    return await res.json();
}

async function run() {
    console.log("Creating room...");
    const createRes = await sendreq("/room/create", { player_id: "p1", display_name: "Host Player" });
    console.log(createRes);
    const code = createRes.room_code;
    
    console.log("Joining room...");
    const joinRes = await sendreq("/room/join", { player_id: "p2", display_name: "Guest Player", room_code: code });
    console.log(joinRes);
    
    console.log("Setting presence for p2...");
    const presenceRes = await sendreq("/presence", { player_id: "p2", display_name: "Guest Player", room_code: code, status: "idle" });
    console.log(presenceRes);

    console.log("Sending chat 1...");
    const chatRes1 = await sendreq("/room/chat", { player_id: "p2", display_name: "Guest Player", room_code: code, text: "Hello from test!" });
    console.log(chatRes1);
    
    console.log("Sending chat 2 (spam - should be 429)...");
    const chatRes2 = await sendreq("/room/chat", { player_id: "p2", display_name: "Guest Player", room_code: code, text: "Spam!" });
    console.log(chatRes2);

    console.log("Waiting 3.1s for rate limit to expire...");
    await new Promise(r => setTimeout(r, 3100));

    console.log("Sending chat 3 (profanity check)...");
    const chatRes3 = await sendreq("/room/chat", { player_id: "p2", display_name: "Guest Player", room_code: code, text: "This is a fuck test" });
    console.log(chatRes3);

    console.log("Checking room state for chat history (should be empty)...");
    const stateRes = await sendreq("/room/state?room_code=" + code, null, "GET");
    console.log({ chat: stateRes.chat });
    
    console.log("Leaving room...");
    const leaveRes = await sendreq("/room/leave", { player_id: "p1", room_code: code });
    console.log(leaveRes);
}

run().catch(console.error);
