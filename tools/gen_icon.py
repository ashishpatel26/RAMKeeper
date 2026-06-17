"""
Generate res/icon.ico — modern Win11-style RAMKeeper icon.
Run: uv run python tools/gen_icon.py
"""
import math
import os
from PIL import Image, ImageDraw

SIZES = [16, 24, 32, 48, 64, 128, 256]

BG_TOP = (72, 52, 212)
BG_BOT = (138, 43, 226)
FG = (255, 255, 255)
FG_DIM = (200, 190, 255)
GOLD = (255, 210, 60)


def draw_icon_256() -> Image.Image:
    """Draw the master 256×256 icon; smaller sizes are downscaled from this."""
    S = 256
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))

    # ── Rounded-rect background (gradient via scanline) ──────────────────────
    bg = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    bg_draw = ImageDraw.Draw(bg)
    for y in range(S):
        t = y / (S - 1)
        r = int(BG_TOP[0] + (BG_BOT[0] - BG_TOP[0]) * t)
        g = int(BG_TOP[1] + (BG_BOT[1] - BG_TOP[1]) * t)
        b = int(BG_TOP[2] + (BG_BOT[2] - BG_TOP[2]) * t)
        bg_draw.line([(0, y), (S - 1, y)], fill=(r, g, b, 255))

    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        [0, 0, S - 1, S - 1], radius=52, fill=255
    )
    bg.putalpha(mask)
    img.paste(bg, (0, 0), bg)

    draw = ImageDraw.Draw(img)

    # ── PCB / RAM stick ───────────────────────────────────────────────────────
    BAR_X0, BAR_X1 = 30, 226
    BAR_Y0, BAR_Y1 = 120, 156   # PCB bar
    BAR_R = 8

    draw.rounded_rectangle(
        [BAR_X0, BAR_Y0, BAR_X1, BAR_Y1], radius=BAR_R, fill=FG
    )

    # 4 memory chips on the bar
    chip_w, chip_h = 28, 22
    chip_y = BAR_Y0 + (BAR_Y1 - BAR_Y0) // 2 - chip_h // 2
    for cx in [52, 98, 144, 190]:
        draw.rounded_rectangle(
            [cx, chip_y, cx + chip_w, chip_y + chip_h],
            radius=4, fill=BG_TOP
        )

    # Gold contacts below bar
    n_contacts = 10
    cw = 8
    gap = (BAR_X1 - BAR_X0 - 20) / (n_contacts - 1)
    for i in range(n_contacts):
        cx = int(BAR_X0 + 10 + i * gap)
        draw.rectangle([cx, BAR_Y1, cx + cw, BAR_Y1 + 28], fill=GOLD)

    # Notch cut in bottom edge of bar
    notch_x = S // 2 - 8
    draw.rectangle([notch_x, BAR_Y1 - 6, notch_x + 16, BAR_Y1 + 2], fill=BG_BOT)

    # ── Speed arc above the RAM stick ─────────────────────────────────────────
    ARC_CX, ARC_CY = S // 2, BAR_Y0 - 10
    ARC_R = 52
    ARC_LW = 6

    draw.arc(
        [ARC_CX - ARC_R, ARC_CY - ARC_R,
         ARC_CX + ARC_R, ARC_CY + ARC_R],
        start=215, end=325,
        fill=FG_DIM, width=ARC_LW
    )

    # Arrowhead at end of arc
    angle = math.radians(325)
    ax = ARC_CX + ARC_R * math.cos(angle)
    ay = ARC_CY + ARC_R * math.sin(angle)
    draw.ellipse([ax - 5, ay - 5, ax + 5, ay + 5], fill=FG_DIM)

    # Two subtle horizontal speed lines to the left of arc
    for dy in [-6, 6]:
        draw.line(
            [(ARC_CX - ARC_R - 24, ARC_CY + dy),
             (ARC_CX - ARC_R - 8, ARC_CY + dy)],
            fill=FG_DIM, width=3
        )

    return img


def main():
    master = draw_icon_256()
    out = os.path.normpath(
        os.path.join(os.path.dirname(__file__), "..", "res", "icon.ico")
    )

    # Save: Pillow resizes master to each requested size inside the ICO
    master.save(
        out,
        format="ICO",
        sizes=[(sz, sz) for sz in SIZES]
    )
    kb = os.path.getsize(out) / 1024
    print(f"Saved {out}  ({kb:.1f} KB, sizes={SIZES})")

    # Sanity: re-open and verify sizes embedded
    with Image.open(out) as ico:
        print(f"Embedded sizes: {ico.info.get('sizes', 'unknown')}")


if __name__ == "__main__":
    main()
