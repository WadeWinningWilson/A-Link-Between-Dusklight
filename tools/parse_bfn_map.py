"""
parse_bfn_map.py — Parse MAP1 and WID1 sections from rodan_b_24_22.bfn.
Needed to build the BDF character mapping table.
"""
import struct

BFN_PATH = r'C:\Users\ryana\Documents\dusklight\tools\font_extract\bfn\rodan_b_24_22.bfn'

with open(BFN_PATH, 'rb') as f:
    d = f.read()


def find_section(data, tag):
    tag_bytes = tag.encode('ascii')
    pos = 0x20
    while pos < len(data) - 8:
        if data[pos:pos+4] == tag_bytes:
            size = struct.unpack_from('>I', data, pos+4)[0]
            return data[pos+8:pos+size], pos, size
        size = struct.unpack_from('>I', data, pos+4)[0]
        if size == 0: break
        pos += size
    return None, 0, 0


inf1, _, _ = find_section(d, 'INF1')
map1, map_pos, map_size = find_section(d, 'MAP1')
wid1, wid_pos, wid_size = find_section(d, 'WID1')

print("=== INF1 ===")
print("Raw:", inf1.hex(' '))

print("\n=== MAP1 ===")
print(f"Offset: {hex(map_pos)}, Size: {hex(map_size)}, Data: {len(map1)} bytes")
print("Raw (all):", map1.hex(' '))

print("\n=== WID1 ===")
print(f"Offset: {hex(wid_pos)}, Size: {hex(wid_size)}, Data: {len(wid1)} bytes")
print("Raw (first 48 bytes):", wid1[:48].hex(' '))

# Try interpreting WID1 as 3-byte entries: (left_bearing u8, glyph_width u8, advance u8)
print("\nWID1 as 3-byte entries (char_idx, left, width, advance):")
for i in range(min(len(wid1) // 3, 30)):
    left = wid1[i*3]
    gw   = wid1[i*3+1]
    adv  = wid1[i*3+2]
    ch   = chr(i + 0x20) if 0x20 <= i + 0x20 < 128 else '?'
    print(f"  [{i:3d}] left={left:3d} width={gw:3d} adv={adv:3d}  (char ~{hex(i+0x20)} '{ch}')")

# Try interpreting WID1 with a u16 prefix (code_start + count) then 3-byte entries
print("\nWID1 with u16 header (code_start, count):")
if len(wid1) >= 4:
    code_start = struct.unpack_from('>H', wid1, 0)[0]
    count      = struct.unpack_from('>H', wid1, 2)[0]
    print(f"  code_start={hex(code_start)}, count={count}")
    for i in range(min(count, 20)):
        base = 4 + i * 3
        if base + 2 >= len(wid1):
            break
        left = wid1[base]; gw = wid1[base+1]; adv = wid1[base+2]
        char = code_start + i
        ch = chr(char) if 0x20 <= char < 128 else '?'
        print(f"  [{i:3d}] U+{char:04X} '{ch}': left={left} width={gw} adv={adv}")
