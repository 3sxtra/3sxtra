#!/usr/bin/env python3
"""
Extract screen-font (HUD) texture pages from scrscrn.ppg (AFS entry 10).

Parses the PPG indexed format, applies palettes, and saves each page as a PNG.
Optionally upscales to 4x via nearest-neighbor for testing.

Usage:
    python extract_ui_pages.py                      # extract pages at native res
    python extract_ui_pages.py --scale 4             # upscale 4x (nearest)
    python extract_ui_pages.py --output assets/ui    # save directly to override dir
"""

import argparse
import os
import struct
import sys
import zlib
from pathlib import Path

from PIL import Image

from sprite_common import read_afs, read_afs_file

# AFS entry index for scrscrn.ppg (from sc_sub.c: load_it_use_any_key2(10, ...))
SCRSCRN_AFS_ENTRY = 10


def parse_ppg_header(data):
    """Parse PPG file, returning list of (offset, size) for each pTEX chunk."""
    chunks = []
    ofs = 0
    while ofs < len(data) - 8:
        magic = data[ofs:ofs + 4]
        if magic == b"pEND":
            break
        file_size = struct.unpack_from(">I", data, ofs + 4)[0]
        if magic == b"pTEX":
            chunks.append((ofs, file_size))
        ofs += (file_size + 3) & ~3
    return chunks


def decode_ppg_page(data, chunk_offset, chunk_size, palette_colors, force_bank=0):
    """Decode a single pTEX chunk into an RGBA PIL Image.

    The PPG pTEX format (PPGFileHeader):
      Bytes 0-3:   magic "pTEX"
      Bytes 4-7:   file_size (BE)
      Byte  8:     width (in 16px blocks)
      Byte  9:     height (in 16px blocks)
      Byte  10:    compress
      Byte  11:    pixel (bits 0-1: depth, bit 5: nibble-swap flag)
      Bytes 12-13: formARGB (BE u16)
      Bytes 14-15: transNums (BE u16, palette region count)
      Bytes 16+:   trans entries (3 bytes each: pal_bank, i_point, cofs_xy)
      After trans:  compressed indexed pixel data

    pixel & 3: 0 = 4-bit indexed (16 colors), 1 = 8-bit indexed (256 colors)
    pixel & 0x20: nibble-swap flag (for 4-bit mode)
    """
    wb = data[chunk_offset + 8]
    hb = data[chunk_offset + 9]
    compress = data[chunk_offset + 10]
    pixel_field = data[chunk_offset + 11]
    pixel_mode = pixel_field & 3
    trans_nums = struct.unpack_from(">H", data, chunk_offset + 14)[0]

    w = wb * 16
    h = hb * 16

    # Determine pixel depth
    is_4bit = (pixel_mode == 0)
    pal_stride = 16 if is_4bit else 256

    # Parse palette region entries (if any)
    trans = []
    for t in range(trans_nums):
        t_ofs = chunk_offset + 16 + t * 3
        pal_bank = data[t_ofs]
        i_point = data[t_ofs + 1]
        cofs_xy = data[t_ofs + 2]
        xs = (cofs_xy >> 4) + 1
        ys = (cofs_xy & 0xF) + 1
        sx = i_point % wb
        sy = i_point // wb
        trans.append((pal_bank, sx, sy, xs, ys))

    # Compressed pixel data starts after trans entries
    comp_start = chunk_offset + 16 + trans_nums * 3
    comp_data = data[comp_start:chunk_offset + chunk_size]

    # Decompress
    if compress & 3:
        raw = zlib.decompress(comp_data)
    else:
        raw = comp_data

    # Build per-pixel palette bank map
    if trans:
        default_pal = trans[0][0]
        pal_map = [[default_pal] * w for _ in range(h)]
        for pal_bank, sx, sy, xs, ys in trans:
            px_x, px_y = sx * 16, sy * 16
            px_w, px_h = xs * 16, ys * 16
            for py in range(px_y, min(px_y + px_h, h)):
                for px in range(px_x, min(px_x + px_w, w)):
                    pal_map[py][px] = pal_bank
    else:
        # No trans entries: fallback to force_bank
        pal_map = None

    nibble_swap = (pixel_field & 0x20) != 0

    # Render indexed pixels to RGBA
    buf = bytearray(w * h * 4)
    for py in range(h):
        for px in range(w):
            if is_4bit:
                byte_idx = py * (w // 2) + px // 2
                if byte_idx >= len(raw):
                    continue
                byte_val = raw[byte_idx]
                is_odd = (px & 1) != 0
                if nibble_swap:
                    # 3SX spec: if nibble-swap is set, high nibble is the RIGHT (odd) pixel
                    idx = (byte_val >> 4) if is_odd else byte_val
                else:
                    # Standard: high nibble is the LEFT (even) pixel
                    idx = byte_val if is_odd else (byte_val >> 4)
                idx &= 0x0F

            else:
                byte_idx = py * w + px
                if byte_idx >= len(raw):
                    continue
                idx = raw[byte_idx]
            if idx == 0:
                continue  # transparent
            pal_bank = pal_map[py][px] if pal_map else force_bank
            color_idx = pal_bank * pal_stride + idx
            off = (py * w + px) * 4
            if color_idx < len(palette_colors):
                r, g, b, a = palette_colors[color_idx]
                buf[off] = r
                buf[off + 1] = g
                buf[off + 2] = b
                buf[off + 3] = a

    return Image.frombytes("RGBA", (w, h), bytes(buf))


def parse_ppl_palette(data, pal_index=0):
    """Parse a single pPAL palette chunk from PPG file by index.

    PPL header (16 bytes, all fields big-endian):
      Bytes 0-3:   magic "pPAL"
      Bytes 4-7:   fileSize (BE u32)
      Bytes 8-9:   free (u16)
      Byte  10:    compress (0=raw, 1=zlib, 2=lz77)
      Byte  11:    c_mode (0=16 colors, 1=64 colors, 2=256 colors)
      Bytes 12-13: formARGB (BE u16)
      Bytes 14-15: palettes count (BE u16)

    scrscrn.ppg palette sets (matching ppgSetupPalChunk calls in sc_sub.c):
      0 = ppgScrPal (default HUD) — 32 × 16-color
      1 = ppgScrPalFace — 260 × 16-color
      2 = ppgScrPalShot — 1 × 256-color
      3 = ppgScrPalOpt — 1 × 256-color
    """
    col_mode_width = [16, 64, 256, 0]
    ofs = 0
    ppal_idx = 0

    while ofs < len(data) - 8:
        magic = data[ofs:ofs + 4]
        if magic == b"pEND":
            break
        file_size = struct.unpack_from(">I", data, ofs + 4)[0]

        if magic == b"pPAL":
            if ppal_idx == pal_index:
                compress = data[ofs + 10]
                c_mode = data[ofs + 11] & 3
                form_argb = struct.unpack_from(">H", data, ofs + 12)[0]
                pal_count = struct.unpack_from(">H", data, ofs + 14)[0]
                col_items = col_mode_width[c_mode]

                comp_data = data[ofs + 16:ofs + file_size]

                is_32bit = (form_argb == 0x8888)
                bytes_per_color = 4 if is_32bit else 2

                if compress & 3:
                    try:
                        raw = zlib.decompress(comp_data)
                    except zlib.error:
                        raw = comp_data
                else:
                    raw = comp_data

                all_colors = []
                for i in range(min(len(raw) // bytes_per_color, pal_count * col_items)):
                    if is_32bit:
                        val = struct.unpack_from(">I", raw, i * 4)[0]
                        a = (val >> 24) & 0xFF
                        r = (val >> 16) & 0xFF
                        g = (val >> 8) & 0xFF
                        b = val & 0xFF
                        if i % col_items == 0 and val == 0:
                            all_colors.append((0, 0, 0, 0))
                        else:
                            all_colors.append((r, g, b, a))
                    else:
                        # PPG palette: BE u16, bits: [A:15] [R:14-10] [G:9-5] [B:4-0]
                        # Confirmed by diagnostic: 0xffe0 → Yellow (255,255,0)
                        val = struct.unpack_from(">H", raw, i * 2)[0]
                        b5 = val & 0x1F
                        g5 = (val >> 5) & 0x1F
                        r5 = (val >> 10) & 0x1F
                        r = (r5 * 255 + 15) // 31
                        g = (g5 * 255 + 15) // 31
                        b = (b5 * 255 + 15) // 31
                        a = 255 if (val & 0x8000) else 0
                        if i % col_items == 0:
                            all_colors.append((0, 0, 0, 0))
                        else:
                            all_colors.append((r, g, b, a))

                print(f"  pPAL[{ppal_idx}]: {pal_count} palettes, {col_items} colors/pal, "
                      f"compress={compress}, {'32bit' if is_32bit else '16bit'}")
                return all_colors

            ppal_idx += 1

        ofs += (file_size + 3) & ~3

    return []


def extract_ui_pages(afs_path, output_dir, scale=1):
    """Extract all screen-font texture pages from scrscrn.ppg."""
    entries = read_afs(afs_path)
    ppg_data = read_afs_file(afs_path, entries[SCRSCRN_AFS_ENTRY])
    print(f"Loaded scrscrn.ppg: {len(ppg_data)} bytes from AFS entry {SCRSCRN_AFS_ENTRY}")

    # Parse all palettes from embedded pPAL chunks
    palettes = []
    pal_idx = 0
    while True:
        pal = parse_ppl_palette(ppg_data, pal_idx)
        if not pal:
            break
        palettes.append(pal)
        pal_idx += 1

    if palettes:
        print(f"  Loaded {len(palettes)} palettes.")
    else:
        print("  WARNING: No embedded palette found in PPG file")
        palettes.append([(128, 128, 128, 255)] * (64 * 512))

    def get_palette_for_page(page_index):
        if page_index == 5 and len(palettes) > 2:
            return palettes[2]
        if page_index == 6 and len(palettes) > 3:
            return palettes[3]
        return palettes[0]

    # Parse texture chunks
    tex_chunks = parse_ppg_header(ppg_data)
    print(f"  Found {len(tex_chunks)} texture page(s)")

    os.makedirs(output_dir, exist_ok=True)

    for i, (offset, size) in enumerate(tex_chunks):
        try:
            page_pal = get_palette_for_page(i)
            pixel_field = ppg_data[offset + 11]
            is_4bit = (pixel_field & 3) == 0

            if is_4bit:
                # 4-bit pages use 32 different 16-color banks. Extract all of them.
                bank_dir = os.path.join(output_dir, f"page_{i}_banks")
                os.makedirs(bank_dir, exist_ok=True)
                for bank in range(32):
                    page = decode_ppg_page(ppg_data, offset, size, page_pal, force_bank=bank)
                    if scale > 1:
                        new_size = (page.width * scale, page.height * scale)
                        page = page.resize(new_size, Image.NEAREST)
                    page.save(os.path.join(bank_dir, f"bank_{bank:02d}.png"))
                
                # Also save the default bank 0 at the root level
                page = decode_ppg_page(ppg_data, offset, size, page_pal, force_bank=0)
            else:
                page = decode_ppg_page(ppg_data, offset, size, page_pal)

            if scale > 1:
                new_size = (page.width * scale, page.height * scale)
                page = page.resize(new_size, Image.NEAREST)
            out_path = os.path.join(output_dir, f"page_{i}.png")
            page.save(out_path)
            print(f"  Page {i}: {page.size[0]}x{page.size[1]} -> {out_path} (plus all 32 banks)")
        except Exception as e:
            print(f"  WARNING: Failed to decode page {i}: {e}")

    # Also save a composite atlas for visual reference
    if tex_chunks:
        cols = min(len(tex_chunks), 4)
        rows = (len(tex_chunks) + cols - 1) // cols
        tile_w = 128 * scale
        tile_h = 128 * scale
        atlas = Image.new("RGBA", (cols * tile_w, rows * tile_h), (0, 0, 0, 0))
        for i, (offset, size) in enumerate(tex_chunks):
            try:
                page_pal = get_palette_for_page(i)
                page = decode_ppg_page(ppg_data, offset, size, page_pal, force_bank=0)
                if scale > 1:
                    page = page.resize((tile_w, tile_h), Image.NEAREST)
                atlas.paste(page, ((i % cols) * tile_w, (i // cols) * tile_h))
            except Exception:
                pass
        atlas_path = os.path.join(output_dir, "ui_atlas.png")
        atlas.save(atlas_path)
        print(f"  Atlas (reference): {atlas.size[0]}x{atlas.size[1]} -> {atlas_path}")


def _default_afs_path():
    return os.environ.get(
        "SF33RD_AFS",
        r"C:\Users\dov\AppData\Roaming\CrowdedStreet\3SX\resources\SF33RD.AFS"
    )


def main():
    parser = argparse.ArgumentParser(
        description="Extract screen-font (HUD) texture pages from scrscrn.ppg."
    )
    parser.add_argument(
        "--afs", default=_default_afs_path(),
        help="Path to SF33RD.AFS (default: SF33RD_AFS env var or built-in path)"
    )
    parser.add_argument(
        "--output", default="output/ui_pages",
        help="Output directory for page PNGs"
    )
    parser.add_argument(
        "--scale", type=int, default=1,
        help="Upscale factor (e.g. 4 for 4x nearest-neighbor)"
    )
    args = parser.parse_args()

    if not os.path.exists(args.afs):
        print(f"ERROR: AFS file not found: {args.afs}")
        sys.exit(1)

    extract_ui_pages(args.afs, args.output, args.scale)


if __name__ == "__main__":
    main()
