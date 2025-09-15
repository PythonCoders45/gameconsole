# os_screen_control.py
# Python module to control screen brightness for a custom OS

import platform
import subprocess

# ------------------------
# Variables
# ------------------------
os_type = platform.system()  # OS type (Windows/Linux/Custom)
current_brightness = 50       # Default brightness (0-100)
is_auto_brightness = False    # Auto-brightness state

# ------------------------
# Core Functions
# ------------------------
def set_brightness(level: int):
    """Set brightness (0-100)"""
    global current_brightness
    current_brightness = max(0, min(level, 100))
    if os_type == "Windows":
        subprocess.call(f"powershell (Get-WmiObject -Namespace root/WMI -Class WmiMonitorBrightnessMethods).WmiSetBrightness(1,{current_brightness})", shell=True)
    elif os_type == "Linux":
        # This assumes sysfs backlight control is available
        subprocess.call(f"echo {current_brightness} | sudo tee /sys/class/backlight/*/brightness", shell=True)
    # For custom OS: implement driver call here

def increase_brightness(step: int = 5):
    """Increase brightness by step"""
    set_brightness(current_brightness + step)

def decrease_brightness(step: int = 5):
    """Decrease brightness by step"""
    set_brightness(current_brightness - step)

def enable_auto_brightness():
    """Enable auto-brightness"""
    global is_auto_brightness
    is_auto_brightness = True
    # For custom OS: implement auto-brightness logic here

def disable_auto_brightness():
    """Disable auto-brightness"""
    global is_auto_brightness
    is_auto_brightness = False
    # For custom OS: stop auto-brightness logic here
