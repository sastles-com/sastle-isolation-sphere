#!/bin/bash
# Isolation Server を再起動

echo "Isolation Server を再起動しています..."
sudo systemctl restart isolation-server
sleep 2
echo ""
systemctl status isolation-server --no-pager
