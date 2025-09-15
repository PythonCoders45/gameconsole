# airplane_mode_bt.py
# Python Bluetooth manager integrated with Airplane Mode

import bluetooth
import threading
import time
import os

STATE_FILE = "/tmp/airplane_mode_state"  # Written by C program

discovered_devices = {}
connected_socks = {}

# ------------------------
# Scan for Bluetooth devices
# ------------------------
def scan_devices():
    while True:
        nearby_devices = bluetooth.discover_devices(duration=8, lookup_names=True)
        for addr, name in nearby_devices:
            discovered_devices[addr] = name
        time.sleep(10)

# ------------------------
# Connect to a device
# ------------------------
def connect_device(addr):
    try:
        sock = bluetooth.BluetoothSocket(bluetooth.RFCOMM)
        sock.connect((addr, 1))
        connected_socks[addr] = sock
        print(f"✅ Connected to {discovered_devices.get(addr,'Unknown')} ({addr})")
    except Exception as e:
        print(f"❌ Failed to connect to {addr}: {e}")

# ------------------------
# Disconnect all devices
# ------------------------
def disconnect_all_devices():
    for addr, sock in connected_socks.items():
        try:
            sock.close()
            print(f"🔌 Disconnected {discovered_devices.get(addr,'Unknown')} ({addr})")
        except:
            pass
    connected_socks.clear()

# ------------------------
# Watch Airplane Mode state
# ------------------------
def watch_airplane_mode():
    last_state = None
    while True:
        state = None
        if os.path.exists(STATE_FILE):
            with open(STATE_FILE, "r") as f:
                state = f.read().strip()
        if state != last_state:
            last_state = state
            if state == "ON":
                print("✈️ Airplane Mode ON - Disconnecting Bluetooth")
                disconnect_all_devices()
            elif state == "OFF":
                print("📶 Airplane Mode OFF - You can reconnect Bluetooth manually")
        time.sleep(2)

# ------------------------
# Main
# ------------------------
if __name__ == "__main__":
    # Start scanning in a separate thread
    scanner_thread = threading.Thread(target=scan_devices, daemon=True)
    scanner_thread.start()

    # Start watching Airplane Mode state
    watch_airplane_mode()
