# Network Lobby: QR Code Join Architecture

## Overview
This document outlines the architecture for the "QR Code Remote Join" feature, which allows players to easily join hidden, password-protected, or tournament rooms by scanning a QR code with their smartphone.

## The Goal
1. User A (Host) creates a private room and a QR code appears on their screen.
2. User A shares a picture of this QR code with User B (Guest) via Discord or text.
3. User B scans the QR code with their phone's native camera app.
4. User B's PC (running 3SXtra on the same Wi-Fi network) instantly joins the room.

No dedicated mobile app is required. Everything runs through standard web URLs and the existing NodeJS lobby server.

## Architecture: Public IP Relay

To bridge the gap between a mobile web browser and a local C application without running into local firewall issues, we leverage the existing Lobby Server as a relay.

### How It Works

1. **Client Connection:** 
   When the 3SXtra client connects to the Lobby Server, the server already tracks the client's Public IP address (via `req.socket.remoteAddress`).

2. **QR Code Generation:**
   The game generates a QR code containing a standardized URL:
   `https://lobby-server-url/qr-join?room=HADOKEN&pw=1234`

3. **The Phone Scan:**
   The native mobile camera scans the QR code and opens the link in Safari/Chrome. 

4. **The Relay (Server-Side):**
   The NodeJS Lobby Server receives the request from the phone.
   - It looks at the incoming Public IP of the phone.
   - It searches its active connections for any 3SXtra P2P clients connected from that **exact same Public IP**.
   - If it finds a match, it pushes a Server-Sent Event (SSE) to that specific 3SXtra client:
     `event: REMOTE_JOIN`
     `data: {"room": "HADOKEN", "password": "1234"}`

5. **The Execution (Client-Side):**
   The 3SXtra client receives the `REMOTE_JOIN` event in its background SSE thread. It immediately triggers `LobbyServer_JoinRoom("HADOKEN", "1234")`, pulling the player into the room on their PC.

### Handling Edge Cases

- **Mobile Data (5G/4G):**
  If the phone is on mobile data, its Public IP will not match the PC's Wi-Fi Public IP. The web page should gracefully display:
  > *"We couldn't find your 3SXtra instance. Make sure your phone is connected to the same Wi-Fi as your PC!"*

- **Multiple PCs on the Same Network:**
  If a dorm or household has multiple PCs running 3SXtra on the same public IP, the server will find multiple active clients. In this case, the web page should return a prompt:
  > *"We found multiple players on your network. Which screen are you on?"*
  > *[Ryu's PC] [Dov's PC]*
  The user taps their profile, and the server pushes the event to that specific client ID.

## Implementation Steps

### 1. Server-Side (NodeJS)
Add a new HTTP endpoint to `lobby-server.js`:
```javascript
app.get('/qr-join', (req, res) => {
    const roomCode = req.query.room;
    const clientIp = req.socket.remoteAddress;
    
    // Find all clients matching this IP
    const matchingClients = activeClients.filter(c => c.ip === clientIp);
    
    if (matchingClients.length === 1) {
        // Direct match, push the event
        sendSSE(matchingClients[0], 'REMOTE_JOIN', { room: roomCode, password: req.query.pw });
        res.send("<h1>Success!</h1><p>Check your PC.</p>");
    } else if (matchingClients.length > 1) {
        // Return a UI to let them pick which client they are
        res.send(renderClientSelectionUI(matchingClients));
    } else {
        res.send("<h1>Error</h1><p>Could not find your PC. Are you on the same Wi-Fi?</p>");
    }
});
```

### 2. Client-Side (C / RmlUi)
In the SSE listener (e.g., `lobby_server.c`), add the handler:
```c
if (strcmp(event_name, "REMOTE_JOIN") == 0) {
    // Parse room and password from JSON, then execute join
    LobbyServer_JoinRoom(parsed_room, parsed_pw, on_join_callback);
}
```

### 3. QR Generation (C / RmlUi)
Use a lightweight C library like `qrcodegen` (already available in many public domain repos) to generate the QR matrix. Render it to an SDL texture and display it in the RmlUi layout when a private room is created.
