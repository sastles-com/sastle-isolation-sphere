#!/usr/bin/env bash
#
# setup_p2p_ap.sh — ESP32 専用 P2P AP を Ubuntu サーバー機に構築する
#
# USB WiFi ドングル (Realtek RTL88x2BU 等) を AP モードで動かし、
# ESP32 (M5AtomS3R) が config.json の wifi 設定で接続できる
# 隔離ネットワーク + MQTT ブローカーを立ち上げる。
#
# 2026-06-13 に NucBox-G10 (Ubuntu 24.04) で構築・稼働確認した手順をスクリプト化したもの。
# 詳細な背景は docs/HANDOFF_2026-06-13_bench_bringup.md を参照。
#
# 使い方:
#   sudo ./setup_p2p_ap.sh          # AP を構築・起動
#   sudo ./setup_p2p_ap.sh --down   # AP を停止 (サービス無効化・IF解放)
#
# 既定値は core/data/config.json の旧 192.168.49.x 体系に合わせてある。
# 変更する場合は下の環境変数を上書きする:
#   AP_SSID / AP_PASS / AP_IP / AP_CIDR / AP_CHANNEL / AP_COUNTRY
set -euo pipefail

AP_SSID="${AP_SSID:-ESP32-P2P-Direct}"
AP_PASS="${AP_PASS:-isolation-sphere-p2p}"
AP_IP="${AP_IP:-192.168.49.1}"
AP_CIDR="${AP_CIDR:-24}"
AP_DHCP_FROM="${AP_DHCP_FROM:-192.168.49.50}"
AP_DHCP_TO="${AP_DHCP_TO:-192.168.49.100}"
AP_CHANNEL="${AP_CHANNEL:-6}"
AP_COUNTRY="${AP_COUNTRY:-JP}"

if [[ $EUID -ne 0 ]]; then echo "root 権限で実行してください (sudo)"; exit 1; fi

# --- USB WiFi ドングルの IF を検出 (内蔵 wlp* ではなく wlx* / rtw88 系) ---
detect_iface() {
  for d in /sys/class/net/wl*; do
    [[ -e "$d" ]] || continue
    local name drv
    name=$(basename "$d")
    drv=$(basename "$(readlink -f "$d/device/driver" 2>/dev/null)" 2>/dev/null || true)
    # 内蔵 (wlp*) を除外し、USB ドングル (wlx* もしくは rtw88 ドライバ) を採用
    if [[ "$name" == wlx* || "$drv" == rtw88* || "$drv" == *88x2* || "$drv" == *8812* ]]; then
      echo "$name"; return 0
    fi
  done
  return 1
}

IFACE="${AP_IFACE:-$(detect_iface || true)}"
if [[ -z "${IFACE}" ]]; then
  echo "USB WiFi ドングルの IF が見つかりません。lsusb / dmesg で列挙を確認してください。"
  echo "（error -71 が出る場合は別の USB ポートに挿し替える）"
  exit 1
fi
MAC=$(cat "/sys/class/net/${IFACE}/address")
echo "対象 IF: ${IFACE} (MAC ${MAC})"

if [[ "${1:-}" == "--down" ]]; then
  systemctl disable --now hostapd dnsmasq ap-ip.service 2>/dev/null || true
  rm -f /etc/dnsmasq.d/ap-bench.conf /etc/systemd/system/ap-ip.service \
        /etc/NetworkManager/conf.d/99-unmanaged-ap.conf
  systemctl daemon-reload
  nmcli dev set "${IFACE}" managed yes 2>/dev/null || true
  systemctl reload NetworkManager 2>/dev/null || true
  ip addr flush dev "${IFACE}" 2>/dev/null || true
  echo "AP を停止しました。"
  exit 0
fi

# --- 必要パッケージ ---
export DEBIAN_FRONTEND=noninteractive
apt-get install -y hostapd dnsmasq mosquitto mosquitto-clients iw >/dev/null

# --- hostapd ---
cat > /etc/hostapd/hostapd.conf <<EOF
interface=${IFACE}
driver=nl80211
ssid=${AP_SSID}
hw_mode=g
channel=${AP_CHANNEL}
ieee80211n=1
wmm_enabled=1
auth_algs=1
wpa=2
wpa_passphrase=${AP_PASS}
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
country_code=${AP_COUNTRY}
EOF
echo 'DAEMON_CONF="/etc/hostapd/hostapd.conf"' > /etc/default/hostapd

# --- dnsmasq (この IF 専用 DHCP。systemd-resolved と衝突しないよう bind-interfaces) ---
cat > /etc/dnsmasq.d/ap-bench.conf <<EOF
interface=${IFACE}
bind-interfaces
dhcp-range=${AP_DHCP_FROM},${AP_DHCP_TO},255.255.255.0,12h
dhcp-option=3,${AP_IP}
dhcp-option=6,${AP_IP}
EOF

# --- mosquitto (匿名許可・全 IF で待受) ---
cat > /etc/mosquitto/conf.d/bench.conf <<EOF
listener 1883 0.0.0.0
allow_anonymous true
EOF

# --- NetworkManager にドングルを触らせない ---
cat > /etc/NetworkManager/conf.d/99-unmanaged-ap.conf <<EOF
[keyfile]
unmanaged-devices=mac:${MAC}
EOF

# --- 静的 IP 付与 (systemd oneshot、hostapd/dnsmasq より前に実行) ---
cat > /etc/systemd/system/ap-ip.service <<EOF
[Unit]
Description=Bench AP static IP (${IFACE})
Before=hostapd.service dnsmasq.service
[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/bin/bash -c "/sbin/ip addr flush dev ${IFACE}; /sbin/ip addr add ${AP_IP}/${AP_CIDR} dev ${IFACE}; /sbin/ip link set ${IFACE} up"
ExecStop=/sbin/ip addr flush dev ${IFACE}
[Install]
WantedBy=multi-user.target
EOF

# --- 起動 ---
nmcli dev set "${IFACE}" managed no 2>/dev/null || true
systemctl reload NetworkManager 2>/dev/null || true
sleep 1
systemctl daemon-reload
systemctl enable --now ap-ip.service
rfkill unblock wlan 2>/dev/null || true
systemctl unmask hostapd
systemctl enable hostapd
systemctl restart hostapd
sleep 2
systemctl restart dnsmasq
systemctl restart mosquitto

echo "=== 構築完了。状態 ==="
for s in ap-ip hostapd dnsmasq mosquitto; do printf "  %-10s %s\n" "$s" "$(systemctl is-active "$s")"; done
iw dev "${IFACE}" info 2>/dev/null | grep -E "ssid|type|channel" | sed 's/^/  /'
ip -br addr show "${IFACE}" | sed 's/^/  /'
echo "MQTT 確認: mosquitto_sub -h ${AP_IP} -t '#' -v"
echo "※ AP を後から立てた場合、デバイスは WiFi 失敗で停止したままなのでリセットが必要"
