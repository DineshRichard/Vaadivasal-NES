"""Generates the Android launcher icons and the Play Store icon from the supplied artwork."""
import os
from PIL import Image, ImageDraw

SRC = r"D:\Vaadivasal - NES\android\store\app_icon_source.png"
RES = r"D:\Vaadivasal - NES\android\app\src\main\res"
STORE = r"D:\Vaadivasal - NES\android\store"

art = Image.open(SRC).convert("RGB")

legacy = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}
adaptive = {"mdpi": 108, "hdpi": 162, "xhdpi": 216, "xxhdpi": 324, "xxxhdpi": 432}

for name in os.listdir(RES):
    if name.startswith("mipmap-") and name != "mipmap-anydpi":
        for f in os.listdir(os.path.join(RES, name)):
            os.remove(os.path.join(RES, name, f))      # drop the old placeholder icons


def circle(img):
    mask = Image.new("L", img.size, 0)
    ImageDraw.Draw(mask).ellipse((0, 0, img.width - 1, img.height - 1), fill=255)
    out = Image.new("RGBA", img.size, (0, 0, 0, 0))
    out.paste(img, (0, 0), mask)
    return out


for dens, px in legacy.items():
    d = os.path.join(RES, f"mipmap-{dens}")
    os.makedirs(d, exist_ok=True)
    sq = art.resize((px, px), Image.LANCZOS)
    sq.save(os.path.join(d, "ic_launcher.png"))
    circle(sq).save(os.path.join(d, "ic_launcher_round.png"))

# Adaptive icon: 108dp canvas, artwork sized to fit inside a circular mask (title not clipped)
for dens, px in adaptive.items():
    d = os.path.join(RES, f"mipmap-{dens}")
    canvas = Image.new("RGBA", (px, px), (0, 0, 0, 0))
    inner = int(px * 60 / 108)
    a = art.resize((inner, inner), Image.LANCZOS).convert("RGBA")
    canvas.paste(a, ((px - inner) // 2, (px - inner) // 2))
    canvas.save(os.path.join(d, "ic_launcher_foreground.png"))

any_dir = os.path.join(RES, "mipmap-anydpi-v26")
os.makedirs(any_dir, exist_ok=True)
for f in os.listdir(any_dir):
    os.remove(os.path.join(any_dir, f))
xml = """<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@color/ic_launcher_background" />
    <foreground android:drawable="@mipmap/ic_launcher_foreground" />
</adaptive-icon>
"""
for n in ("ic_launcher.xml", "ic_launcher_round.xml"):
    open(os.path.join(any_dir, n), "w", encoding="utf-8").write(xml)

# Play Store hi-res icon: 512x512 PNG (the supplied file, flattened to RGB)
os.makedirs(STORE, exist_ok=True)
art.resize((512, 512), Image.LANCZOS).save(os.path.join(STORE, "play_icon_512.png"), optimize=True)
print("icons written")
