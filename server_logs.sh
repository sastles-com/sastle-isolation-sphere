#!/bin/bash
# Isolation Server のログを表示

echo "=== Isolation Server ログ ==="
echo "終了するには Ctrl+C を押してください"
echo ""

journalctl -u isolation-server -f --no-pager
