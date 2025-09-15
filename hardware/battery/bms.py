# battery.py
import psutil
import time

def get_battery_status():
    """Returns battery info as a dictionary"""
    battery = psutil.sensors_battery()
    if battery is None:
        return {
            "available": False,
            "percent": None,
            "plugged": None,
            "time_left_sec": None
        }
    return {
        "available": True,
        "percent": battery.percent,
        "plugged": battery.power_plugged,
        "time_left_sec": battery.secsleft
    }

if __name__ == "__main__":
    while True:
        status = get_battery_status()
        if status["available"]:
            plugged = "Charging" if status["plugged"] else "On Battery"
            print(f"Battery: {status['percent']}% | {plugged} | Time left: {status['time_left_sec']} sec")
        else:
            print("No battery detected.")
        time.sleep(5)
