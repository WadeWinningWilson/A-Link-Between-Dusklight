"""
render_bfn3.py — Decode the BFN glyph atlas as a single GC I4 texture.
Tries several atlas dimensions until one produces readable glyphs.
GC I4: 4 bits/px, tiles stored as 8x8 blocks in row-major tile order.
"""
import struct
from pathlib import Path
from PIL import Image
import os

_FONT_EXTRACT = Path(__file__).resolve().parent / "font_extract"
BFN_PATH = _FONT_EXTRACT / "bfn" / "rodan_b_24_22.bfn"
OUT_DIR = _FONT_EXTRACT

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


def decode_i4_texture(src, width, height):
    """Decode a full GC I4 texture (width x height px) stored in 8x8 tiles."""
    assert width % 8 == 0 and height % 8 == 0, f"Dims must be multiples of 8: {width}x{height}"
    pixels = [0] * (width * height)
    offset = 0
    for ty in range(height // 8):
        for tx in range(width // 8):
            for row in range(8):
                for col_pair in range(4):
                    if offset >= len(src):
                        break
                    byte = src[offset]; offset += 1
                    hi = ((byte >> 4) & 0xF) * 17
                    lo = (byte & 0xF) * 17
                    px = (ty * 8 + row) * width + (tx * 8 + col_pair * 2)
                    if px < len(pixels):     pixels[px] = hi
                    if px + 1 < len(pixels): pixels[px + 1] = lo
    return pixels


gly1 = find_section(d, 'GLY1')
data_size = len(gly1)
print(f"GLY1 data: {data_size} bytes")

# Candidate: skip a small sub-header if present, try several sheet sizes
candidates = [
    (0,  384, 192),  # 384x192 = 73728 bytes I4 (skip 0 bytes header)
    (0,  512, 144),  # 512x144
    (24, 384, 192),  # skip 24-byte sub-header
    (24, 256, 576),
    (0,  256, 288),
    (0,  512, 288),
]

for skip, w, h in candidates:
    expected = w * h // 2
    actual   = data_size - skip
    diff     = abs(expected - actual)
    if diff > 512:
        print(f"skip={skip}, {w}x{h}: expected {expected}, got {actual} (off {diff}) -- SKIP")
        continue
    print(f"skip={skip}, {w}x{h}: expected {expected}, got {actual} (diff {diff}) -- TRYING")
    raw = gly1[skip:skip + expected]
    if len(raw) < expected:
        raw = raw + bytes(expected - len(raw))  # zero-pad if needed
    px = decode_i4_texture(raw, w, h)
    img = Image.new('L', (w, h), 0)
    img.putdata(px)
    out = os.path.join(OUT_DIR, f'atlas_{skip}_{w}x{h}.png')
    img.save(out)
    print(f"  Saved: {out}")
