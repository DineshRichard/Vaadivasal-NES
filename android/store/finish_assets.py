"""Picks the final phone screenshots and builds the 1024x500 feature graphic."""
import io
import os
import subprocess
import time

from PIL import Image, ImageDraw, ImageFont

ADB = r"C:\Users\Admin\AppData\Local\Android\Sdk\platform-tools\adb.exe"
PKG = "com.dineshrichard.vaadivasal"
HERE = r"D:\Vaadivasal - NES\android\store"
RAW = os.path.join(HERE, "raw")
OUT = os.path.join(HERE, "screenshots")
os.makedirs(OUT, exist_ok=True)
W, H = 1080, 2160
PIC_H = int(W * 240 / 256)


def adb(*a, binary=False):
    r = subprocess.run([ADB, *a], capture_output=True)
    return r.stdout if binary else r.stdout.decode("utf-8", "replace")


def shot():
    return Image.open(io.BytesIO(adb("exec-out", "screencap", "-p", binary=True))).convert("RGB")


# ---- title screenshot with the blinking "press START" text visible ----
adb("shell", "wm", "size", f"{W}x{H}")
time.sleep(2)
adb("shell", "am", "force-stop", PKG)
adb("shell", "am", "start", "-n", f"{PKG}/.MainActivity")
time.sleep(5)
best, best_score = None, -1
for _ in range(14):
    im = shot()
    box = im.crop((200, int(PIC_H * 0.77), 880, int(PIC_H * 0.87)))
    score = sum(1 for r, g, b in box.resize((68, 10)).getdata() if r > 170 and g > 140 and b > 140)
    if score > best_score:
        best, best_score = im, score
    time.sleep(0.25)
adb("shell", "wm", "size", "reset")
best.save(os.path.join(RAW, "01_title.png"))
print("title text pixels:", best_score)

order = ["01_title", "02_chase", "03_qte", "04_pause", "05_help", "06_gameover"]
for i, n in enumerate(order, 1):
    Image.open(os.path.join(RAW, n + ".png")).convert("RGB").save(os.path.join(OUT, f"phone_{i}.png"), optimize=True)

# ---- feature graphic 1024x500 ----
ORANGE = (246, 157, 74)
PANEL = (72, 35, 19)
BRICK = (197, 95, 37)
fg = Image.new("RGB", (1024, 500), PANEL)
d = ImageDraw.Draw(fg)
for y in range(0, 500, 8):                      # subtle dotted arena-sand texture
    for x in range((y // 8 % 2) * 4, 1024, 8):
        d.point((x, y), fill=(86, 44, 24))
d.rectangle((0, 0, 1023, 7), fill=ORANGE)
d.rectangle((0, 492, 1023, 499), fill=ORANGE)

# the game's own pixel-art title banner, taken from the title screenshot
title = Image.open(os.path.join(RAW, "01_title.png")).convert("RGB")
ban = title.crop((120, 80, 960, int(PIC_H * 0.30)))
# trim to the dark banner box
px = ban.load()
xs = [x for x in range(ban.width) for y in (ban.height // 2,) if sum(px[x, y]) < 150]
ys = [y for y in range(ban.height) for x in (ban.width // 2,) if sum(px[x, y]) < 150]
ban = ban.crop((min(xs), min(ys), max(xs) + 1, max(ys) + 1)) if xs and ys else ban
bw = 540
ban = ban.resize((bw, int(ban.height * bw / ban.width)), Image.NEAREST)
fg.paste(ban, (40, 70))

font_b = ImageFont.truetype(r"C:\Windows\Fonts\arialbd.ttf", 40)
font_s = ImageFont.truetype(r"C:\Windows\Fonts\arialbd.ttf", 27)
y = 70 + ban.height + 36
d.text((40, y), "Tame the Bull.", font=font_b, fill=ORANGE)
d.text((40, y + 50), "Honor the Tradition.", font=font_b, fill=(244, 228, 212))
d.text((40, y + 120), "The first Tamil 8-bit game", font=font_s, fill=BRICK if False else (244, 228, 212))
d.text((40, y + 158), "25 bulls  -  Jallikattu  -  Alanganallur", font=font_s, fill=ORANGE)

icon = Image.open(os.path.join(HERE, "play_icon_512.png")).convert("RGB").resize((420, 420), Image.LANCZOS)
d.rectangle((560 + 25, 40 + 0, 560 + 25 + 420 + 7, 40 + 420 + 7), fill=ORANGE)
fg.paste(icon, (560 + 28, 44))
# keep the graphic clear of the corners / safe area: play shows a play button in the centre only on video, so fine
fg.save(os.path.join(HERE, "feature_graphic_1024x500.png"), optimize=True)
print("feature graphic written", fg.size)
