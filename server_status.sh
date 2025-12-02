#!/bin/bash
# Isolation Server の状態確認

echo "=== Isolation Server 状態 ==="
systemctl status isolation-server --no-pager -l

echo ""
echo "=== 最新ログ (20行) ==="
journalctl -u isolation-server -n 20 --no-pager
