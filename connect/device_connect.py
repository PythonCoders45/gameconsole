# device_connect.py
import socket
import threading
import time

# --- CONFIGURATION ---
BROADCAST_PORT = 50000   # Port used to broadcast presence
CONNECT_PORT = 50001     # Port for direct connections
BROADCAST_INTERVAL = 5   # Seconds between broadcast messages

# --- GLOBAL STATE ---
known_devices = set()  # Stores (ip, port) tuples

# --- BROADCAST FUNCTION ---
def broadcast_presence():
    """Broadcast this device's presence to the local network."""
    broadcaster = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    broadcaster.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    message = b"MY_OS_DEVICE"
    while True:
        broadcaster.sendto(message, ('<broadcast>', BROADCAST_PORT))
        time.sleep(BROADCAST_INTERVAL)

# --- LISTENER FUNCTION ---
def listen_for_broadcasts():
    """Listen for other devices broadcasting their presence."""
    listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    listener.bind(('', BROADCAST_PORT))
    while True:
        data, addr = listener.recvfrom(1024)
        if data == b"MY_OS_DEVICE":
            known_devices.add(addr[0])

# --- SERVER FUNCTION ---
def accept_connections():
    """Accept incoming TCP connections from other devices."""
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind(('', CONNECT_PORT))
    server.listen(5)
    print(f"Listening for connections on port {CONNECT_PORT}...")
    while True:
        conn, addr = server.accept()
        print(f"Connected by {addr}")
        threading.Thread(target=handle_client, args=(conn, addr), daemon=True).start()

def handle_client(conn, addr):
    """Handle a single connected client."""
    try:
        while True:
            data = conn.recv(1024)
            if not data:
                break
            print(f"[{addr}] {data.decode()}")
    except Exception as e:
        print(f"Connection error with {addr}: {e}")
    finally:
        conn.close()

# --- CLIENT FUNCTION ---
def connect_to_device(ip):
    """Connect to a device via TCP."""
    try:
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.connect((ip, CONNECT_PORT))
        client.sendall(b"Hello from another OS device!")
        client.close()
        print(f"Connected to {ip}")
    except Exception as e:
        print(f"Failed to connect to {ip}: {e}")

# --- MAIN ---
if __name__ == "__main__":
    # Start broadcast and listener threads
    threading.Thread(target=broadcast_presence, daemon=True).start()
    threading.Thread(target=listen_for_broadcasts, daemon=True).start()
    threading.Thread(target=accept_connections, daemon=True).start()

    print("Device connection system running...")
    print("Known devices:", known_devices)

    # Simple loop to connect to devices discovered
    while True:
        time.sleep(10)
        for ip in list(known_devices):
            connect_to_device(ip)
