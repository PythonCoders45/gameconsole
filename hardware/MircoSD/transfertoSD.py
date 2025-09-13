import os
import shutil
import sys

# Configurable paths
SOURCE_FILE = "storage.c"

# Possible SD card mount points (Linux, macOS, Windows)
SD_PATHS = [
    "/mnt/sdcard",     # Linux typical
    "/media/sdcard",   # Linux alternative
    "/Volumes/SDCARD", # macOS
]

# Windows drives D:..Z: (check for removable drives)
SD_PATHS.extend([f"{d}:/" for d in "DEFGHIJKLMNOPQRSTUVWXYZ"])

def find_sd_mount():
    """Check for an existing SD card path."""
    for path in SD_PATHS:
        if os.path.exists(path):
            return path
    return None

def transfer_to_sd():
    """Transfer storage.c to the SD card if detected."""
    if not os.path.exists(SOURCE_FILE):
        sys.exit("Error: storage.c not found!")

    sd_mount = find_sd_mount()
    if not sd_mount:
        sys.exit("Error: No microSD card detected.")

    dest = os.path.join(sd_mount, os.path.basename(SOURCE_FILE))
    shutil.copy2(SOURCE_FILE, dest)
    print(f"Transferred {SOURCE_FILE} → {dest}")

if __name__ == "__main__":
    transfer_to_sd()
