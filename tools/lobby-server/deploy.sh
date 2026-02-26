#!/bin/bash
# Deploy the 3SX lobby server to the Oracle VPS.
# Usage: ./deploy.sh
#
# Prerequisites:
#   - SSH key at D:\oraclekey (or key configured in ~/.ssh/config)
#   - Node.js installed on the VPS: sudo apt install -y nodejs
#   - Port 8080 open in VPS firewall + Oracle Cloud security list

set -e

VPS_USER="ubuntu"
VPS_HOST="152.67.75.184"
VPS_KEY="D:/oraclekey"
REMOTE_DIR="/home/ubuntu/lobby-server"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Deploying 3SX Lobby Server ==="

# Create remote directory
ssh -i "$VPS_KEY" "$VPS_USER@$VPS_HOST" "mkdir -p $REMOTE_DIR"

# Copy server files
scp -i "$VPS_KEY" "$SCRIPT_DIR/lobby-server.js" "$VPS_USER@$VPS_HOST:$REMOTE_DIR/"
scp -i "$VPS_KEY" "$SCRIPT_DIR/lobby-server.service" "$VPS_USER@$VPS_HOST:$REMOTE_DIR/"
scp -i "$VPS_KEY" "$SCRIPT_DIR/.env" "$VPS_USER@$VPS_HOST:$REMOTE_DIR/"

# Install systemd service
ssh -i "$VPS_KEY" "$VPS_USER@$VPS_HOST" << 'EOF'
sudo cp /home/ubuntu/lobby-server/lobby-server.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable lobby-server
sudo systemctl restart lobby-server
echo "Service status:"
sudo systemctl status lobby-server --no-pager
EOF

echo "=== Deploy complete ==="
echo "Test: curl http://$VPS_HOST:3000/"
