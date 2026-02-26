#!/bin/bash
# Integration tests for the 3SX Lobby Server.
# Usage: bash test-server.sh
#
# Starts the server with a test secret, runs tests, then stops it.
# Requires: node, curl, openssl (or any OS with /dev/urandom)

set -e

SECRET="test-secret-key-for-ci"
PORT=9876
PASS=0
FAIL=0
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# --- Helpers ---

sign_request() {
    local method="$1" path="$2" body="$3"
    local ts
    ts=$(date +%s)
    local payload="${ts}${method}${path}${body}"
    local sig
    sig=$(printf '%s' "$payload" | openssl dgst -sha256 -hmac "$SECRET" -hex 2>/dev/null | sed 's/^.* //')
    echo "$ts $sig"
}

do_request() {
    local method="$1" path="$2" body="$3" expect_status="$4" label="$5"
    local ts sig auth
    auth=$(sign_request "$method" "$path" "$body")
    ts=$(echo "$auth" | cut -d' ' -f1)
    sig=$(echo "$auth" | cut -d' ' -f2)

    local status
    if [ "$method" = "POST" ]; then
        status=$(curl -s -o /dev/null -w '%{http_code}' \
            -X POST "http://127.0.0.1:$PORT$path" \
            -H "Content-Type: application/json" \
            -H "X-Timestamp: $ts" \
            -H "X-Signature: $sig" \
            -d "$body" 2>/dev/null)
    else
        status=$(curl -s -o /dev/null -w '%{http_code}' \
            "http://127.0.0.1:$PORT$path" \
            -H "X-Timestamp: $ts" \
            -H "X-Signature: $sig" 2>/dev/null)
    fi

    if [ "$status" = "$expect_status" ]; then
        echo "  PASS: $label (HTTP $status)"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $label (expected $expect_status, got $status)"
        FAIL=$((FAIL + 1))
    fi
}

# --- Start server ---

echo "=== Starting test server on port $PORT ==="
LOBBY_SECRET="$SECRET" LOBBY_PORT="$PORT" node "$SCRIPT_DIR/lobby-server.js" &
SERVER_PID=$!
sleep 1

# Check server started
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "FATAL: Server failed to start"
    exit 1
fi

cleanup() {
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
}
trap cleanup EXIT

echo ""
echo "=== Running tests ==="

# 1. Health endpoint (no auth)
status=$(curl -s -o /dev/null -w '%{http_code}' "http://127.0.0.1:$PORT/" 2>/dev/null)
if [ "$status" = "200" ]; then
    echo "  PASS: Health endpoint (HTTP 200)"
    PASS=$((PASS + 1))
else
    echo "  FAIL: Health endpoint (expected 200, got $status)"
    FAIL=$((FAIL + 1))
fi

# 2. Missing auth headers → 403
status=$(curl -s -o /dev/null -w '%{http_code}' "http://127.0.0.1:$PORT/searching" 2>/dev/null)
if [ "$status" = "403" ]; then
    echo "  PASS: Missing auth → 403"
    PASS=$((PASS + 1))
else
    echo "  FAIL: Missing auth (expected 403, got $status)"
    FAIL=$((FAIL + 1))
fi

# 3. Bad signature → 403
status=$(curl -s -o /dev/null -w '%{http_code}' \
    "http://127.0.0.1:$PORT/searching" \
    -H "X-Timestamp: $(date +%s)" \
    -H "X-Signature: 0000000000000000000000000000000000000000000000000000000000000000" 2>/dev/null)
if [ "$status" = "403" ]; then
    echo "  PASS: Bad signature → 403"
    PASS=$((PASS + 1))
else
    echo "  FAIL: Bad signature (expected 403, got $status)"
    FAIL=$((FAIL + 1))
fi

# 4. Malformed signature (non-hex, DoS vector) → 403 (not crash)
status=$(curl -s -o /dev/null -w '%{http_code}' \
    "http://127.0.0.1:$PORT/searching" \
    -H "X-Timestamp: $(date +%s)" \
    -H "X-Signature: ZZZZ-not-hex-at-all" 2>/dev/null)
if [ "$status" = "403" ]; then
    echo "  PASS: Malformed hex signature → 403 (no crash)"
    PASS=$((PASS + 1))
else
    echo "  FAIL: Malformed hex signature (expected 403, got $status)"
    FAIL=$((FAIL + 1))
fi

# 5. Verify server still alive after malformed signature
if kill -0 $SERVER_PID 2>/dev/null; then
    echo "  PASS: Server survived malformed signature"
    PASS=$((PASS + 1))
else
    echo "  FAIL: Server CRASHED on malformed signature"
    FAIL=$((FAIL + 1))
fi

# 6. Register presence
do_request POST "/presence" \
    '{"player_id":"test1","display_name":"Alice","region":"US","room_code":"ROOM1","connect_to":""}' \
    200 "Register presence"

# 7. Start searching
do_request POST "/searching/start" \
    '{"player_id":"test1"}' \
    200 "Start searching"

# 8. Get searching players (authenticated)
do_request GET "/searching" "" 200 "Get searching players"

# 9. Stop searching
do_request POST "/searching/stop" \
    '{"player_id":"test1"}' \
    200 "Stop searching"

# 10. Leave
do_request POST "/leave" \
    '{"player_id":"test1"}' \
    200 "Leave lobby"

# 11. Search for non-existent player → 404
do_request POST "/searching/start" \
    '{"player_id":"ghost"}' \
    404 "Start searching unknown player → 404"

# 12. Invalid JSON → 400
do_request POST "/presence" \
    'not-json-at-all' \
    400 "Invalid JSON → 400"

# 13. Missing required fields → 400
do_request POST "/presence" \
    '{"player_id":"test2"}' \
    400 "Missing display_name → 400"

# 14. Test body size limit (>64KB)
big_body=$(printf '{"player_id":"x","display_name":"%0*d"}' 70000 0)
auth=$(sign_request "POST" "/presence" "$big_body")
ts=$(echo "$auth" | cut -d' ' -f1)
sig=$(echo "$auth" | cut -d' ' -f2)
status=$(curl -s -o /dev/null -w '%{http_code}' \
    -X POST "http://127.0.0.1:$PORT/presence" \
    -H "Content-Type: application/json" \
    -H "X-Timestamp: $ts" \
    -H "X-Signature: $sig" \
    -d "$big_body" 2>/dev/null)
if [ "$status" = "413" ] || [ "$status" = "000" ]; then
    echo "  PASS: Body >64KB rejected (HTTP $status)"
    PASS=$((PASS + 1))
else
    echo "  FAIL: Body >64KB (expected 413, got $status)"
    FAIL=$((FAIL + 1))
fi

# --- Results ---

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
echo "All tests passed!"
