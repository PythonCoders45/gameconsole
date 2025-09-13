# multi_controller_sync.py
import socket
import threading
import time
import json
import os
import glob

BROADCAST_PORT = 50000
CONNECT_PORT = 50001
BROADCAST_INTERVAL = 5
SYNC_INTERVAL = 0.05  # 20 FPS

known_devices = set()
controller_states = {}  # {"controller_id": {button states, axes}}

# --- DEVICE DISCOVERY ---
def broadcast_presence():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    msg = b"MY_OS_DEVICE"
    while True:
        sock.sendto(msg, ('<broadcast>', BROADCAST_PORT))
        time.sleep(BROADCAST_INTERVAL)

def listen_for_broadcasts():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('', BROADCAST_PORT))
    while True:
        data, addr = sock.recvfrom(1024)
        if data == b"MY_OS_DEVICE":
            known_devices.add(addr[0])

# --- TCP SERVER ---
def accept_connections():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind(('', CONNECT_PORT))
    server.listen(5)
    while True:
        conn, addr = server.accept()
        threading.Thread(target=handle_client, args=(conn, addr), daemon=True).start()

def handle_client(conn, addr):
    try:
        while True:
            data = conn.recv(4096)
            if not data:
                break
            remote_state = json.loads(data.decode())
            # Merge remote controller states
            print(f"[{addr}] Remote controller: {remote_state}")
    except:
        pass
    finally:
        conn.close()

# --- READ CONTROLLERS ---
def detect_controllers():
    """
    Detect all connected controllers (Joy-Cons, USB, etc.) via device files.
    Example: /dev/controller_*
    """
    return glob.glob("/dev/controller_*")  # Each controller exposed as a file

def read_controller(file_path):
    """
    Read a controller file and parse JSON for state.
    Expected format: {"A": bool, "B": bool, "JOY_X": int, ...}
    """
    try:
        with open(file_path, "r") as f:
            state = json.load(f)
            controller_states[os.path.basename(file_path)] = state
    except:
        pass

# --- SYNC CONTROLLERS ---
def sync_controllers():
    while True:
        for ctrl_file in detect_controllers():
            read_controller(ctrl_file)

        # Broadcast each controller state to all devices
        for ip in list(known_devices):
            try:
                client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                client.connect((ip, CONNECT_PORT))
                client.sendall(json.dumps(controller_states).encode())
                client.close()
            except:
                pass
        time.sleep(SYNC_INTERVAL)

# --- MAIN ---
if __name__ == "__main__":
    threading.Thread(target=broadcast_presence, daemon=True).start()
    threading.Thread(target=listen_for_broadcasts, daemon=True).start()
    threading.Thread(target=accept_connections, daemon=True).start()
    threading.Thread(target=sync_controllers, daemon=True).start()

    print("Multi-controller sync running...")
    while True:
        time.sleep(10)
