
import os
import math
import queue
import threading

import requests
import paho.mqtt.client as mqtt

# ---------------------------------------------
# MQTT Configuration
# ---------------------------------------------

BROKER = "broker.mqtt-dashboard.com"
PORT = 1883

TOPIC = "ee470/emanuel/pot"
NODE_NAME = "emanuel470"

# ---------------------------------------------
# Hostinger Configuration
# ---------------------------------------------

API_URL = os.environ.get("MQTT_INSERT_URL", "")
API_KEY = os.environ.get("MQTT_API_KEY", "")

if not API_URL.startswith("https://"):
    raise SystemExit(
        "Set MQTT_INSERT_URL to your HTTPS endpoint."
    )

if not API_KEY:
    raise SystemExit("Set a valid MQTT_API_KEY.")

# ---------------------------------------------
# HTTP Session and Queue
# ---------------------------------------------

session = requests.Session()

session.headers.update({
    "X-API-Key": API_KEY,
    "Content-Type": "application/json"
})

measurements = queue.Queue(maxsize=100)

# ---------------------------------------------
# Forward Measurements to Hostinger
# ---------------------------------------------

def forward_measurements():

    while True:

        voltage = measurements.get()

        try:
            data = {
                "node_name": NODE_NAME,
                "voltage": voltage
            }

            response = session.post(
                API_URL,
                json=data,
                timeout=10
            )

            response.raise_for_status()

            result = response.json()

            if result.get("status") != "success":
                raise ValueError(
                    "Unexpected server response"
                )

            print(
                "[DATABASE] Stored:",
                voltage,
                "V | ID:",
                result.get("id")
            )

        except (requests.RequestException, ValueError) as e:

            print("[DATABASE] Insert failed:", e)
            print("[DATABASE] Measurement not stored.")

        finally:
            measurements.task_done()

# ---------------------------------------------
# MQTT Connection Callback
# ---------------------------------------------

def on_connect(
    client,
    userdata,
    flags,
    reason_code,
    properties
):

    if reason_code == 0:

        print("[MQTT] Connected to HiveMQ")

        client.subscribe(TOPIC, qos=0)

        print("[MQTT] Subscribed to:", TOPIC)

    else:
        print("[MQTT] Connection failed:", reason_code)

# ---------------------------------------------
# MQTT Message Callback
# ---------------------------------------------

def on_message(client, userdata, msg):

    try:
        voltage = float(
            msg.payload.decode("utf-8").strip()
        )

        if not math.isfinite(voltage):
            raise ValueError("Non-finite voltage")

        if not 0 <= voltage <= 3.3:
            raise ValueError("Voltage out of range")

        print()
        print("----- NEW MEASUREMENT -----")
        print("[MQTT] Topic:", msg.topic)
        print("[SENSOR] Voltage:", voltage, "V")

        measurements.put_nowait(voltage)

        print("[RELAY] Measurement queued.")
        print("---------------------------")

    except (ValueError, UnicodeDecodeError):
        print("[ERROR] Invalid potentiometer reading")

    except queue.Full:
        print("[ERROR] Relay queue full. Reading dropped.")

# ---------------------------------------------
# Start HTTP Worker
# ---------------------------------------------

worker = threading.Thread(
    target=forward_measurements,
    daemon=True
)

worker.start()

# ---------------------------------------------
# Start MQTT Client
# ---------------------------------------------

client = mqtt.Client(
    mqtt.CallbackAPIVersion.VERSION2
)

client.on_connect = on_connect
client.on_message = on_message

print("[SYSTEM] Starting MQTT relay...")

client.connect(BROKER, PORT, 60)

try:
    client.loop_forever()

except KeyboardInterrupt:
    print("\n[SYSTEM] Relay stopped.")

finally:
    client.disconnect()
    session.close()

