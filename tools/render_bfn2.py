"""
render_bfn2.py — Decode GC I4 tiled glyph atlas from BFN GLY1 section.
Assumes: 256 glyphs, I4 format, 24x24 effective glyph size (stored as 3x3 tiles of 8x8).
"""

import struct
from PIL import Image

BFN_PATH = r'C:\Users\ryana\Documents\dusklight\tools\font_extract\bfn\rodan_b_24_22.bfn'
OUT_PATH = r'C:\Users\ryana\Documents\dusklight\tools\font_extract\rodan_atlas.png'

with open(BFN_PATH, 'rb') as f:
    d = f.read()


def find_section(data, tag):
    tag_bytes = tag.encode('ascii')
    pos = 0x20
    while pos < len(data) - 8:
        if data[pos:pos+4] == tag_bytes:
            size = struct.unpack_from('>I', data, pos+4)[0]
            return data[pos+8:pos+size]
        size = struct.unpack_from('>I', data, pos+4)[0]
        if size == 0: break
        pos += size
    return None


def decode_i4_tile(src, offset):
    """Decode one 8x8 I4 tile into a list of 64 pixel values (0..255)."""
    out = []
    for i in range(32):
        byte = src[offset + i]
        out.append(((byte >> 4) & 0xF) * 17)  # expand 4-bit to 8-bit
        out.append((byte & 0xF) * 17)
    return out


def decode_i4_glyph(src, offset, tile_w, tile_h):
    """Decode a glyph stored as tile_w x tile_h tiles (each 8x8 I4)."""
    px_w = tile_w * 8
    px_h = tile_h * 8
    pixels = [[0] * px_w for _ in range(px_h)]
    tile_offset = offset
    for ty in range(tile_h):
        for tx in range(tile_w):
            tile = decode_i4_tile(src, tile_offset)
            tile_offset += 32  # 8x8 / 2 bytes
            for py in range(8):
                for px in range(8):
                    pixels[ty * 8 + py][tx * 8 + px] = tile[py * 8 + px]
    return pixels, px_w, px_h


gly1 = find_section(d, 'GLY1')
wid1 = find_section(d, 'WID1')

# Expected: 256 glyphs, each 3x3 I4 tiles -&gt; 24x24px, 288 bytes per glyph
TILE_W = 3   # 3 tiles wide  = 24px
TILE_H = 3   # 3 tiles tall  = 24px
BYTES_PER_GLYPH = TILE_W * TILE_H * 32  # = 288
num_glyphs = len(gly1) // BYTES_PER_GLYPH
print(f"GLY1: {len(gly1)} bytes, {BYTES_PER_GLYPH} bytes/glyph -&gt; {num_glyphs} glyphs")

# Render in a 16-column grid
COLS = 16
ROWS = (num_glyphs + COLS - 1) // COLS
GLYPH_PX = 24
img = Image.new('L', (COLS * GLYPH_PX, ROWS * GLYPH_PX), 0)

for g in range(num_glyphs):
    px_col = (g % COLS) * GLYPH_PX
    px_row = (g // COLS) * GLYPH_PX
    pixels, gw, gh = decode_i4_glyph(gly1, g * BYTES_PER_GLYPH, TILE_W, TILE_H)
    for y in range(min(gh, GLYPH_PX)):
        for x in range(min(gw, GLYPH_PX)):
            img.putpixel((px_col + x, px_row + y), pixels[y][x])

img.save(OUT_PATH)
print(f"Saved atlas to: {OUT_PATH}")
print(f"Grid: {COLS} cols x {ROWS} rows, each glyph {GLYPH_PX}x{GLYPH_PX}px")
