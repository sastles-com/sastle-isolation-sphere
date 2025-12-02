#!/bin/bash
# サービスファイルを正しいパスで修正

cat <<'EOF' | sudo tee /etc/systemd/system/isolation-server.service
[Unit]
Description=Isolation Sphere Web Server
After=network.target

[Service]
User=yakatano
WorkingDirectory=/home/yakatano/work/m5atoms3r/repo/server
ExecStart=/home/yakatano/.local/bin/uvicorn app.main:app --host 0.0.0.0 --port 9000
Restart=always
RestartSec=5
Environment="PATH=/home/yakatano/.local/bin:/usr/bin"

[Install]
WantedBy=multi-user.target
EOF

echo "サービスファイルを更新しました"
sudo systemctl daemon-reload
echo "設定を再読み込みしました"
sudo systemctl restart isolation-server
echo "サービスを再起動しました"
sleep 2
sudo systemctl status isolation-server --no-pager
