import socket
import time
import sys
import os
from PIL import Image
import io

# Configuration
ESP32_IP = "192.168.4.1" # Default IP for ESP32 AP mode
ESP32_PORT = 8889
IMAGE_PATH = "test_image.jpg"

def create_test_image():
    # Create a simple RGB image
    img = Image.new('RGB', (64, 64), color = 'red')
    img.save(IMAGE_PATH)
    print(f"Created test image: {IMAGE_PATH}")

def send_image(ip, port, image_path):
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        
        with open(image_path, "rb") as f:
            image_data = f.read()
        
        print(f"Sending {len(image_data)} bytes to {ip}:{port}...")
        sock.sendto(image_data, (ip, port))
        print("Sent.")
        
    except Exception as e:
        print(f"Error: {e}")
    finally:
        sock.close()

if __name__ == "__main__":
    if len(sys.argv) > 1:
        ESP32_IP = sys.argv[1]
    
    if not os.path.exists(IMAGE_PATH):
        create_test_image()
        
    while True:
        send_image(ESP32_IP, ESP32_PORT, IMAGE_PATH)
        time.sleep(1) # Send every 1 second
