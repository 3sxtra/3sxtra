const crypto = require('crypto');

async function sendreq(path, body) {
    const timestamp = Math.floor(Date.now() / 1000).toString();
    const bodyStr = JSON.stringify(body);
    const hmac = crypto.createHmac('sha256', 'testsecret');
    hmac.update(timestamp + 'POST' + path + bodyStr);
    
    const res = await fetch(`http://localhost:3000${path}`, {
        method: 'POST',
        headers: {
            'X-Timestamp': timestamp,
            'X-Signature': hmac.digest('hex'),
            'Content-Type': 'application/json'
        },
        body: bodyStr
    });
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
    
    console.log("Sending chat...");
    const chatRes = await sendreq("/room/chat", { player_id: "p2", display_name: "Guest Player", room_code: code, text: "Hello from test!" });
    console.log(chatRes);
    
    console.log("Leaving room...");
    const leaveRes = await sendreq("/room/leave", { player_id: "p1", room_code: code });
    console.log(leaveRes);
}

run().catch(console.error);
