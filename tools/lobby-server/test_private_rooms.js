const crypto = require('crypto');
const http = require('http');

const SECRET = 'testsecret';

function signPayload(method, path, body) {
    const timestamp = Math.floor(Date.now() / 1000).toString();
    const hmac = crypto.createHmac('sha256', SECRET);
    hmac.update(timestamp + method + path + body);
    return {
        'x-timestamp': timestamp,
        'x-signature': hmac.digest('hex'),
        'Content-Type': 'application/json'
    };
}

function request(method, path, data = null) {
    return new Promise((resolve, reject) => {
        const bodyStr = data ? JSON.stringify(data) : '';
        const headers = signPayload(method, path, bodyStr);

        const options = {
            hostname: '127.0.0.1',
            port: 3000,
            path: path,
            method: method,
            headers: headers
        };

        const req = http.request(options, res => {
            let resBody = '';
            res.on('data', chunk => resBody += chunk);
            res.on('end', () => {
                try {
                    resolve({ status: res.statusCode, data: JSON.parse(resBody) });
                } catch {
                    resolve({ status: res.statusCode, data: resBody });
                }
            });
        });

        req.on('error', reject);
        if (data) req.write(bodyStr);
        req.end();
    });
}

async function test() {
    console.log("Creating public room...");
    const publicRes = await request('POST', '/room/create', { player_id: 'p1', name: 'Public Room' });
    console.log(publicRes.data);

    console.log("Creating password room...");
    const passRes = await request('POST', '/room/create', { player_id: 'p2', name: 'Pass Room', password: '123' });
    console.log(passRes.data);
    const passCode = passRes.data.room_code;

    console.log("Creating hidden room...");
    const hiddenRes = await request('POST', '/room/create', { player_id: 'p3', name: 'Hidden Room', visibility: 1 });
    console.log(hiddenRes.data);

    console.log("Listing rooms...");
    const listRes = await request('GET', '/rooms/list');
    console.log("List has " + listRes.data.rooms.length + " rooms.");
    const hiddenInList = listRes.data.rooms.some(r => r.name === 'Hidden Room');
    console.log("Hidden Room in list? " + hiddenInList);
    const passInList = listRes.data.rooms.find(r => r.name === 'Pass Room');
    console.log("Pass Room requires password? " + (passInList ? passInList.password_required : 'N/A'));

    console.log("Joining password room with wrong pass...");
    const join1 = await request('POST', '/room/join', { player_id: 'p4', room_code: passCode, password: 'wrong' });
    console.log(join1);

    console.log("Joining password room with CORRECT pass...");
    const join2 = await request('POST', '/room/join', { player_id: 'p4', room_code: passCode, password: '123' });
    console.log(join2.status, join2.data.ok);

    console.log("Tests Complete.");
}

test().catch(console.error);
