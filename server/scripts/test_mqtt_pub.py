import paho.mqtt.client as mqtt
import json
import time
import sys

# Configuration
BROKER_ADDRESS = "localhost"
TOPIC = "isolation_sphere_esp32/led"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT Broker!")
    else:
        print(f"Failed to connect, return code {rc}")

def publish_test_message():
    client = mqtt.Client()
    client.on_connect = on_connect

    try:
        print(f"Connecting to {BROKER_ADDRESS}...")
        client.connect(BROKER_ADDRESS, 1883, 60)
        client.loop_start()
        time.sleep(1) # Wait for connection

        # Test 1: Solid Color (Red)
        # FastLED HSV: Hue 0 = Red, 96 = Green, 160 = Blue
        payload = {
            "type": "solid",
            "hue": 0,   
            "val": 255
        }
        print(f"Publishing to {TOPIC}: {payload}")
        client.publish(TOPIC, json.dumps(payload))
        
        time.sleep(2)

        # Test 2: Solid Color (Blue)
        payload = {
            "type": "solid",
            "hue": 160, 
            "val": 255
        }
        print(f"Publishing to {TOPIC}: {payload}")
        client.publish(TOPIC, json.dumps(payload))

        time.sleep(2)
        
        client.loop_stop()
        client.disconnect()
        print("Done.")

    except Exception as e:
        print(f"Error: {e}")
        print("Make sure paho-mqtt is installed: pip install paho-mqtt")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        BROKER_ADDRESS = sys.argv[1]
    publish_test_message()
