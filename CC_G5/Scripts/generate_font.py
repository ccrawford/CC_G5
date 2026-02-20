import sys
import os
import struct
import argparse
import freetype

# ------------------------------------------------------------
# Helpers
# ------------------------------------------------------------

def measure_cap_m(face):
    face.load_char('M', freetype.FT_LOAD_RENDER)
    bmp = face.glyph.bitmap
    rows = bmp.rows
    pitch = bmp.pitch
    width = bmp.width
    data = bmp.buffer

    used = 0
    for y in range(rows):
        row = data[y * pitch : y * pitch + width]
        if any(b > 0 for b in row):
            used += 1
    return used


def find_font_size_for_cap(face, target):
    low, high = 4, 128
    best = (None, None)

    while low <= high:
        mid = (low + high) // 2
        face.set_pixel_sizes(0, mid)
        h = measure_cap_m(face)

        if best[0] is None or abs(h - target) < abs(best[1] - target):
            best = (mid, h)

        if h < target:
            low = mid + 1
        elif h > target:
            high = mid - 1
        else:
            break

    return best


# ------------------------------------------------------------
# Args
# ------------------------------------------------------------

def parse_char_ranges(range_str):
    """
    Parse a range string like "32-126,167,169,176" into a sorted list of codepoints.
    Supports:
      - Single values: 167
      - Ranges: 32-126
      - Mixed: 32-126,167,169,176-180
    """
    codepoints = set()
    for part in range_str.split(","):
        part = part.strip()
        if "-" in part:
            start, end = part.split("-", 1)
            codepoints.update(range(int(start), int(end) + 1))
        else:
            codepoints.add(int(part))
    return sorted(codepoints)


parser = argparse.ArgumentParser()
parser.add_argument("ttf")
parser.add_argument("--ascii-range", type=str, default="32-126",
                    help="Character ranges, e.g. '32-126,167,176' (default: 32-126)")
parser.add_argument("--font-size", type=int)
parser.add_argument("--target-cap-height", type=int)
parser.add_argument("--spacing-adjust", type=int, default=0)
args = parser.parse_args()

if not args.font_size and not args.target_cap_height:
    parser.error("Must specify --font-size or --target-cap-height")

codepoints = parse_char_ranges(args.ascii_range)

# ------------------------------------------------------------
# Load font
# ------------------------------------------------------------

face = freetype.Face(args.ttf)

if args.target_cap_height:
    size, cap_height = find_font_size_for_cap(face, args.target_cap_height)
else:
    size = args.font_size
    face.set_pixel_sizes(0, size)
    cap_height = measure_cap_m(face)

face.set_pixel_sizes(0, size)

ascent = face.size.ascender >> 6
descent = -(face.size.descender >> 6)

# ------------------------------------------------------------
# Render glyphs
# ------------------------------------------------------------

glyphs = []

for codepoint in codepoints:
    face.load_char(chr(codepoint), freetype.FT_LOAD_RENDER)
    g = face.glyph
    bmp = g.bitmap

    # Strip pitch padding correctly
    bitmap = bytearray()
    for y in range(bmp.rows):
        row = bmp.buffer[y * bmp.pitch : y * bmp.pitch + bmp.width]
        bitmap += bytes(row)

    glyphs.append({
        "code": codepoint,
        "width": bmp.width,
        "height": bmp.rows,
        "advance": (g.advance.x >> 6) + args.spacing_adjust,
        "left": g.bitmap_left,
        "top": g.bitmap_top,
        "bitmap": bytes(bitmap),
    })


# ------------------------------------------------------------
# Build VLW binary (CORRECT FORMAT)
# ------------------------------------------------------------

vlw = bytearray()

# Header: 6 x int32 = 24 bytes
vlw += struct.pack(">I", len(glyphs))    # glyph count
vlw += struct.pack(">I", 11)              # version (always 11)
vlw += struct.pack(">I", size)            # font size
vlw += struct.pack(">I", 0)               # reserved (always 0)
vlw += struct.pack(">I", ascent)          # ascent
vlw += struct.pack(">I", descent)         # descent

# Glyph metrics: 7 x int32 = 28 bytes each
for g in glyphs:
    vlw += struct.pack(">I", g["code"])      # codePoint
    vlw += struct.pack(">I", g["height"])    # height
    vlw += struct.pack(">I", g["width"])     # width
    vlw += struct.pack(">I", g["advance"])   # advanceWidth
    vlw += struct.pack(">i", g["top"])       # topSideBearing (signed)
    vlw += struct.pack(">i", g["left"])      # leftSideBearing (signed)
    vlw += struct.pack(">I", 0)              # reserved

# Bitmap data
for g in glyphs:
    vlw += g["bitmap"]

# ------------------------------------------------------------
# Emit C header
# ------------------------------------------------------------

base = os.path.splitext(os.path.basename(args.ttf))[0]
name = f"{base}{cap_height if args.target_cap_height else size}"
macro = name.upper()

print("#include <pgmspace.h>\n")

print(f"#define {macro}_CAP_M_HEIGHT {cap_height}")
print(f"#define {macro}_ASCENT       {ascent}")
print(f"#define {macro}_DESCENT      {descent}")
print(f"#define {macro}_FONT_SIZE    {size}\n")

print(f"const uint8_t {name}[] PROGMEM = {{")

for i, b in enumerate(vlw):
    if i % 16 == 0:
        print("  ", end="")
    print(f"0x{b:02X}", end="")
    if i != len(vlw) - 1:
        print(", ", end="")
    if i % 16 == 15:
        print()

if len(vlw) % 16 != 0:
    print()

print("};")