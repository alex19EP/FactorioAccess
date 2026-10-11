"""Draws the audio ruler's icons into Graphics/: the item icon and the shortcut bar's two sizes.

The ruler is a cross with a box on its own tile and tick marks along its arms. The shortcut icons
follow the base game's colours: a light glyph for the bar (x56), a dark one for the popup (x24).

Run from the repo root: python helper-scripts/make-audio-ruler-icons.py
"""

from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parent.parent / "Graphics"

# Drawn at 4x and scaled down, for smooth edges.
SCALE = 4


def ruler(size, colour, outline=None):
    big = size * SCALE
    image = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    mid = big / 2
    arm = big * 0.06
    box = big * 0.16
    tick = big * 0.12
    ticks = [0.12, 0.27, 0.73, 0.88]

    def shapes(pad, fill):
        draw.rectangle([0 - pad, mid - arm - pad, big + pad, mid + arm + pad], fill=fill)
        draw.rectangle([mid - arm - pad, 0 - pad, mid + arm + pad, big + pad], fill=fill)
        draw.rectangle([mid - box - pad, mid - box - pad, mid + box + pad, mid + box + pad], fill=fill)
        for t in ticks:
            p = big * t
            draw.rectangle([p - arm * 0.6 - pad, mid - tick - pad, p + arm * 0.6 + pad, mid + tick + pad], fill=fill)
            draw.rectangle([mid - tick - pad, p - arm * 0.6 - pad, mid + tick + pad, p + arm * 0.6 + pad], fill=fill)

    if outline:
        shapes(big * 0.03, outline)
    shapes(0, colour)
    # The ruler's own tile is hollow, as it is drawn in the world.
    hole = box - arm * 1.2
    draw.rectangle([mid - hole, mid - hole, mid + hole, mid + hole], fill=(0, 0, 0, 0))
    return image.resize((size, size), Image.LANCZOS)


def main():
    ruler(64, (255, 200, 40, 255), (40, 30, 0, 255)).save(OUT / "audio-ruler.png")
    ruler(56, (226, 226, 226, 255)).save(OUT / "audio-ruler-x56.png")
    ruler(24, (29, 28, 29, 255)).save(OUT / "audio-ruler-x24.png")


if __name__ == "__main__":
    main()
