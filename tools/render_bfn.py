"""
render_bfn.py — Render rodan_b_24_22.bfn glyph atlas to PNG for visual inspection.

The BFN GLY1 section stores glyph pixel data in GameCube I4 texture format
(4-bit intensity, each byte = 2 pixels, stored in 8x8 tiles).
"""

import struct
from PIL import Image

BFN_PATH = r'C:\Users\<user>\Documents\dusklight\tools\font_extract\bfn\rodan_b_24_22.bfn'
OUT_PATH = r'C:\Users\<user>\Documents\dusklight\tools\font_extract\rodan_atlas.png'

with open(BFN_PATH, 'rb') as f:
    d = f.read()

# ---- Parse sections ----
def find_section(data, tag):
    tag_bytes = tag.encode('ascii')
    pos = 0x20
    while pos < len(data) - 8:
        t = data[pos:pos+4]
        if t == tag_bytes:
            size = struct.unpack_from('>I', data, pos+4)[0]
            return data[pos+8:pos+size], size
        size = struct.unpack_from('>I', data, pos+4)[0]
        if size == 0:
            break
        pos += size
    return None, 0

inf1, _ = find_section(d, 'INF1')
gly1, gly_size = find_section(d, 'GLY1')

# ---- Parse INF1 metrics ----
# INF1 data layout (best guess from filename "24_22" = cell 24h x 22w):
# The filename rodan_b_24_22 suggests cell_height=24, cell_width=22
# Try to extract sheet dimensions from INF1
# Known offsets from GC JUTFont research:
#   +0x00: leading (u8)
#   +0x01: ascent  (u8)
#   +0x02: descent (u8)
#   +0x03: cell_width  (u8)  ← from filename: should be ~22
#   +0x04: cell_height (u8)  ← from filename: should be ~24
#   +0x05: sheet_count (u8)
#   +0x06: max_char_width (u8)
#   +0x07-0x0A: sheet_size (u32 BE)  ← size of each glyph sheet in bytes
#   +0x0B-0x0C: default_char (u16)
#   +0x0D-0x0E: texture_format (u16) ← 0=I4, 1=I8, 4=IA4, 5=IA8
#   +0x0F-0x10: sheet_cols (u16) ← glyphs per row
#   +0x11-0x12: sheet_rows (u16) ← glyph rows per sheet
#   +0x13-0x14: sheet_width  (u16) ← atlas pixel width
#   +0x15-0x16: sheet_height (u16) ← atlas pixel height

print("INF1 bytes:", inf1.hex(' '))

# Try interpreting with 1-byte alignment shift (data starts differently)
# Raw: 00 00 00 15 00 03 00 16 00 18 00 00 00 00 00 00 00 00 00 00 00 00 00 00
# Check a few plausible layouts:
for start_offset in [0, 3, 4]:
    data = inf1[start_offset:]
    if len(data) < 16:
        continue
    cell_w = data[3]
    cell_h = data[4]
    if cell_w in range(16, 32) and cell_h in range(16, 32):
        sheet_size = struct.unpack_from('>I', data, 7)[0] if len(data) >= 11 else 0
        tex_fmt = struct.unpack_from('>H', data, 13)[0] if len(data) >= 15 else 0
        sheet_w = struct.unpack_from('>H', data, 19)[0] if len(data) >= 21 else 0
        sheet_h = struct.unpack_from('>H', data, 21)[0] if len(data) >= 23 else 0
        print(f"  [offset +{start_offset}] cell={cell_w}x{cell_h}, sheet_size={hex(sheet_size)}, "
              f"tex_fmt={tex_fmt}, atlas={sheet_w}x{sheet_h}")

# Since filename says 24_22, use those values as ground truth
# and try to determine texture format and atlas dimensions from GLY1 size
# I4 format: 1 byte = 2 pixels → GLY_size = (atlas_w * atlas_h) / 2
# I8 format: 1 byte = 1 pixel → GLY_size = atlas_w * atlas_h

gly_data_size = len(gly1)
print(f"\nGLY1 data size: {gly_data_size} bytes")

# Try common atlas sizes
CELL_W, CELL_H = 22, 24
for cols in [8, 16, 32]:
    for rows in [8, 16, 32]:
        atlas_w = cols * CELL_W
        atlas_h = rows * CELL_H
        i4_size = (atlas_w * atlas_h) // 2
        i8_size = atlas_w * atlas_h
        if abs(i4_size - gly_data_size) < 512:
            print(f"  I4 match: {cols}x{rows} grid ({atlas_w}x{atlas_h}px), "
                  f"expected={i4_size}, actual={gly_data_size}")
        if abs(i8_size - gly_data_size) < 512:
            print(f"  I8 match: {cols}x{rows} grid ({atlas_w}x{atlas_h}px), "
                  f"expected={i8_size}, actual={gly_data_size}")

# Best guess: 16x16 grid of 22x24 glyphs = 352x384px, I4 = 352*384/2 = 67584
# Actual GLY1 = 73752; let's try other cell sizes
print("\nTrying to find matching cell size:")
for cw in range(16, 32):
    for ch in range(16, 32):
        for cols in [8, 16, 32]:
            for rows in [8, 16, 32]:
                aw = cols * cw; ah = rows * ch
                if abs((aw * ah) // 2 - gly_data_size) < 8:
                    print(f"  I4: cell={cw}x{ch}, grid={cols}x{rows}, atlas={aw}x{ah}")
                if abs(aw * ah - gly_data_size) < 8:
                    print(f"  I8: cell={cw}x{ch}, grid={cols}x{rows}, atlas={aw}x{ah}")
