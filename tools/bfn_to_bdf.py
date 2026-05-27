"""
bfn_to_bdf.py — Convert rodan_b_24_22.bfn to rodan.bdf (Bitmap Distribution Format).
BDF is natively loaded by FreeType, which RmlUI uses as its font backend.

Atlas confirmed: 256 glyphs, GC I4 format, 256x576px, GLY1 has 24-byte sub-header.
MAP1: direct map U+0020..U+00FF -> glyph indices 0..223.
WID1: 3-byte entries (left_bearing, glyph_width, advance) at offset 4 (after u32 count).

Usage: python bfn_to_bdf.py
Output: rodan.bdf next to this script
"""

import struct, os

BFN_PATH = r'C:\Users\<user>\Documents\dusklight\tools\font_extract\bfn\rodan_b_24_22.bfn'
BDF_PATH = r'C:\Users\<user>\Documents\dusklight\tools\font_extract\rodan.bdf'

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


gly1_raw = find_section(d, 'GLY1')
wid1_raw = find_section(d, 'WID1')

# ---- Decode GC I4 texture (skip 24-byte GLY1 sub-header, 256x576 atlas) ----
GLY_SKIP  = 24
ATLAS_W   = 256
ATLAS_H   = 576
GLYPH_W   = 16    # grid columns
GLYPH_H   = 16    # grid rows
CELL_W    = ATLAS_W // GLYPH_W   # 16px per cell column
CELL_H    = ATLAS_H // GLYPH_H   # 36px per cell row

gly1 = gly1_raw[GLY_SKIP:]

def decode_i4_atlas(src, atlas_w, atlas_h):
    """Decode a full GC I4 texture into a flat pixel list."""
    pixels = [0] * (atlas_w * atlas_h)
    offset = 0
    for ty in range(atlas_h // 8):
        for tx in range(atlas_w // 8):
            for row in range(8):
                for col_pair in range(4):
                    if offset >= len(src): break
                    byte = src[offset]; offset += 1
                    hi = ((byte >> 4) & 0xF) * 17
                    lo = (byte & 0xF) * 17
                    px_idx = (ty * 8 + row) * atlas_w + (tx * 8 + col_pair * 2)
                    if px_idx < len(pixels):     pixels[px_idx]     = hi
                    if px_idx + 1 < len(pixels): pixels[px_idx + 1] = lo
    return pixels

atlas = decode_i4_atlas(gly1[:ATLAS_W * ATLAS_H // 2], ATLAS_W, ATLAS_H)
print(f"Atlas decoded: {ATLAS_W}x{ATLAS_H}, {len(atlas)} pixels")


def get_glyph_cell(atlas_pixels, glyph_idx, atlas_w, cell_w, cell_h, cols):
    """Extract a cell_w x cell_h pixel grid for the given glyph index."""
    col = glyph_idx % cols
    row = glyph_idx // cols
    ox  = col * cell_w
    oy  = row * cell_h
    cell = []
    for y in range(cell_h):
        row_pixels = []
        for x in range(cell_w):
            px = (oy + y) * atlas_w + (ox + x)
            row_pixels.append(atlas_pixels[px] if px < len(atlas_pixels) else 0)
        cell.append(row_pixels)
    return cell


def cell_to_bdf_bitmap(cell, threshold=64):
    """Convert a pixel cell to BDF BITMAP hex rows."""
    h = len(cell)
    w = len(cell[0]) if h else 0
    # BDF bitmap width is rounded up to a multiple of 8 bits
    bdf_w = (w + 7) & ~7
    lines = []
    for row in cell:
        bits = 0
        for x in range(bdf_w):
            bits <<= 1
            if x < w and row[x] >= threshold:
                bits |= 1
        hex_bytes = bdf_w // 8
        lines.append(format(bits, f'0{hex_bytes * 2}X'))
    return lines


# ---- Parse WID1 (3-byte entries, offset 4 = skip u32 count header) ----
# Format: (left_bearing u8, glyph_pixel_width u8, advance u8)
WID_HDR = 4
wid_entries = []
i = WID_HDR
while i + 2 < len(wid1_raw):
    lb  = wid1_raw[i]
    gw  = wid1_raw[i + 1]
    adv = wid1_raw[i + 2]
    wid_entries.append((lb, gw, adv))
    i += 3
print(f"WID1: {len(wid_entries)} entries parsed")

# MAP1: direct map U+0020..U+00FF -> glyph 0..223
CHAR_START = 0x0020
CHAR_END   = 0x00FF
NUM_CHARS  = CHAR_END - CHAR_START + 1

# ---- Determine font metrics from glyph cells ----
# The cell is CELL_W x CELL_H but the actual glyph body may be smaller.
# Use a slightly tighter crop: known from filename "24_22" = cell 24h x 22w,
# atlas decoded as 16px cell (CELL_W=16). Scale factor: actual cell is 22-24px.
# Since our atlas decoded to 16px cells from 256px/16cols, each cell is 16px.
# The filename "24_22" likely means 24pt at 72DPI, cell is 22px wide.
# We use CELL_W=16 as the baseline glyph box.

POINT_SIZE = 24 * 10   # BDF uses decipoints (24pt * 10 = 240)
DPI        = 75
ASCENT     = 18        # pixels above baseline (estimated from visible glyphs)
DESCENT    = 6         # pixels below baseline

# ---- Write BDF file ----
lines = []
lines.append("STARTFONT 2.1")
lines.append(f"FONT -Dusklight-Rodan-Medium-R-Normal--{CELL_H}-{POINT_SIZE}-{DPI}-{DPI}-C-0-ISO8859-1")
lines.append(f"SIZE {POINT_SIZE // 10} {DPI} {DPI}")
lines.append(f"FONTBOUNDINGBOX {CELL_W} {CELL_H} 0 -{DESCENT}")
lines.append("STARTPROPERTIES 9")
lines.append(f"FONT_ASCENT {ASCENT}")
lines.append(f"FONT_DESCENT {DESCENT}")
lines.append(f"DEFAULT_CHAR 32")
lines.append(f"PIXEL_SIZE {CELL_H}")
lines.append(f"POINT_SIZE {POINT_SIZE}")
lines.append(f"RESOLUTION_X {DPI}")
lines.append(f"RESOLUTION_Y {DPI}")
lines.append('FAMILY_NAME "Rodan"')
lines.append('WEIGHT_NAME "Medium"')
lines.append("ENDPROPERTIES")
lines.append(f"CHARS {NUM_CHARS}")

for char_code in range(CHAR_START, CHAR_END + 1):
    glyph_idx = char_code - CHAR_START
    wid_idx   = glyph_idx  # same index into wid_entries

    # Look up width data; fall back to full-cell defaults
    if wid_idx < len(wid_entries):
        lb, gw, adv = wid_entries[wid_idx]
        # Clamp to sane values (some entries may be malformed)
        lb  = min(lb,  CELL_W - 1)
        gw  = min(gw,  CELL_W - lb)
        adv = max(adv, gw + lb) if adv > 0 else (lb + gw + 1)
    else:
        lb, gw, adv = 2, CELL_W - 4, CELL_W

    # BDF SWIDTH = advance in 1/1000 of em (scaled to DPI)
    swidth = int(adv * 1000 * 72 / (DPI * CELL_H))
    char_name = f"U+{char_code:04X}"
    cell = get_glyph_cell(atlas, glyph_idx, ATLAS_W, CELL_W, CELL_H, GLYPH_W)
    bitmap_lines = cell_to_bdf_bitmap(cell)

    lines.append(f"STARTCHAR {char_name}")
    lines.append(f"ENCODING {char_code}")
    lines.append(f"SWIDTH {swidth} 0")
    lines.append(f"DWIDTH {adv} 0")
    lines.append(f"BBX {CELL_W} {CELL_H} {lb} -{DESCENT}")
    lines.append("BITMAP")
    lines.extend(bitmap_lines)
    lines.append("ENDCHAR")

lines.append("ENDFONT")

with open(BDF_PATH, 'w', encoding='ascii') as f:
    f.write('\n'.join(lines))

print(f"BDF written: {BDF_PATH}")
print(f"Characters: U+{CHAR_START:04X} to U+{CHAR_END:04X} ({NUM_CHARS} glyphs)")
print(f"Cell size: {CELL_W}x{CELL_H}px")
