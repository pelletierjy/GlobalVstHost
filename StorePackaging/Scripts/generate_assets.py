#!/usr/bin/env python3
"""Generate MSIX logo assets for Global VST Host with a 'GVH' neon design — v3."""

import os
from PIL import Image, ImageDraw, ImageFont, ImageFilter

# App palette from custom_look_and_feel.h
BG_DEEP = (0x0A, 0x0E, 0x1A)
BG_PANEL = (0x12, 0x18, 0x2B)
ACCENT_CYAN = (0x00, 0xE5, 0xFF)
ACCENT_BLUE = (0x29, 0x79, 0xFF)
TEXT_PRIMARY = (0xE0, 0xE6, 0xF1)

ASSETS_DIR = r"D:\repos\others\GlobalVSTHost\StorePackaging\Assets"


def find_font(size, bold=True):
    """Try to load a clean system font."""
    names = []
    if bold:
        names = [
            "segoeuib.ttf",
            "arialbd.ttf",
            "Arial Bold.ttf",
            "calibrib.ttf",
            "verdanab.ttf",
            "framd.ttf",
            "Consola Bold.ttf",
        ]
    else:
        names = [
            "segoeui.ttf",
            "arial.ttf",
            "calibri.ttf",
            "verdana.ttf",
        ]
    for name in names:
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def make_glow_layer(draw_func, size, blur_radius=4):
    """Create a glow by drawing onto a larger canvas, blurring, then cropping."""
    margin = blur_radius * 3
    big = Image.new("RGBA", (size + margin * 2, size + margin * 2), (0, 0, 0, 0))
    big_draw = ImageDraw.Draw(big)
    draw_func(big_draw, big.size[0], big.size[1], margin)
    big = big.filter(ImageFilter.GaussianBlur(radius=blur_radius))
    return big.crop((margin, margin, margin + size, margin + size))


def draw_eq_bars(draw, cx, cy, bar_w, bar_h_max, gap, fill):
    """Draw three vertical EQ bars centered at (cx, cy)."""
    heights = [bar_h_max * 0.55, bar_h_max * 0.85, bar_h_max * 0.65]
    total_w = len(heights) * bar_w + (len(heights) - 1) * gap
    x0 = cx - total_w // 2
    for i, h in enumerate(heights):
        x = x0 + i * (bar_w + gap)
        y = cy - int(h) // 2
        draw.rounded_rectangle([x, y, x + bar_w, y + int(h)], radius=max(1, bar_w // 4), fill=fill)


def make_icon(size):
    """Create a single icon at the given square size."""
    is_small = size <= 50
    is_medium = 50 < size <= 150

    # Choose text: full "GVH" for large, "GV" for medium, eq bars for small
    if is_small:
        text = None
    elif is_medium:
        text = "GV"
    else:
        text = "GVH"

    # Base canvas — fill with the deep background so corners are never transparent
    img = Image.new("RGBA", (size, size), BG_DEEP)
    draw = ImageDraw.Draw(img)

    s = size / 310.0
    radius = int(32 * s)
    pad = int(8 * s)

    # Background rounded rect
    bg_rect = (pad, pad, size - pad, size - pad)
    draw.rounded_rectangle(bg_rect, radius=radius, fill=BG_DEEP)

    # Inner panel
    inner_pad = int(16 * s)
    inner_rect = (
        pad + inner_pad,
        pad + inner_pad,
        size - pad - inner_pad,
        size - pad - inner_pad,
    )
    draw.rounded_rectangle(inner_rect, radius=max(0, radius - inner_pad), fill=BG_PANEL)

    # ---- Cyan glow via Gaussian blur on separate layer ----
    # Boosted blur for medium+ so the glow actually reads
    blur_r = max(3, int(10 * s)) if not is_small else max(2, int(5 * s))

    def glow_draw(d, w, h, m):
        pr = pad + m
        glow_rect = (pr, pr, w - pr, h - pr)
        d.rounded_rectangle(glow_rect, radius=radius, outline=ACCENT_CYAN, width=max(2, int(5 * s)))

    glow = make_glow_layer(glow_draw, size, blur_radius=blur_r)
    img = Image.alpha_composite(img, glow)
    draw = ImageDraw.Draw(img)

    # Sharp accent border + inner stroke on small sizes to prevent taskbar bleed
    border_width = max(1, int(3 * s))
    draw.rounded_rectangle(bg_rect, radius=radius, outline=ACCENT_BLUE + (220,), width=border_width)
    if is_small:
        inner_border = (
            pad + border_width,
            pad + border_width,
            size - pad - border_width,
            size - pad - border_width,
        )
        draw.rounded_rectangle(
            inner_border, radius=max(0, radius - border_width), outline=ACCENT_BLUE + (80,), width=1
        )

    # ---- Text or EQ bars ----
    if text:
        font_ratio = 0.50 if is_small else (0.32 if is_medium else 0.38)
        font_size = int(size * font_ratio)
        font = find_font(font_size)

        bbox = draw.textbbox((0, 0), text, font=font)
        text_w = bbox[2] - bbox[0]
        text_h = bbox[3] - bbox[1]
        tx = (size - text_w) // 2
        ty = (size - text_h) // 2 - int(2 * s)

        # Text glow layer (cyan blur behind text)
        def text_glow_draw(d, w, h, m):
            d.text((tx + m, ty + m), text, font=font, fill=ACCENT_CYAN)

        text_glow = make_glow_layer(text_glow_draw, size, blur_radius=max(2, int(6 * s)))
        img = Image.alpha_composite(img, text_glow)
        draw = ImageDraw.Draw(img)

        # Main text
        draw.text((tx, ty), text, font=font, fill=TEXT_PRIMARY)
    else:
        # Small size: draw three EQ bars instead of a single letter
        bar_w = max(2, int(size * 0.14))
        bar_h_max = int(size * 0.42)
        gap = max(1, int(size * 0.06))
        cx = size // 2
        cy = size // 2

        # Glow behind bars
        def bars_glow_draw(d, w, h, m):
            draw_eq_bars(d, cx + m, cy + m, bar_w, bar_h_max, gap, ACCENT_CYAN)

        bars_glow = make_glow_layer(bars_glow_draw, size, blur_radius=max(2, int(4 * s)))
        img = Image.alpha_composite(img, bars_glow)
        draw = ImageDraw.Draw(img)

        # Sharp bars
        draw_eq_bars(draw, cx, cy, bar_w, bar_h_max, gap, TEXT_PRIMARY)

    # ---- Gradient bar under text (medium+ only) ----
    if is_medium or size >= 150:
        bar_w = int(size * 0.42)
        bar_h = max(2, int(size * 0.022))
        if text:
            bbox = draw.textbbox((0, 0), text, font=font)
            text_h = bbox[3] - bbox[1]
        else:
            text_h = int(size * 0.38)
        bar_y = ty + text_h + int(10 * s)
        bar_x = (size - bar_w) // 2
        for x in range(bar_w):
            ratio = x / bar_w
            r = int(ACCENT_BLUE[0] * (1 - ratio) + ACCENT_CYAN[0] * ratio)
            g = int(ACCENT_BLUE[1] * (1 - ratio) + ACCENT_CYAN[1] * ratio)
            b = int(ACCENT_BLUE[2] * (1 - ratio) + ACCENT_CYAN[2] * ratio)
            draw.rectangle(
                [bar_x + x, bar_y, bar_x + x + 1, bar_y + bar_h],
                fill=(r, g, b, 255),
            )

    return img


def main():
    os.makedirs(ASSETS_DIR, exist_ok=True)

    sizes = {
        "StoreLogo.png": 50,
        "Square50x50Logo.png": 50,
        "Square44x44Logo.png": 44,
        "Square150x150Logo.png": 150,
        "Square310x310Logo.png": 310,
    }

    for filename, size in sizes.items():
        img = make_icon(size)
        path = os.path.join(ASSETS_DIR, filename)
        img.save(path, "PNG")
        print(f"Saved {filename} ({size}x{size})")

    print("\nAll assets regenerated. Rebuild the MSIX to pick them up.")


if __name__ == "__main__":
    main()
