import time
import threading
import pyautogui
import mss
import cv2
import os
import numpy as np
from datetime import datetime
from gpiozero import Button
from tkinter import Tk, Frame, Label, Canvas, Scrollbar, PhotoImage, NW, Toplevel, Button as TkButton, simpledialog
from PIL import Image, ImageTk
from http.server import HTTPServer, SimpleHTTPRequestHandler
from moviepy.editor import VideoFileClip

# CONFIG
FPS = 60.0
BUTTON_PIN = 17
SAVE_DIR = "captures"
FLASH_DURATION = 0.2
HTTP_PORT = 8000

os.makedirs(SAVE_DIR, exist_ok=True)

def timestamp_name(ext):
    return datetime.now().strftime("%Y-%m-%d_%H-%M-%S") + ext

def show_flash():
    screen = pyautogui.screenshot()
    img = np.array(screen)
    white = np.ones_like(img) * 255
    overlay = cv2.addWeighted(img, 0.0, white, 1.0, 0)
    cv2.imshow("Capture Flash", overlay)
    cv2.waitKey(int(FLASH_DURATION*1000))
    cv2.destroyWindow("Capture Flash")

def safe_save_screenshot():
    temp_file = os.path.join(SAVE_DIR, "temp_screenshot.png")
    final_file = os.path.join(SAVE_DIR, timestamp_name(".png"))
    pyautogui.screenshot().save(temp_file)
    os.rename(temp_file, final_file)
    show_flash()
    print(f"[+] Screenshot saved: {final_file}")

def safe_save_recording(stop_event):
    temp_file = os.path.join(SAVE_DIR, "temp_recording.mp4")
    final_file = os.path.join(SAVE_DIR, timestamp_name(".mp4"))
    with mss.mss() as sct:
        monitor = sct.monitors[0]
        width, height = monitor["width"], monitor["height"]
        fourcc = cv2.VideoWriter_fourcc(*"mp4v")
        out = cv2.VideoWriter(temp_file, fourcc, FPS, (width, height))
        last_frame_time = time.time()
        while not stop_event.is_set():
            now = time.time()
            if now - last_frame_time >= 1/FPS:
                frame = cv2.cvtColor(np.array(sct.grab(monitor)), cv2.COLOR_BGRA2BGR)
                out.write(frame)
                last_frame_time = now
        out.release()
    os.rename(temp_file, final_file)
    print(f"[+] Recording saved: {final_file}")

def button_pressed():
    press_start = time.time()
    stop_event = threading.Event()
    recording_thread = None
    while button.is_pressed:
        if time.time() - press_start > 0.5 and recording_thread is None:
            print("[*] Starting recording... hold button to keep recording")
            recording_thread = threading.Thread(target=safe_save_recording, args=(stop_event,), daemon=True)
            recording_thread.start()
        time.sleep(0.01)
    hold_time = time.time() - press_start
    if hold_time < 0.5:
        safe_save_screenshot()
    else:
        stop_event.set()
        if recording_thread:
            recording_thread.join()
        show_flash()

# Video editing
def trim_video(path):
    start = simpledialog.askfloat("Trim Video", "Start time (seconds):", minvalue=0)
    end = simpledialog.askfloat("Trim Video", "End time (seconds):", minvalue=0)
    if start is None or end is None or start >= end:
        print("[!] Invalid times")
        return
    clip = VideoFileClip(path)
    new_clip = clip.subclip(start, end)
    temp_file = path + ".temp.mp4"
    new_clip.write_videofile(temp_file, codec="libx264")
    os.replace(temp_file, path)
    print(f"[+] Video trimmed: {path}")

def play_video(path):
    cap = cv2.VideoCapture(path)
    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            break
        cv2.imshow(f"Playing {os.path.basename(path)}", frame)
        if cv2.waitKey(int(1000/FPS)) & 0xFF == ord('q'):
            break
    cap.release()
    cv2.destroyWindow(f"Playing {os.path.basename(path)}")

# Album GUI with video thumbnails
def open_album():
    album_win = Toplevel()
    album_win.title("Capture Album")
    canvas = Canvas(album_win)
    scrollbar = Scrollbar(album_win, orient="vertical", command=canvas.yview)
    scrollable_frame = Frame(canvas)
    scrollable_frame.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))
    canvas.create_window((0,0), window=scrollable_frame, anchor=NW)
    canvas.configure(yscrollcommand=scrollbar.set)

    files = sorted(os.listdir(SAVE_DIR), reverse=True)
    images = []
    row, col = 0, 0
    for f in files:
        path = os.path.join(SAVE_DIR, f)
        if f.lower().endswith((".png",".jpg")):
            img = Image.open(path)
            img.thumbnail((200,200))
            photo = ImageTk.PhotoImage(img)
            images.append(photo)
            lbl = Label(scrollable_frame, image=photo)
            lbl.grid(row=row, column=col, padx=5, pady=5)
            col += 1
            if col > 3:
                col = 0
                row += 1
        elif f.lower().endswith(".mp4"):
            # Extract first frame for thumbnail
            cap = cv2.VideoCapture(path)
            ret, frame = cap.read()
            cap.release()
            if ret:
                frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                img = Image.fromarray(frame)
                img.thumbnail((200,200))
                photo = ImageTk.PhotoImage(img)
                images.append(photo)
                lbl = Label(scrollable_frame, image=photo, text=f, compound="top")
                lbl.grid(row=row, column=col, padx=5, pady=5)
                TkButton(scrollable_frame, text="Play", command=lambda p=path: threading.Thread(target=play_video, args=(p,), daemon=True).start()).grid(row=row+1, column=col)
                TkButton(scrollable_frame, text="Trim", command=lambda p=path: trim_video(p)).grid(row=row+2, column=col)
                col += 1
                if col > 3:
                    col = 0
                    row += 3

    canvas.pack(side="left", fill="both", expand=True)
    scrollbar.pack(side="right", fill="y")

# HTTP server for smartphone
def start_http_server():
    os.chdir(SAVE_DIR)
    server = HTTPServer(("0.0.0.0", HTTP_PORT), SimpleHTTPRequestHandler)
    print(f"[*] HTTP server running on port {HTTP_PORT}. Access from smartphone: http://<PC_IP>:{HTTP_PORT}/")
    server.serve_forever()

# Setup
button = Button(BUTTON_PIN)
button.when_pressed = button_pressed

print("[*] Custom button: quick press = screenshot, hold = recording until release")
print("[*] Press Enter to open album GUI")

def album_listener():
    while True:
        input()
        open_album()

threading.Thread(target=album_listener, daemon=True).start()
threading.Thread(target=start_http_server, daemon=True).start()

while True:
    time.sleep(1)
