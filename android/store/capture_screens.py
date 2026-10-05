"""Captures Play Store phone screenshots from the running emulator at 1080x2160 (Play allows at most 2:1).

Real app screenshots: title, chase, pause, help popup, game over. The QTE moment is composed from the bot's dump of
the exact game frame (qte.raw) placed into a real app screenshot with the app's own scaling and brightness curve.
"""
import io
import os
import subprocess
import time

from PIL import Image

ADB = r"C:\Users\Admin\AppData\Local\Android\Sdk\platform-tools\adb.exe"
PKG = "com.dineshrichard.vaadivasal"
HERE = r"D:\Vaadivasal - NES\android\store"
OUT = os.path.join(HERE, "raw")
os.makedirs(OUT, exist_ok=True)
W, H = 1080, 2160
PIC_H = int(W * 240 / 256)           # 1012: height of the game picture in the app


def adb(*a, binary=False):
    r = subprocess.run([ADB, *a], capture_output=True)
    return r.stdout if binary else r.stdout.decode("utf-8", "replace")


def shot(name):
    img = Image.open(io.BytesIO(adb("exec-out", "screencap", "-p", binary=True))).convert("RGB")
    img.save(os.path.join(OUT, name + ".png"))
    return img


def tap(x, y, ms=120):
    adb("shell", "input", "swipe", str(x), str(y), str(x), str(y), str(ms))


def launch():
    adb("shell", "am", "force-stop", PKG)
    adb("shell", "am", "start", "-n", f"{PKG}/.MainActivity")
    time.sleep(5)


START = (756, 1230)          # Select/Start row hangs off the picture's bottom edge
DPAD_R = (356, 1663)
HELP = (154, 2060)

adb("shell", "wm", "size", f"{W}x{H}")
time.sleep(2)

launch()
shot("01_title")

tap(*START)
time.sleep(0.8)
h = subprocess.Popen([ADB, "shell", "input", "swipe", str(DPAD_R[0]), str(DPAD_R[1]), str(DPAD_R[0]), str(DPAD_R[1]), "1800"])
time.sleep(2.6)
shot("02_chase")
h.wait()

launch()
tap(*START)
time.sleep(2.5)
tap(*START)
time.sleep(0.8)
shot("04_pause")
tap(*START)
time.sleep(0.5)
tap(*HELP)
time.sleep(1.5)
shot("05_help")
adb("shell", "input", "keyevent", "KEYCODE_BACK")

launch()
tap(*START)
time.sleep(36)               # stand still until the bull has hit three times
shot("06_gameover")

adb("shell", "wm", "size", "reset")

# ---- QTE screenshot: bot frame -> app layout -------------------------------------------------------------
GAMMA = 0.70


def curve(v, mx):
    return int(round(mx * (v / mx) ** GAMMA))


raw = open(os.path.join(OUT, "qte.raw"), "rb").read()
frame = Image.new("RGB", (256, 240))
px = frame.load()
for i in range(256 * 240):
    v = raw[2 * i] | (raw[2 * i + 1] << 8)
    r5, g6, b5 = (v >> 11) & 31, (v >> 5) & 63, v & 31
    r5, g6, b5 = curve(r5, 31), curve(g6, 63), curve(b5, 31)
    px[i % 256, i // 256] = ((r5 << 3) | (r5 >> 2), (g6 << 2) | (g6 >> 4), (b5 << 3) | (b5 >> 2))
base = Image.open(os.path.join(OUT, "02_chase.png")).convert("RGB")
base.paste(frame.resize((W, PIC_H), Image.NEAREST), (0, 0))
base.save(os.path.join(OUT, "03_qte.png"))
print("done")
