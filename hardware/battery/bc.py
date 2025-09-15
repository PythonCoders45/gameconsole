# battery_client.py
import socket

HOST = "192.168.1.100"  # replace with server IP
PORT = 7070

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((HOST, PORT))
    print("[CONNECTED] Listening for battery updates...")
    while True:
        data = s.recv(1024)
        if not data:
            break
        print("Update:", data.decode().strip())
