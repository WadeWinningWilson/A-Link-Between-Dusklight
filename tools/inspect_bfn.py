"""inspect_bfn.py — Parse BFN section headers and INF1 font metrics."""
import struct
from pathlib import Path

BFN_PATH = Path(__file__).resolve().parent / "font_extract" / "bfn" / "rodan_b_24_22.bfn"

with open(BFN_PATH, 'rb') as f:
    d = f.read()

pos = 0x20
while pos < len(d) - 8:
    tag = d[pos:pos+4]
    try:
        tag_str = tag.decode('ascii')
    except Exception:
        break
    if not tag_str[0].isalpha():
        break
    size = struct.unpack_from('>I', d, pos+4)[0]
    print(f'Section {tag_str!r} at {hex(pos)}, size={hex(size)} ({size} bytes)')

    if tag_str == 'INF1':
        data = d[pos+8:pos+size]
        print(f'  INF1 raw (24 bytes): {" ".join(f"{b:02x}" for b in data[:24])}')
        print(f'  leading={data[0]}, ascent={data[1]}, descent={data[2]}')
        print(f'  cell_w={data[3]}, cell_h={data[4]}, sheet_count={data[5]}, max_char_w={data[6]}')
        sheet_size  = struct.unpack_from('>I', data, 7)[0]
        default_char = struct.unpack_from('>H', data, 11)[0]
        tex_fmt     = struct.unpack_from('>H', data, 13)[0]
        sheet_cols  = struct.unpack_from('>H', data, 15)[0]
        sheet_rows  = struct.unpack_from('>H', data, 17)[0]
        sheet_w     = struct.unpack_from('>H', data, 19)[0]
        sheet_h     = struct.unpack_from('>H', data, 21)[0]
        print(f'  sheet_size={hex(sheet_size)}, default_char={hex(default_char)}')
        print(f'  tex_fmt={tex_fmt}, {sheet_cols} cols x {sheet_rows} rows per sheet, sheet={sheet_w}x{sheet_h}px')

    elif tag_str == 'GLY1':
        data = d[pos+8:pos+size]
        print(f'  GLY1 glyph data: {len(data)} bytes (raw pixel atlas)')

    if size == 0:
        break
    pos += size
