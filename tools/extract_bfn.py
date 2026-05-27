"""
extract_bfn.py — Extract rodan_b_24_22.bfn from fontres.arc (Yaz0-compressed RARC)
Usage: python extract_bfn.py <fontres.arc> <output_dir>

Handles:
  1. Yaz0 decompression (Nintendo SZS wrapper)
  2. RARC (JKernel archive) unpacking
  3. Writes every file found (including .bfn) to output_dir
"""

import struct
import os
import sys


# ============================================================================
# Yaz0 decompression
# ============================================================================
def yaz0_decompress(data: bytes) -> bytes:
    magic = data[0:4]
    if magic != b'Yaz0':
        return data  # Not compressed, return as-is

    out_size = struct.unpack_from('>I', data, 4)[0]
    out = bytearray(out_size)
    src = 16  # Yaz0 header is 16 bytes
    dst = 0
    group_bits = 0
    group_count = 0

    while dst < out_size:
        if group_count == 0:
            group_bits = data[src]
            src += 1
            group_count = 8

        if group_bits & 0x80:
            # Direct copy
            out[dst] = data[src]
            src += 1
            dst += 1
        else:
            # Back-reference
            b1 = data[src]; b2 = data[src + 1]; src += 2
            n = b1 >> 4
            dist = (((b1 & 0xF) << 8) | b2) + 1
            if n == 0:
                n = data[src] + 18
                src += 1
            else:
                n += 2
            copy_from = dst - dist
            for _ in range(n):
                out[dst] = out[copy_from]
                dst += 1
                copy_from += 1

        group_bits <<= 1
        group_count -= 1

    return bytes(out)


# ============================================================================
# RARC (JKernel archive) parser
# ============================================================================
def get_cstr(data: bytes, offset: int) -> str:
    end = data.index(b'\x00', offset)
    return data[offset:end].decode('latin-1')


def rarc_extract(data: bytes, output_dir: str):
    if data[0:4] != b'RARC':
        raise ValueError(f"Expected RARC magic, got: {data[0:4]!r}")

    # RARC header (big-endian)
    # 0x00  magic      'RARC'
    # 0x04  file_size  u32
    # 0x08  header_sz  u32 (always 0x20)
    # 0x0C  data_off   u32 (offset of file data, relative to 0x20)
    # 0x10  data_size  u32
    # 0x14..0x1F  padding/unknown
    # 0x20  node_count u32
    # 0x24  node_off   u32 (relative to 0x20)
    # 0x28  entry_count u32
    # 0x2C  entry_off  u32 (relative to 0x20)
    # 0x30  str_pool_sz u32
    # 0x34  str_off    u32 (relative to 0x20)
    # 0x38  next_file_id u16
    # 0x3A  sync_ids   u8

    HDR = 0x20  # "info block" starts at 0x20

    node_count  = struct.unpack_from('>I', data, HDR + 0x00)[0]
    node_off    = struct.unpack_from('>I', data, HDR + 0x04)[0] + HDR
    entry_count = struct.unpack_from('>I', data, HDR + 0x08)[0]
    entry_off   = struct.unpack_from('>I', data, HDR + 0x0C)[0] + HDR
    str_off     = struct.unpack_from('>I', data, HDR + 0x14)[0] + HDR
    data_off    = struct.unpack_from('>I', data, 0x0C)[0] + HDR  # from RARC header

    # Collect all file entries from all nodes
    os.makedirs(output_dir, exist_ok=True)
    extracted = []

    for n in range(node_count):
        nbase = node_off + n * 16
        name_soff   = struct.unpack_from('>I', data, nbase + 0x04)[0]
        num_entries = struct.unpack_from('>H', data, nbase + 0x0A)[0]
        first_entry = struct.unpack_from('>I', data, nbase + 0x0C)[0]

        node_name = get_cstr(data, str_off + name_soff)

        for e in range(num_entries):
            ebase = entry_off + (first_entry + e) * 20
            # Entry layout (20 bytes, big-endian):
            #   0x00  file_id       u16  (0xFFFF = directory)
            #   0x02  name_hash     u16
            #   0x04  flags         u8
            #   0x05  name_off_hi   u8   ← top byte of 24-bit name offset
            #   0x06  name_off_lo   u16  ← low 16 bits of 24-bit name offset
            #   0x08  data_off      u32  ← from data area start
            #   0x0C  data_size     u32
            #   0x10  padding       u32
            file_id    = struct.unpack_from('>H', data, ebase + 0x00)[0]
            name_hi    = data[ebase + 0x05]
            name_lo    = struct.unpack_from('>H', data, ebase + 0x06)[0]
            name_off2  = (name_hi << 16) | name_lo
            file_off   = struct.unpack_from('>I', data, ebase + 0x08)[0]
            file_size  = struct.unpack_from('>I', data, ebase + 0x0C)[0]

            if file_id == 0xFFFF:
                # Directory entry — skip
                continue

            name2 = get_cstr(data, str_off + name_off2)
            if name2 in ('.', '..'):
                continue

            abs_off = data_off + file_off
            file_bytes = data[abs_off : abs_off + file_size]

            out_path = os.path.join(output_dir, name2)
            with open(out_path, 'wb') as f:
                f.write(file_bytes)
            extracted.append((name2, file_size, out_path))
            print(f"  Extracted: {name2} ({file_size} bytes) → {out_path}")

    return extracted


# ============================================================================
# Main
# ============================================================================
if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <fontres.arc> <output_dir>")
        sys.exit(1)

    arc_path = sys.argv[1]
    out_dir  = sys.argv[2]

    print(f"Reading: {arc_path}")
    with open(arc_path, 'rb') as f:
        raw = f.read()

    print(f"  Raw size: {len(raw)} bytes, magic: {raw[0:4]!r}")
    decompressed = yaz0_decompress(raw)
    print(f"  Decompressed size: {len(decompressed)} bytes, magic: {decompressed[0:4]!r}")

    files = rarc_extract(decompressed, out_dir)
    print(f"\nDone. {len(files)} file(s) extracted to: {out_dir}")
