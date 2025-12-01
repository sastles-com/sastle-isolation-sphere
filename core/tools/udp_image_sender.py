#!/usr/bin/env python3
"""
UDP JPEG Image Sender for Isolation Sphere

Usage:
    python udp_image_sender.py image.jpg
    python udp_image_sender.py --camera  # Use webcam
"""

import socket
import struct
import time
import sys
import argparse
from pathlib import Path

# ESP32設定
ESP32_IP = "192.168.49.101"
ESP32_PORT = 8889

# UDPパケットヘッダー
MAGIC = 0x4A504547  # "JPEG"

def send_jpeg_udp(jpeg_data: bytes, frame_id: int, dest_ip: str, dest_port: int):
    """
    JPEG画像をUDP経由でESP32に送信
    
    Args:
        jpeg_data: JPEGバイナリデータ
        frame_id: フレーム連番
        dest_ip: 送信先IPアドレス
        dest_port: 送信先ポート番号
    """
    # ヘッダー作成 (8 bytes: magic + frame_id)
    header = struct.pack('<II', MAGIC, frame_id)
    
    # パケット構築
    packet = header + jpeg_data
    
    # UDP送信
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.sendto(packet, (dest_ip, dest_port))
        print(f"[Frame {frame_id}] Sent {len(jpeg_data)} bytes JPEG (total: {len(packet)} bytes)")
    finally:
        sock.close()

def send_image_file(image_path: str, dest_ip: str = ESP32_IP, dest_port: int = ESP32_PORT):
    """
    JPEGファイルを1回送信
    
    Args:
        image_path: JPEGファイルパス
        dest_ip: 送信先IPアドレス
        dest_port: 送信先ポート番号
    """
    # JPEGファイル読み込み
    with open(image_path, 'rb') as f:
        jpeg_data = f.read()
    
    if len(jpeg_data) == 0:
        print(f"Error: Empty file {image_path}")
        return
    
    if len(jpeg_data) > 65507 - 8:  # UDP最大 - ヘッダー
        print(f"Warning: JPEG size {len(jpeg_data)} bytes may exceed UDP limit")
    
    # 送信
    send_jpeg_udp(jpeg_data, frame_id=1, dest_ip=dest_ip, dest_port=dest_port)

def send_camera_stream(dest_ip: str = ESP32_IP, dest_port: int = ESP32_PORT, 
                       width: int = 320, height: int = 160, fps: int = 30):
    """
    Webカメラからリアルタイム映像をストリーミング送信
    
    Args:
        dest_ip: 送信先IPアドレス
        dest_port: 送信先ポート番号
        width: 画像幅
        height: 画像高さ
        fps: 目標フレームレート
    """
    try:
        import cv2
    except ImportError:
        print("Error: OpenCV (cv2) is required for camera streaming")
        print("Install: pip install opencv-python")
        return
    
    # カメラ初期化
    cap = cv2.VideoCapture(0)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
    cap.set(cv2.CAP_PROP_FPS, fps)
    
    if not cap.isOpened():
        print("Error: Cannot open camera")
        return
    
    print(f"Streaming to {dest_ip}:{dest_port} at {width}x{height} @ {fps}fps")
    print("Press Ctrl+C to stop")
    
    frame_id = 0
    frame_interval = 1.0 / fps
    
    try:
        while True:
            start_time = time.time()
            
            # フレーム取得
            ret, frame = cap.read()
            if not ret:
                print("Error: Failed to capture frame")
                break
            
            # リサイズ
            frame = cv2.resize(frame, (width, height))
            
            # JPEG圧縮
            encode_param = [int(cv2.IMWRITE_JPEG_QUALITY), 80]
            result, encoded = cv2.imencode('.jpg', frame, encode_param)
            
            if not result:
                print("Error: JPEG encoding failed")
                continue
            
            jpeg_data = encoded.tobytes()
            
            # UDP送信
            send_jpeg_udp(jpeg_data, frame_id, dest_ip, dest_port)
            
            frame_id += 1
            
            # フレームレート調整
            elapsed = time.time() - start_time
            sleep_time = frame_interval - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)
            
    except KeyboardInterrupt:
        print(f"\nStopped. Total frames sent: {frame_id}")
    finally:
        cap.release()

def main():
    parser = argparse.ArgumentParser(description='UDP JPEG Image Sender for ESP32')
    parser.add_argument('image', nargs='?', help='JPEG image file to send')
    parser.add_argument('--camera', action='store_true', help='Stream from webcam')
    parser.add_argument('--ip', default=ESP32_IP, help=f'ESP32 IP address (default: {ESP32_IP})')
    parser.add_argument('--port', type=int, default=ESP32_PORT, help=f'ESP32 UDP port (default: {ESP32_PORT})')
    parser.add_argument('--width', type=int, default=320, help='Image width (default: 320)')
    parser.add_argument('--height', type=int, default=160, help='Image height (default: 160)')
    parser.add_argument('--fps', type=int, default=30, help='Target FPS for camera (default: 30)')
    
    args = parser.parse_args()
    
    if args.camera:
        # カメラストリーミングモード
        send_camera_stream(args.ip, args.port, args.width, args.height, args.fps)
    elif args.image:
        # 静止画送信モード
        if not Path(args.image).exists():
            print(f"Error: File not found: {args.image}")
            sys.exit(1)
        send_image_file(args.image, args.ip, args.port)
    else:
        parser.print_help()
        sys.exit(1)

if __name__ == '__main__':
    main()
