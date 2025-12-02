#!/bin/bash
# Test brightness API endpoint

echo "Testing brightness API endpoint..."
curl -X POST http://100.88.207.117:9000/api/command/params \
  -H "Content-Type: application/json" \
  -d '{"brightness": 75}' \
  -v

echo -e "\n\nDone. Check mosquitto_sub for state update."
