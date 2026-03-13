const crypto = require('crypto');
const http = require('http');

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
    const code = createRes.room_code;
    console.log("Room created:", code);
    
    console.log("Connecting to SSE stream...");
    
    // Manual HTTP GET to strictly read the stream
    const req = http.get(`http://localhost:3000/room/events?room_code=${code}`, (res) => {
        console.log("SSE Connected, status:", res.statusCode);
        
        res.on('data', (chunk) => {
            console.log("\n--- SSE Event Received ---");
            console.log(chunk.toString());
        });
    });

    // Wait 1 second, then trigger an event
    setTimeout(async () => {
        console.log("Triggering chat event...");
        await sendreq("/room/chat", { player_id: "p1", display_name: "Host Player", room_code: code, text: "Streaming test!" });
    }, 1000);

    // Wait another second, then close
    setTimeout(() => {
        console.log("Closing test.");
        req.destroy();
        process.exit(0);
    }, 2000);
}

run().catch(console.error);
