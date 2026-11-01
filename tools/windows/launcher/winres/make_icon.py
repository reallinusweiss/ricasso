"""Generates the program icon (original, generic sword motif)."""
import sys
from PIL import Image, ImageDraw, ImageFilter

S = 1024  # working size, scaled down

def draw():
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    # Background: rounded square with a vertical gradient
    grad = Image.new("RGBA", (S, S))
    gd = ImageDraw.Draw(grad)
    top, bottom = (36, 72, 120), (18, 34, 64)
    for y in range(S):
        t = y / (S - 1)
        c = tuple(int(top[i] * (1 - t) + bottom[i] * t) for i in range(3)) + (255,)
        gd.line([(0, y), (S, y)], fill=c)
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle([24, 24, S - 24, S - 24], radius=210, fill=255)
    img.paste(grad, (0, 0), mask)

    # Soft glow behind the sword
    glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse([250, 200, 780, 730], fill=(120, 190, 255, 70))
    glow = glow.filter(ImageFilter.GaussianBlur(90))
    img = Image.alpha_composite(img, Image.composite(glow, Image.new("RGBA", (S, S)), mask))

    # Sword, diagonal (tip at the top right), drawn in a rotated helper image
    sw = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(sw)
    cx = S // 2
    # Blade
    d.polygon([(cx, 120), (cx + 62, 230), (cx + 62, 640), (cx - 62, 640), (cx - 62, 230)], fill=(222, 230, 240, 255))
    d.polygon([(cx, 120), (cx + 62, 230), (cx + 62, 640), (cx, 640)], fill=(176, 190, 208, 255))
    d.line([(cx, 190), (cx, 620)], fill=(250, 252, 255, 255), width=10)
    # Crossguard
    d.rounded_rectangle([cx - 190, 630, cx + 190, 700], radius=34, fill=(232, 178, 58, 255))
    d.rounded_rectangle([cx - 190, 666, cx + 190, 700], radius=30, fill=(196, 140, 36, 255))
    # Grip
    d.rounded_rectangle([cx - 40, 700, cx + 40, 860], radius=20, fill=(120, 70, 40, 255))
    for y in range(720, 850, 34):
        d.line([(cx - 40, y), (cx + 40, y + 16)], fill=(90, 50, 28, 255), width=10)
    # Pommel
    d.ellipse([cx - 62, 846, cx + 62, 960], fill=(232, 178, 58, 255))
    d.ellipse([cx - 30, 872, cx + 22, 920], fill=(255, 222, 130, 255))
    sw = sw.rotate(-38, resample=Image.BICUBIC, center=(cx, 540))
    sw = sw.resize((int(S * 1.06), int(S * 1.06)), Image.LANCZOS)
    layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    off = (S - sw.width) // 2
    sw = sw.crop((0, 0, sw.width, sw.height))
    # Shadow
    shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    shadow.paste((0, 0, 0, 120), (off + 14, off + 22), sw)
    shadow = shadow.filter(ImageFilter.GaussianBlur(16))
    img = Image.alpha_composite(img, Image.composite(shadow, Image.new("RGBA", (S, S)), mask))
    layer.paste(sw, (off, off), sw)
    img = Image.alpha_composite(img, layer)
    return img

img = draw()
sizes = [256, 128, 64, 48, 40, 32, 24, 20, 16]
img.save(sys.argv[1], format="ICO", sizes=[(s, s) for s in sizes])
img.resize((512, 512), Image.LANCZOS).save(sys.argv[2])
