#!/bin/bash
# Deploy the 3SX lobby server to the Oracle VPS.
# Usage: ./deploy.sh
#
# Prerequisites:
#   - SSH key at D:\oraclekey (or key configured in ~/.ssh/config)
#   - Node.js installed on the VPS: sudo apt install -y nodejs npm
#   - Port 8080 open in VPS firewall + Oracle Cloud security list

set -e

VPS_USER="ubuntu"
VPS_HOST="152.67.75.184"
VPS_KEY="D:\\oraclekey"
REMOTE_DIR="/home/ubuntu/lobby-server"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Deploying 3SX Lobby Server ===" 

# Create remote directory
ssh -i "$VPS_KEY" "$VPS_USER@$VPS_HOST" "mkdir -p $REMOTE_DIR"

# Copy server files
scp -i "$VPS_KEY" "$SCRIPT_DIR/lobby-server.js" "$VPS_USER@$VPS_HOST:$REMOTE_DIR/"
scp -i "$VPS_KEY" "$SCRIPT_DIR/package.json" "$VPS_USER@$VPS_HOST:$REMOTE_DIR/"
scp -i "$VPS_KEY" "$SCRIPT_DIR/lobby-server.service" "$VPS_USER@$VPS_HOST:$REMOTE_DIR/"
scp -i "$VPS_KEY" "$SCRIPT_DIR/.env" "$VPS_USER@$VPS_HOST:$REMOTE_DIR/"

# Install dependencies and restart service
ssh -i "$VPS_KEY" "$VPS_USER@$VPS_HOST" << 'EOF'
cd /home/ubuntu/lobby-server

# Ensure build tools exist (needed for better-sqlite3 native bindings)
if ! dpkg -s build-essential > /dev/null 2>&1; then
    echo "Installing build-essential for native modules..."
    sudo apt-get update -qq && sudo apt-get install -y -qq build-essential python3
fi

echo "Installing dependencies..."
npm install --production
echo ""
sudo cp /home/ubuntu/lobby-server/lobby-server.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable lobby-server
sudo systemctl restart lobby-server
sleep 2
echo "Service status:"
sudo systemctl status lobby-server --no-pager
echo ""
echo "Quick health check:"
curl -s http://localhost:3000/ || echo "(health check failed)"
echo ""
echo "SQLite check:"
ls -la /home/ubuntu/lobby-server/lobby.db 2>/dev/null || echo "(lobby.db will be created on first match report)"
EOF

echo "=== Deploy complete ==="
echo "Test: curl http://$VPS_HOST:3000/"
