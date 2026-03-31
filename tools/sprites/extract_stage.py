#!/usr/bin/env python3
"""
Extract stage background layers from AFS archive.

Two extraction modes:
  --mode=ppg   (default) Raw PPG direct-color tiles (AFS entries 54-76)
  --mode=indexed         Indexed tiles + separate palette (AFS entries ~1383-1450)

Usage:
  python extract_stage.py all                  # extract all stages (ppg mode)
  python extract_stage.py 0                    # extract stage 0
  python extract_stage.py all --mode=indexed   # extract all with palettes
  python extract_stage.py 5 --mode=indexed     # extract stage 5 with palette
"""

import argparse
import json
import struct
import zlib
import os
import sys
from PIL import Image

from sprite_common import (
    read_afs, read_afs_file, lz_ext_p6_fx, DTL, unswizzle,
    decode_color_abgr1555, decode_palette_banks,
    STAGE_NAMES, STAGE_PAL_AFS, STAGE_TILE_AFS,
    COLORS_PER_BANK, PALETTE_BANK_BYTES,
)

# Optional NumPy for accelerated tile decoding
try:
    import numpy as np
    _HAS_NUMPY = True
except ImportError:
    _HAS_NUMPY = False

# STAGE_NAMES, STAGE_TILE_AFS, STAGE_PAL_AFS imported from sprite_common

# --- Mode 1: Raw PPG entries (direct-color 16-bit tiles) ---
STAGE_PPG_ENTRY = [
    54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
    64, 65, 66, 67, 68, 69, 70, 71, 72, 73,
    75, 76,
]

# Layers per stage (from use_real_scr)
STAGE_LAYERS = [2, 1, 3, 2, 1, 2, 1, 2, 2, 2, 2, 1, 2, 2, 3, 2, 2, 2, 2, 1, 2, 1]

# Animation (rewrite) tile count per stage (from rewrite_scr in bg_data.c)
# These tiles come AFTER the static tiles in the same PPG data
REWRITE_SCR = [0, 0, 0, 25, 0, 0, 0, 12, 24, 0, 96, 0, 0, 0, 1, 0, 0, 0, 10, 18, 0, 0]

# BGW number per stage per layer (from stage_bgw_number in bg_data.c)
# First non-zero index determines stg offset for GBIX base calculation.
STAGE_BGW_NUMBER = [
    [1, 2, 0], [0, 2, 0], [1, 2, 3], [1, 2, 0], [0, 2, 0], [1, 2, 0],
    [0, 2, 0], [1, 2, 0], [1, 2, 0], [1, 2, 0], [1, 2, 0], [0, 1, 0],
    [1, 2, 0], [1, 2, 0], [1, 2, 3], [1, 2, 0], [1, 2, 0], [1, 2, 0],
    [1, 2, 0], [0, 2, 0], [1, 2, 0], [0, 2, 0],
]

# Tile bitmasks per stage per layer (from bgtex_stage_gbix)
STAGE_GBIX = [
    [0xF0F0F0F0, 0x7F7FFFFF, 0x0],
    [0xFFFFFFFF, 0x0, 0x0],
    [0x3078FCFF, 0xFF, 0x7E7E7E7E],
    [0xFFFFFFFF, 0xC0E0F1FF, 0x0],
    [0xFFFFFFFF, 0x0, 0x0],
    [0x7E7E7E7E, 0x424FEFFF, 0x0],
    [0xFFFFFFFF, 0x0, 0x0],
    [0x7E00007E, 0xF0FFF2FF, 0x7E7E7E00],
    [0x3C3C3C3C, 0xFF, 0x0],
    [0x7F7F7F3F, 0x80F4FF, 0x0],
    [0xFFFFFFFF, 0xFFFFFFFF, 0x0],
    [0xFFFFFFFF, 0x0, 0x0],
    [0x7F7F7F3F, 0x98FFFFF, 0x0],
    [0xFFFFFF38, 0x3FFFFFFF, 0x0],
    [0xFFFFFFFF, 0x6FEFEFFF, 0x18181818],
    [0xFFFFFFFF, 0x80E6FFFF, 0x0],
    [0x7E7E0000, 0x77FFFFF, 0x0],
    [0x7E7E0000, 0x77FFFFF, 0x0],
    [0x7E7E7E7E, 0x424FEFFF, 0x0],
    [0xFFFFFFFF, 0x0, 0x0],
    [0x3C3C3C1C, 0x20343C3C, 0x0],
    [0x3E3E3E3E, 0x0, 0x0],
]

# bg_texture_type during gameplay (ramcnt type 0x12 = 18)
BG_TEXTURE_TYPE_GAMEPLAY = 0x12


def compute_stg_start(stage_idx):
    """Find first non-zero index in stage_bgw_number — mirrors Bg_Texture_Load_EX."""
    for stg in range(3):
        if STAGE_BGW_NUMBER[stage_idx][stg] != 0:
            return stg
    return 0


def compute_gbix_base(stage_idx, layer):
    """Compute the GBIX base for a given stage/layer: (stg + layer) * 64 + 0x84."""
    stg = compute_stg_start(stage_idx)
    return (stg + layer) * 64 + 0x84


# ─── Default AFS path ──────────────────────────────────────────


def _default_afs_path():
    return os.environ.get(
        "SF33RD_AFS",
        r"C:\Users\dov\AppData\Roaming\CrowdedStreet\3SX\resources\SF33RD.AFS"
    )


# ─── Palette loading ────────────────────────────────────────────


def load_palette(pal_data):
    """Load palette as flat RGBA tuple array from ABGR1555 little-endian data."""
    banks = decode_palette_banks(pal_data)
    colors = []
    for bank in banks:
        colors.extend(bank)
    return colors


# ─── Mode 1: Raw PPG direct-color tile extraction ──────────────


def _decode_ppg_tile_raw_numpy(raw, w, h):
    """Decode a raw ABGR1555 BE tile using NumPy (fast path)."""
    pixels = np.frombuffer(raw, dtype=np.dtype(">u2")).reshape(h, w)
    b = ((pixels & 0x1F) << 3).astype(np.uint8)
    g = (((pixels >> 5) & 0x1F) << 3).astype(np.uint8)
    r = (((pixels >> 10) & 0x1F) << 3).astype(np.uint8)
    a = np.where(pixels & 0x8000, 255, 0).astype(np.uint8)
    # Magenta colorkey (r5=31, g5=0|6, b5=31 -> r>=248, g=0|48, b>=248 with << 3)
    mg_mask = (r >= 248) & (b >= 248) & ((g == 0) | (g == 48))
    a[mg_mask] = 0
    # Zero out RGB where alpha is 0 to avoid ghosting
    trans_mask = (a == 0)
    r[trans_mask] = 0
    g[trans_mask] = 0
    b[trans_mask] = 0

    rgba = np.stack([r, g, b, a], axis=-1)
    return Image.fromarray(rgba, "RGBA")


def _decode_ppg_tile_raw_python(raw, w, h):
    """Decode a raw ABGR1555 BE tile using pure Python (fallback)."""
    buf = bytearray(w * h * 4)
    for py in range(h):
        for px in range(w):
            val = struct.unpack_from(">H", raw, (py * w + px) * 2)[0]
            b = (val & 0x1F) << 3
            g = ((val >> 5) & 0x1F) << 3
            r = ((val >> 10) & 0x1F) << 3
            a = 255 if (val & 0x8000) else 0
            if r >= 248 and b >= 248 and (g == 0 or g == 48):
                r = g = b = a = 0
            elif a == 0:
                r = g = b = 0
            off = (py * w + px) * 4
            buf[off] = r
            buf[off + 1] = g
            buf[off + 2] = b
            buf[off + 3] = a
    return Image.frombytes("RGBA", (w, h), bytes(buf))


def extract_ppg_tiles_raw(afs_path, entries, entry_idx):
    """Extract 128x128 tiles as direct ABGR1555 big-endian color."""
    data = read_afs_file(afs_path, entries[entry_idx])
    tiles = []
    ofs = 0
    while ofs < len(data) - 8:
        magic = data[ofs : ofs + 4]
        if magic == b"pEND":
            break
        file_size = struct.unpack_from(">I", data, ofs + 4)[0]

        if magic == b"pTEX":
            comp = data[ofs + 16 : ofs + file_size]
            try:
                raw = zlib.decompress(comp)
                w, h = 128, 128
                if _HAS_NUMPY:
                    tiles.append(_decode_ppg_tile_raw_numpy(raw, w, h))
                else:
                    tiles.append(_decode_ppg_tile_raw_python(raw, w, h))
            except Exception as e:
                print(f"    WARNING: failed to decode PPG tile at offset {ofs}: {e}")
                tiles.append(Image.new("RGBA", (128, 128), (255, 0, 255, 128)))

        ofs += (file_size + 3) & ~3
    return tiles


# ─── Mode 2: Indexed tile extraction with palette ──────────────


def _decode_ppg_tile_indexed_numpy(raw, w, h, pal_map, palette_colors, default_pal):
    """Decode an indexed PPG tile using NumPy (fast path)."""
    indices = np.frombuffer(raw[:w * h], dtype=np.uint8).reshape(h, w)

    # Build per-pixel palette bank as numpy array
    pal_map_arr = np.array(pal_map, dtype=np.int32)

    # Compute flat color indices
    color_idx = pal_map_arr * COLORS_PER_BANK + indices.astype(np.int32)

    # Build full palette as RGBA array
    max_idx = max(color_idx.max() + 1, len(palette_colors))
    pal_arr = np.zeros((max_idx, 4), dtype=np.uint8)
    for ci, c in enumerate(palette_colors):
        if ci < max_idx:
            pal_arr[ci] = c

    # Apply: index 0 = transparent
    rgba = np.where(
        indices[..., np.newaxis] == 0,
        np.array([0, 0, 0, 0], dtype=np.uint8),
        pal_arr[np.clip(color_idx, 0, len(pal_arr) - 1)]
    )
    return Image.fromarray(rgba.astype(np.uint8), "RGBA")


def extract_ppg_tiles_indexed(data, palette_colors):
    """Extract 128x128 tiles using indexed pixels + per-region palette banks."""
    tiles = []
    ofs = 0
    while ofs < len(data) - 8:
        magic = data[ofs : ofs + 4]
        if magic == b"pEND":
            break
        if magic != b"pTEX":
            break
        file_size = struct.unpack_from(">I", data, ofs + 4)[0]
        wb = data[ofs + 8]
        # hb = data[ofs + 9]
        trans_nums = struct.unpack_from(">H", data, ofs + 14)[0]

        # Parse trans entries: per-region palette bank mapping
        trans = []
        for t in range(trans_nums):
            t_ofs = ofs + 16 + t * 3
            pal_bank = data[t_ofs]
            i_point = data[t_ofs + 1]
            cofs_xy = data[t_ofs + 2]
            xs = (cofs_xy >> 4) + 1
            ys = (cofs_xy & 0xF) + 1
            sx = i_point % wb
            sy = i_point // wb
            trans.append((pal_bank, sx, sy, xs, ys))

        comp_start = ofs + 16 + trans_nums * 3
        comp = data[comp_start : ofs + file_size]

        w, h = 128, 128
        try:
            raw = zlib.decompress(comp)

            # Build per-pixel palette bank map from trans entries
            default_pal = trans[0][0] if trans else 0
            pal_map = [[default_pal] * w for _ in range(h)]
            for pal_bank, sx, sy, xs, ys in trans:
                px_x, px_y = sx * 16, sy * 16
                px_w, px_h = xs * 16, ys * 16
                for py in range(px_y, min(px_y + px_h, h)):
                    for px in range(px_x, min(px_x + px_w, w)):
                        pal_map[py][px] = pal_bank

            if _HAS_NUMPY:
                tiles.append(_decode_ppg_tile_indexed_numpy(
                    raw, w, h, pal_map, palette_colors, default_pal))
            else:
                buf = bytearray(w * h * 4)
                for py in range(h):
                    for px in range(w):
                        if py * w + px < len(raw):
                            idx = raw[py * w + px]
                            if idx == 0:
                                pass  # already zeroed
                            else:
                                color_idx = pal_map[py][px] * COLORS_PER_BANK + idx
                                off = (py * w + px) * 4
                                if color_idx < len(palette_colors):
                                    r, g, b, a = palette_colors[color_idx]
                                    buf[off] = r
                                    buf[off + 1] = g
                                    buf[off + 2] = b
                                    buf[off + 3] = a
                                else:
                                    buf[off:off + 4] = b'\x80\x80\x80\xff'
                tiles.append(Image.frombytes("RGBA", (w, h), bytes(buf)))
        except Exception as e:
            print(f"    WARNING: failed to decode indexed tile at offset {ofs}: {e}")
            tiles.append(Image.new("RGBA", (128, 128), (255, 0, 255, 128)))
        ofs += (file_size + 3) & ~3

    return tiles


# ─── Layer compositing ─────────────────────────────────────────


def composite_layers(tiles, stage_idx, num_layers, stage_dir):
    """Composite tiles into full-grid layer images with GBIX metadata JSON."""
    tile_ptr = 0
    for layer in range(num_layers):
        mask = STAGE_GBIX[stage_idx][layer]
        gbix_base = compute_gbix_base(stage_idx, layer)
        layer_img = Image.new("RGBA", (8 * 128, 4 * 128), (0, 0, 0, 0))
        tile_entries = []
        count = 0
        for row in range(4):
            byte_val = (mask >> (24 - row * 8)) & 0xFF
            for col in range(8):
                bit_index = row * 8 + col
                gbix = gbix_base + bit_index
                if byte_val & (0x80 >> col):
                    if tile_ptr < len(tiles):
                        layer_img.paste(tiles[tile_ptr], (col * 128, row * 128))
                        composite_key = BG_TEXTURE_TYPE_GAMEPLAY * 100000 + stage_idx * 1000 + gbix
                        tile_entries.append({
                            "row": row,
                            "col": col,
                            "gbix": gbix,
                            "composite_key": composite_key,
                        })
                        tile_ptr += 1
                        count += 1

        # Save full-grid layer (no crop — preserves tile positions for retiling)
        layer_img.save(os.path.join(stage_dir, f"layer_{layer}.png"))

        # Save metadata JSON for retiling
        metadata = {
            "stage_idx": stage_idx,
            "stage_name": STAGE_NAMES[stage_idx],
            "layer": layer,
            "bg_texture_type": BG_TEXTURE_TYPE_GAMEPLAY,
            "gbix_base": gbix_base,
            "grid_width": 8,
            "grid_height": 4,
            "tile_size": 128,
            "tile_count": count,
            "tiles": tile_entries,
        }
        json_path = os.path.join(stage_dir, f"layer_{layer}.json")
        with open(json_path, "w") as f:
            json.dump(metadata, f, indent=2)

        print(f"  Layer {layer}: {count} tiles, {len(tile_entries)} gbix entries -> {layer_img.size}")


# ─── Stage extraction (both modes) ─────────────────────────────


def extract_stage(afs_path, stage_idx, output_dir, mode="ppg"):
    """Extract all layers of a stage."""
    entries = read_afs(afs_path)
    name = STAGE_NAMES[stage_idx]
    num_layers = STAGE_LAYERS[stage_idx]

    suffix = "_indexed" if mode == "indexed" else "_ppg"
    stage_dir = os.path.join(output_dir, f"stage_{stage_idx:02d}_{name}{suffix}")
    tiles_dir = os.path.join(stage_dir, "tiles")
    os.makedirs(tiles_dir, exist_ok=True)

    if mode == "indexed":
        tile_afs = STAGE_TILE_AFS[stage_idx]
        pal_afs = STAGE_PAL_AFS[stage_idx]
        print(
            f"Stage {stage_idx}: {name} [INDEXED] (tiles AFS={tile_afs}, pal AFS={pal_afs}, {num_layers} layers)"
        )

        # Load palette
        pal_data = read_afs_file(afs_path, entries[pal_afs])
        palette_colors = load_palette(pal_data)
        print(
            f"  Palette: {len(palette_colors)} colors ({len(palette_colors) // COLORS_PER_BANK} banks)"
        )

        # Load and extract tiles with palette
        tile_data = read_afs_file(afs_path, entries[tile_afs])
        tiles = extract_ppg_tiles_indexed(tile_data, palette_colors)
    else:
        ppg_entry = STAGE_PPG_ENTRY[stage_idx]
        print(
            f"Stage {stage_idx}: {name} [PPG RAW] (AFS entry {ppg_entry}, {num_layers} layers)"
        )
        tiles = extract_ppg_tiles_raw(afs_path, entries, ppg_entry)

    print(f"  Extracted {len(tiles)} tiles")

    # Save individual tiles
    for i, tile in enumerate(tiles):
        tile.save(os.path.join(tiles_dir, f"tile_{i:03d}.png"))

    # Save tile atlas
    cols = 8
    rows = (len(tiles) + cols - 1) // cols
    atlas = Image.new("RGBA", (cols * 128, rows * 128), (0, 0, 0, 255))
    for i, t in enumerate(tiles):
        atlas.paste(t, ((i % cols) * 128, (i // cols) * 128))
    atlas.save(os.path.join(stage_dir, "tile_atlas.png"))
    print(f"  Saved tile atlas ({cols * 128}x{rows * 128})")

    # Count static tiles (from GBIX bitmasks)
    static_count = 0
    for layer in range(num_layers):
        mask = STAGE_GBIX[stage_idx][layer]
        for row in range(4):
            byte_val = (mask >> (24 - row * 8)) & 0xFF
            for col in range(8):
                if byte_val & (0x80 >> col):
                    static_count += 1

    anim_count = REWRITE_SCR[stage_idx]
    anim_start = static_count

    # Composite layers (static tiles only)
    composite_layers(
        tiles[:static_count] if static_count > 0 else tiles,
        stage_idx,
        num_layers,
        stage_dir,
    )

    # Save animation tiles separately
    if anim_count > 0 and anim_start < len(tiles):
        anim_tiles = tiles[anim_start : anim_start + anim_count]
        anim_dir = os.path.join(stage_dir, "anim_tiles")
        os.makedirs(anim_dir, exist_ok=True)
        for i, tile in enumerate(anim_tiles):
            tile.save(os.path.join(anim_dir, f"anim_{i:03d}.png"))

        # Save animation atlas
        cols = 8
        rows = (len(anim_tiles) + cols - 1) // cols
        if rows > 0:
            anim_atlas = Image.new("RGBA", (cols * 128, rows * 128), (0, 0, 0, 255))
            for i, t in enumerate(anim_tiles):
                anim_atlas.paste(t, ((i % cols) * 128, (i // cols) * 128))
            anim_atlas.save(os.path.join(stage_dir, "anim_atlas.png"))

        print(
            f"  Animation: {len(anim_tiles)}/{anim_count} tiles (starting at tile {anim_start})"
        )
    elif anim_count > 0:
        print(
            f"  Animation: expected {anim_count} tiles but only {len(tiles)} total extracted"
        )

    # Extract chip-based sprite animations (flames, rain, crowd, etc.)
    extract_chip_anims(afs_path, entries, stage_idx, stage_dir)

    print(f"  Output: {stage_dir}/")


# ─── Chip-based sprite animation extraction ────────────────────
# Uses the shared tile parser and compositor from sprite_compositor.py
# (lazy-imported to avoid circular/coupling issues).


def extract_chip_anims(afs_path, entries, stage_idx, stage_dir):
    """Extract chip-based sprite animations from gap AFS entries.
    Only extracts Type A files (self-contained with embedded LZ textures).
    Type B files (non-zero attr, no embedded textures) are skipped.
    """
    from sprite_compositor import extract_stage_frame, build_stage_colorram
    pal_afs = STAGE_PAL_AFS[stage_idx]
    tile_afs = STAGE_TILE_AFS[stage_idx]
    lo = min(pal_afs, tile_afs)
    hi = max(pal_afs, tile_afs)
    gap_entries = list(range(lo + 1, hi))

    if not gap_entries:
        return

    # Build full 512-bank ColorRAM (common + stage palettes)
    colorram = build_stage_colorram(afs_path, entries, pal_afs, apply_clut=False)

    # Filter gap entries to valid chip data
    chip_candidates = []
    for idx in gap_entries:
        off, size = entries[idx]
        if size < 256:
            continue
        header = read_afs_file(afs_path, entries[idx])[:4]
        if header == b"pTEX" or header == b"pEND":
            continue
        chip_candidates.append(idx)

    if not chip_candidates:
        return

    total_frames = 0
    type_a_count = 0

    for ci, afs_idx in enumerate(chip_candidates):
        data = read_afs_file(afs_path, entries[afs_idx])
        first_u32 = struct.unpack_from("<I", data, 0)[0]
        if first_u32 == 0 or first_u32 >= len(data) or first_u32 % 4 != 0:
            continue

        num_frames = first_u32 // 4

        # Check if Type A (all attrs == 0 in first frame)
        foff = struct.unpack_from("<I", data, 0)[0]
        if foff + 2 > len(data):
            continue
        cnt = struct.unpack_from("<H", data, foff)[0]
        if cnt == 0:
            continue

        is_type_a = True
        for ti in range(cnt):
            eoff = foff + 2 + ti * 8
            if eoff + 8 > len(data):
                break
            _, _, attr, _ = struct.unpack_from("<hhHH", data, eoff)
            if attr != 0:
                is_type_a = False
                break

        if not is_type_a:
            continue  # Skip Type B files

        # Find to_tex offset
        max_frame_end = first_u32
        for fi in range(num_frames):
            fo = struct.unpack_from("<I", data, fi * 4)[0]
            if fo + 2 > len(data):
                continue
            c = struct.unpack_from("<H", data, fo)[0]
            frame_end = fo + 2 + c * 8
            if frame_end > max_frame_end:
                max_frame_end = frame_end

        to_tex = max_frame_end

        # Verify texture data is valid (first code should produce valid wh)
        foff = struct.unpack_from("<I", data, 0)[0]
        cnt = struct.unpack_from("<H", data, foff)[0]
        _, _, _, code = struct.unpack_from("<hhHH", data, foff + 2)
        tpos = to_tex + code * 4
        if tpos + 4 > len(data):
            continue
        toff_val = struct.unpack_from("<I", data, tpos)[0]
        aoff = to_tex + toff_val
        if aoff >= len(data):
            continue

        chip_dir = os.path.join(stage_dir, f"chip_anims_{type_a_count}")
        os.makedirs(chip_dir, exist_ok=True)

        extracted = 0
        for fi in range(num_frames):
            img = extract_stage_frame(data, to_tex, fi, colorram,
                                      colcd_base=300, rendering_mode=33)
            if img is not None:
                img.save(os.path.join(chip_dir, f"frame_{fi:04d}.png"))
                extracted += 1

        if extracted > 0:
            total_frames += extracted
            print(
                f"  Chip anims [{type_a_count}] (AFS {afs_idx}): "
                f"{extracted}/{num_frames} frames"
            )
            type_a_count += 1

    if total_frames > 0:
        print(f"  Total chip animation frames: {total_frames}")


def main():
    parser = argparse.ArgumentParser(
        description="Extract stage background layers from AFS archive."
    )
    parser.add_argument(
        "stage", nargs="?", default=None,
        help="Stage index (0-21) or 'all' to extract all stages"
    )
    parser.add_argument(
        "--mode", choices=["ppg", "indexed"], default="ppg",
        help="Extraction mode: ppg (raw direct-color) or indexed (palette-based)"
    )
    parser.add_argument(
        "--output", default="output/stages",
        help="Output directory (default: output/stages)"
    )
    parser.add_argument(
        "--afs", default=_default_afs_path(),
        help="Path to SF33RD.AFS (default: SF33RD_AFS env var or built-in path)"
    )
    args = parser.parse_args()

    if args.stage is None:
        parser.print_help()
        sys.exit(1)

    afs_path = args.afs
    output_dir = args.output

    if args.stage == "all":
        os.makedirs(output_dir, exist_ok=True)
        for i in range(len(STAGE_NAMES)):
            extract_stage(afs_path, i, output_dir, args.mode)
            print()
    else:
        try:
            stage_idx = int(args.stage)
        except ValueError:
            print(f"ERROR: '{args.stage}' is not a valid stage index or 'all'")
            sys.exit(1)
        if stage_idx < 0 or stage_idx >= len(STAGE_NAMES):
            print(f"ERROR: stage index {stage_idx} out of range (0-{len(STAGE_NAMES)-1})")
            sys.exit(1)
        os.makedirs(output_dir, exist_ok=True)
        extract_stage(afs_path, stage_idx, output_dir, args.mode)


if __name__ == "__main__":
    main()
