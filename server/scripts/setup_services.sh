#!/bin/bash
set -e

USER="yakatano"
WORKING_DIR="/home/yakatano/work/m5atoms3r/repo/server"
PYTHON_EXEC="/home/yakatano/work/m5atoms3r/repo/server/.venv/bin/python"
UVICORN_EXEC="/home/yakatano/work/m5atoms3r/repo/server/.venv/bin/uvicorn"

# Ensure virtualenv exists
if [ ! -d "$WORKING_DIR/.venv" ]; then
    echo "Creating virtual environment..."
    cd "$WORKING_DIR"
    uv venv
    uv pip install -e .
fi

# 1. Create Web App Service
echo "Creating isolation-server.service..."
cat <<EOF | sudo tee /etc/systemd/system/isolation-server.service
[Unit]
Description=Isolation Sphere Web Server
After=network.target

[Service]
User=$USER
WorkingDirectory=$WORKING_DIR
ExecStart=$UVICORN_EXEC app.main:app --host 0.0.0.0 --port 9000
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

# 2. Create Joystick Daemon Service
echo "Creating isolation-joystick.service..."
cat <<EOF | sudo tee /etc/systemd/system/isolation-joystick.service
[Unit]
Description=Isolation Sphere Joystick Daemon
After=network.target isolation-server.service

[Service]
User=$USER
WorkingDirectory=$WORKING_DIR
ExecStart=$PYTHON_EXEC joystick/daemon.py
Restart=always
RestartSec=5
Environment="PYTHONPATH=$WORKING_DIR"

[Install]
WantedBy=multi-user.target
EOF

# 3. Reload and Enable
echo "Reloading systemd..."
sudo systemctl daemon-reload
echo "Enabling services..."
sudo systemctl enable isolation-server
sudo systemctl enable isolation-joystick

echo "Service setup complete."
