# sound_control.py
# Python program to control system speaker volume

import platform
import subprocess
import threading
import time

# ------------------------
# Helper functions for different OS
# ------------------------
def set_volume_windows(level):
    """Set volume on Windows (0-100)"""
    import ctypes
    devices = ctypes.windll.winmm
    # Using simple command, for real OS adapt to driver
    subprocess.call(f"nircmd setsysvolume {int(level*655.35)}", shell=True)

def set_volume_linux(level):
    """Set volume on Linux (0-100)"""
    subprocess.call(f"amixer -D pulse sset Master {level}%", shell=True)

def set_volume(level):
    """Set volume depending on OS"""
    os_name = platform.system()
    if os_name == "Windows":
        set_volume_windows(level)
    elif os_name == "Linux":
        set_volume_linux(level)
    else:
        print("Unsupported OS")

# ------------------------
# Increase / Decrease volume
# ------------------------
def increase_volume(step=5):
    os_name = platform.system()
    if os_name == "Windows":
        subprocess.call("nircmd changesysvolume 3276", shell=True)  # ~5%
    elif os_name == "Linux":
        subprocess.call(f"amixer -D pulse sset Master {step}%+", shell=True)

def decrease_volume(step=5):
    os_name = platform.system()
    if os_name == "Windows":
        subprocess.call("nircmd changesysvolume -3276", shell=True)  # ~5%
    elif os_name == "Linux":
        subprocess.call(f"amixer -D pulse sset Master {step}%-", shell=True)

def mute():
    os_name = platform.system()
    if os_name == "Windows":
        subprocess.call("nircmd mutesysvolume 1", shell=True)
    elif os_name == "Linux":
        subprocess.call("amixer -D pulse sset Master mute", shell=True)

def unmute():
    os_name = platform.system()
    if os_name == "Windows":
        subprocess.call("nircmd mutesysvolume 0", shell=True)
    elif os_name == "Linux":
        subprocess.call("amixer -D pulse sset Master unmute", shell=True)

# ------------------------
# CLI Example
# ------------------------
if __name__ == "__main__":
    print("Sound Control CLI")
    print("Commands: up, down, mute, unmute, exit")
    while True:
        cmd = input("Command: ").strip().lower()
        if cmd == "up":
            increase_volume()
        elif cmd == "down":
            decrease_volume()
        elif cmd == "mute":
            mute()
        elif cmd == "unmute":
            unmute()
        elif cmd == "exit":
            break
        else:
            print("Unknown command")
