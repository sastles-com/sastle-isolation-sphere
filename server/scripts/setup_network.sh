#!/bin/bash
set -e

# Configuration
INTERFACE="wlx90de8068da46"
SSID="ESP32-P2P-Direct"
PASSWORD="isolation-sphere-p2p"
IP_ADDRESS="192.168.49.1/24"

# Check if interface exists
if ! ip link show "$INTERFACE" > /dev/null 2>&1; then
    echo "Error: Interface $INTERFACE not found."
    exit 1
fi

echo "Configuring $INTERFACE as Access Point..."

# Remove existing connection if it exists
if nmcli connection show "$SSID" > /dev/null 2>&1; then
    echo "Removing existing connection '$SSID'..."
    nmcli connection delete "$SSID"
fi

# Create new Hotspot connection
echo "Creating new Hotspot connection..."
nmcli con add type wifi ifname "$INTERFACE" con-name "$SSID" autoconnect yes ssid "$SSID"
nmcli con modify "$SSID" 802-11-wireless.mode ap
nmcli con modify "$SSID" 802-11-wireless.band bg
nmcli con modify "$SSID" ipv4.method shared
nmcli con modify "$SSID" ipv4.addresses "$IP_ADDRESS"
nmcli con modify "$SSID" wifi-sec.key-mgmt wpa-psk
nmcli con modify "$SSID" wifi-sec.psk "$PASSWORD"

# Bring up the connection
echo "Activating connection..."
nmcli con up "$SSID"

echo "Network setup complete."
echo "SSID: $SSID"
echo "IP: $IP_ADDRESS"
