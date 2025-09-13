# mutual_device_info.py
import socket
import threading
import json
import platform
import psutil
import time

BROADCAST_PORT = 50003
CONNECT_PORT = 50004
BROADCAST_INTERVAL = 5

known_devices = set()
approved_devices = set()  # Devices the user allowed
device_info = {}

# --- SYSTEM INFO FUNCTION ---
def get_device_info():
    """Collect real system info"""
    return {
        "hostname": platform.node(),
        "os": platform.system(),
        "os_version": platform.version(),
        "cpu_cores": psutil.cpu_count(),
        "memory_total": psutil.virtual_memory().total,
        "storage_total": psutil.disk_usage('/').total
    }

# --- BROADCAST ---
def broadcast_presence():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    message = b"TRUSTED_OS_DEVICE"
    while True:
        sock.sendto(message, ('<broadcast>', BROADCAST_PORT))
        time.sleep(BROADCAST_INTERVAL)

def listen_for_broadcasts():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('', BROADCAST_PORT))
    while True:
        data, addr = sock.recvfrom(1024)
        ip = addr[0]
        if data == b"TRUSTED_OS_DEVICE" and ip not in known_devices:
            known_devices.add(ip)
            # Prompt user for permission
            approve = input(f"Device {ip} wants to collect your info. Approve? (y/n): ").lower()
            if approve == "y":
                approved_devices.add(ip)
                print(f"Approved {ip} for info sharing.")
            else:
                print(f"Denied {ip}.")

# --- TCP SERVER ---
def server():
    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.bind(('', CONNECT_PORT))
    server_sock.listen(5)
    while True:
        conn, addr = server_sock.accept()
        data = conn.recv(4096)
        if data:
            info = json.loads(data.decode())
            print(f"Received info from {addr[0]}: {info}")
        conn.close()

# --- CLIENT SENDING ---
def send_info():
    while True:
        time.sleep(10)
        for ip in list(approved_devices):
            try:
                client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                client.connect((ip, CONNECT_PORT))
                client.sendall(json.dumps(get_device_info()).encode())
                client.close()
            except Exception as e:
                print(f"Failed to send info to {ip}: {e}")

# --- MAIN ---
if __name__ == "__main__":
    threading.Thread(target=broadcast_presence, daemon=True).start()
    threading.Thread(target=listen_for_broadcasts, daemon=True).start()
    threading.Thread(target=server, daemon=True).start()
    threading.Thread(target=send_info, daemon=True).start()

    print("Mutual device info collector running...")
    while True:
        time.sleep(10)
