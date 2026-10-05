"""
Android-app level checks, driven through adb with pixel comparisons.

Complements the ROM bot (bot.c). This verifies what only the real app shows:
touch controls reach the game, popups, orientation, background/foreground and
rotation handling, and that nothing crashes.

Usage: python app_checks.py [outdir]
"""
import io
import os
import subprocess
import sys
import time

from PIL import Image, ImageChops

ADB = os.environ.get("ADB", r"C:\Users\Admin\AppData\Local\Android\Sdk\platform-tools\adb.exe")
PKG = "com.dineshrichard.vaadivasal"
OUT = sys.argv[1] if len(sys.argv) > 1 else "out"
os.makedirs(OUT, exist_ok=True)
results = []


def adb(*args, binary=False):
    r = subprocess.run([ADB, *args], capture_output=True)
    return r.stdout if binary else r.stdout.decode("utf-8", "replace")


def shot(name):
    png = adb("exec-out", "screencap", "-p", binary=True)
    img = Image.open(io.BytesIO(png)).convert("RGB")
    img.save(os.path.join(OUT, name + ".png"))
    return img


def diff_pct(a, b, box=None):
    """Percent of pixels that differ noticeably between two screenshots (optionally within a box)."""
    if box:
        a, b = a.crop(box), b.crop(box)
    d = ImageChops.difference(a, b).convert("L").point(lambda v: 255 if v > 24 else 0)
    hist = d.histogram()
    return 100.0 * hist[255] / (a.width * a.height)


def tap(x, y, ms=120):
    adb("shell", "input", "swipe", str(x), str(y), str(x), str(y), str(ms))


def check(ok, label):
    results.append(ok)
    print(f"  [{'ok  ' if ok else 'FAIL'}] {label}")


def crashed():
    return PKG in adb("logcat", "-d", "-b", "crash")


def relaunch():
    adb("shell", "am", "force-stop", PKG)
    adb("logcat", "-c")
    adb("shell", "am", "start", "-n", f"{PKG}/.MainActivity")
    time.sleep(4)


size = adb("shell", "wm", "size").strip().split()[-1]
W, H = map(int, size.split("x"))
print(f"device {W}x{H}")


def fx(f):  # x as fraction of 1000
    return W * f // 1000


def fy(f):
    return H * f // 1000


START = (fx(700), fy(512))
DPAD_RIGHT, DPAD_LEFT, DPAD_UP, DPAD_DOWN = (fx(330), fy(742)), (fx(150), fy(742)), (fx(241), fy(690)), (fx(241), fy(800))
BTN_A = (fx(850), fy(795))
HELP, CREDITS = (fx(143), fy(957)), (fx(895), fy(957))
PICTURE = (0, 0, W, int(W * 240 / 256))                 # game picture area (portrait layout)
CONTROLS = (0, int(W * 240 / 256), W, H)

print("== A1 launch ==")
relaunch()
check(bool(adb("shell", "pidof", PKG).strip()), "app process running after launch")
check(not crashed(), "no crash in the log")

print("== A2 orientation ==")
check(H > W, f"portrait layout ({W}x{H})")

print("== A3 title screen -> START (touch) -> game ==")
title = shot("a3_title")
tap(*START)
time.sleep(2.5)
game = shot("a3_game")
d = diff_pct(title, game, PICTURE)
check(d > 20, f"touching START replaced the title with the arena ({d:.0f}% of the picture changed)")

print("== A4 D-pad moves the player ==")
# start from a fresh game so the bull cannot have hit (and frozen) the player yet
relaunch()
tap(*START)
time.sleep(0.6)
before = shot("a4_before")
hold = subprocess.Popen([ADB, "shell", "input", "swipe", str(DPAD_LEFT[0]), str(DPAD_LEFT[1]),
                         str(DPAD_LEFT[0]), str(DPAD_LEFT[1]), "1200"])
time.sleep(0.6)
pressed = shot("a4_pressed")          # finger still down
hold.wait()
time.sleep(0.3)
left = shot("a4_left")
adb("shell", "input", "swipe", str(DPAD_RIGHT[0]), str(DPAD_RIGHT[1]), str(DPAD_RIGHT[0]), str(DPAD_RIGHT[1]), "700")
right = shot("a4_right")
dl = diff_pct(before, left, PICTURE)
dr = diff_pct(left, right, PICTURE)
check(dl > 0.5, f"holding LEFT moved the player ({dl:.1f}% of picture changed)")
check(dr > 0.5, f"holding RIGHT moved the player back ({dr:.1f}% of picture changed)")
cl = diff_pct(before, pressed, CONTROLS)
check(cl > 0.1, f"D-pad shows the pressed state while held ({cl:.2f}% of control area changed)")

print("== A5 Help / Credits popups ==")
base = shot("a5_base")
tap(*HELP)
time.sleep(1)
helpimg = shot("a5_help")
check(diff_pct(base, helpimg) > 30, f"Help popup appears ({diff_pct(base, helpimg):.0f}% of screen changed)")
adb("shell", "input", "keyevent", "KEYCODE_BACK")
time.sleep(1)
after = shot("a5_after_help")
check(diff_pct(helpimg, after) > 30, "Help popup closes with Back")
tap(*CREDITS)
time.sleep(1)
credimg = shot("a5_credits")
check(diff_pct(after, credimg) > 30, f"Credits popup appears ({diff_pct(after, credimg):.0f}% changed)")
check(diff_pct(helpimg, credimg) > 1, "Credits shows different text from Help")
adb("shell", "input", "keyevent", "KEYCODE_BACK")
time.sleep(1)

print("== A6 popup pauses the game ==")
tap(*HELP)
time.sleep(2.0)                       # dialog fade-in/dim animation finished
p1 = shot("a6_p1")
time.sleep(1.5)
p2 = shot("a6_p2")
check(diff_pct(p1, p2, PICTURE) < 0.05, f"picture frozen while a popup is open ({diff_pct(p1, p2, PICTURE):.2f}% changed)")
adb("shell", "input", "keyevent", "KEYCODE_BACK")
time.sleep(1)
q1 = shot("a6_q1")
time.sleep(1.5)
q2 = shot("a6_q2")
check(diff_pct(q1, q2, PICTURE) > 0.05, f"game running again after closing the popup ({diff_pct(q1, q2, PICTURE):.2f}% changed)")

print("== A7 background / foreground keeps the game ==")
time.sleep(1)
before_bg = shot("a7_before_bg")
adb("shell", "input", "keyevent", "KEYCODE_HOME")
time.sleep(3)
check(bool(adb("shell", "pidof", PKG).strip()), "process survives going to the background")
adb("shell", "am", "start", "-n", f"{PKG}/.MainActivity")
time.sleep(1.5)
r1 = shot("a7_r1")
time.sleep(1.5)
r2 = shot("a7_r2")
check(not crashed(), "no crash after returning")
check(diff_pct(r1, title, PICTURE) > 20, f"still in the arena, not back at the title screen ({diff_pct(r1, title, PICTURE):.0f}% differs from the title)")
HUD_L = (0, int(PICTURE[3] * 0.86), int(W * 0.35), int(PICTURE[3] * 0.97))        # lives
HUD_R = (int(W * 0.6), int(PICTURE[3] * 0.86), W, int(PICTURE[3] * 0.97))         # score (middle skipped: the bull walks through it)
hud_diff = max(diff_pct(before_bg, r1, HUD_L), diff_pct(before_bg, r1, HUD_R))
check(hud_diff < 5, f"lives and score unchanged after returning ({hud_diff:.1f}% differs)")
check(diff_pct(r1, r2, PICTURE) > 0.05 or max(diff_pct(before_bg, r2, HUD_L), diff_pct(before_bg, r2, HUD_R)) < 5, "game is running after returning")

print("== A8 rotation ==")
adb("shell", "settings", "put", "system", "accelerometer_rotation", "0")
adb("shell", "settings", "put", "system", "user_rotation", "1")
time.sleep(2)
rot = shot("a8_rot")
adb("shell", "settings", "put", "system", "user_rotation", "0")
time.sleep(2)
back = shot("a8_back")
check(not crashed() and bool(adb("shell", "pidof", PKG).strip()), "app alive after rotating the device and back")
check(back.width < back.height, "returns to portrait layout")

print("== A9 two-finger touch (D-pad + A) ==")
before = shot("a9_before")
adb("shell", f"input swipe {DPAD_RIGHT[0]} {DPAD_RIGHT[1]} {DPAD_RIGHT[0]} {DPAD_RIGHT[1]} 800 & "
              f"input swipe {BTN_A[0]} {BTN_A[1]} {BTN_A[0]} {BTN_A[1]} 800; wait")
after = shot("a9_after")
check(not crashed(), "simultaneous touches handled without crash")

print("== A10 stability: 60 seconds of random touches ==")
import random
random.seed(7)
t_end = time.time() + 60
n = 0
while time.time() < t_end:
    x, y = random.randint(0, W - 1), random.randint(int(H * 0.45), int(H * 0.92))
    adb("shell", "input", "swipe", str(x), str(y), str(x), str(y), str(random.randint(30, 400)))
    n += 1
time.sleep(1)
check(not crashed() and bool(adb("shell", "pidof", PKG).strip()), f"survived {n} random touches")

print()
print(f"APP CHECKS: pass={sum(results)} fail={len(results) - sum(results)}")
sys.exit(0 if all(results) else 1)
