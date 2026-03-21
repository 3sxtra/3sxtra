#!/usr/bin/env python3
"""
Extract stage sprite animations from AFS archive using texgrpdat metadata.

These are the animated stage elements (flames, crowds, debris, etc.) identified
at runtime as sprite_{group}_{cg}.png. This tool extracts them offline using
the same trans_table + texture_table format the engine uses.

Uses the actual texgrpdat table from texgroup.c for accurate CG ranges,
AFS entries, and texture table offsets.

Usage:
  python extract_stage_sprites.py all                    # extract all stage sprite groups
  python extract_stage_sprites.py --group 52             # extract group 52 (Gill stage)
  python extract_stage_sprites.py --stage 0              # all groups for stage 0
  python extract_stage_sprites.py --list                 # list all known groups
"""

import struct
import zlib
import os
import sys
import json
from PIL import Image


# ── texgrpdat table (from texgroup.c) ──────────────────────────────────────────
# Each entry: (group_index, num_of_1st, apfn, to_tex)
# Only non-character, non-empty groups with valid apfn (>0) are included.

TEXGRPDAT = [
    # idx   num_of_1st  apfn    to_tex   description
    (20,    27040,      1452,   436,     "System/common"),
    (22,    27104,      1454,   134308,  "System/select object JP"),
    (24,    29152,      1455,   14180,   "System/intro"),
    (25,    29344,      1456,   92368,   "System/endings"),
    (26,    30640,      1461,   5788,    "System/super flash"),
    (29,    30896,      1457,   3448,    "System/continue"),
    (32,    31152,      1446,   120700,  "Bonus/car smash"),
    (33,    32432,      1444,   2580,    "Bonus/misc"),
    (34,    36896,      1462,   4212,    "System/training HUD"),
    (37,    32560,      1458,   25088,   "System/win/lose"),
    (42,    34352,      1401,   1704,    "Stage 05 (Necro/Russia) sprites"),
    (43,    34384,      1410,   2180,    "Stage 08 (Elena/Kenya) sprites"),
    (44,    34448,      1389,   19772,   "Stage 01 (Alex/NY) sprites"),
    (45,    34576,      1395,   3256,    "Stage 03 (Yun/HK) sprites"),
    (46,    34672,      1428,   1672,    "Stage 14 (Akuma/Japan) sprites"),
    (47,    34704,      1405,   1832,    "Stage 06 (Hugo/Germany) sprites"),
    (48,    34736,      1413,   2220,    "Stage 09 (Oro/Brazil) sprites"),
    (49,    34832,      1425,   296,     "Stage 13 (Urien/Egypt) sprites"),
    (50,    34864,      1398,   12208,   "Stage 04 (Dudley/England) sprites"),
    (51,    34960,      1434,   4016,    "Stage 16 (Makoto/Japan) sprites"),
    (52,    35024,      1386,   4568,    "Stage 00 (Gill/Boss) sprites"),
    (53,    35120,      1407,   1100,    "Stage 07 (Ibuki/Japan) sprites"),
    (54,    35152,      1443,   64,      "Stage 20 (Bonus car) sprites"),
    (55,    35184,      1440,   7372,    "Stage 19 (Remy) sprites"),
    (56,    35328,      1431,   17548,   "Stage 15 (Chun-Li/China) sprites"),
    (58,    35648,      1392,   2100,    "Stage 02 (Ryu/Japan) sprites"),
    (59,    35744,      1448,   8496,    "Stage 21 (Bonus parry) sprites"),
    (61,    35904,      74,     54492,   "Effect/common (PPG 74)"),
    (62,    36096,      34,     6552,    "Effect/flames"),
    (63,    36160,      35,     1540,    "Effect/sparks"),
    (64,    36192,      36,     11480,   "Effect/hit FX A"),
    (65,    36288,      37,     2300,    "Effect/hit FX B"),
    (66,    36320,      38,     1712,    "Effect/dust"),
    (67,    36352,      39,     6460,    "Effect/shadow"),
    (68,    36384,      40,     2332,    "Effect/projectiles A"),
    (69,    36416,      41,     3412,    "Effect/projectiles B"),
    (70,    36448,      42,     444,     "Effect/guard"),
    (71,    36480,      43,     1072,    "Effect/block"),
    (72,    36512,      44,     2832,    "Effect/super A"),
    (73,    36544,      45,     4676,    "Effect/super B"),
    (74,    36576,      46,     5992,    "Effect/super C"),
    (75,    36608,      47,     8904,    "Effect/EX moves A"),
    (76,    36640,      48,     14508,   "Effect/EX moves B"),
    (77,    36704,      49,     1536,    "Effect/misc A"),
    (78,    36736,      50,     2968,    "Effect/misc B"),
    (79,    36768,      51,     6628,    "Effect/misc C"),
    (80,    36800,      52,     2868,    "Effect/misc D"),
    (81,    36864,      53,     4776,    "Effect/misc E"),
    (82,    37024,      1459,   42292,   "System/menu objects"),
    (89,    37408,      1384,   2488,    "Stage 00 (Gill/Boss) chip A"),
    (90,    37536,      1385,   12984,   "Stage 00 (Gill/Boss) chip B"),
    (91,    34576,      1416,   3256,    "Stage 10 (Yang/HK) sprites"),
    (92,    34448,      1419,   19772,   "Stage 11 (Ken/NY) sprites"),
    (93,    34736,      1422,   2220,    "Stage 12 (Sean/Brazil) sprites"),
    (94,    34352,      1437,   1704,    "Stage 18 (Twelve) sprites"),
    (98,    27104,      1453,   134044,  "System/select object EN"),
]

# Stage palette AFS entries (from extract_stage.py / bg_data.c)
STAGE_PAL_AFS = {
    0: 1383, 1: 1388, 2: 1391, 3: 1394, 4: 1397, 5: 1400, 6: 1403,
    7: 1406, 8: 1409, 9: 1412, 10: 1415, 11: 1418, 12: 1421, 13: 1424,
    14: 1427, 15: 1430, 16: 1433, 17: 1418, 18: 1436, 19: 1439, 20: 1442,
    21: 1447,
}

STAGE_TILE_AFS = {
    0: 1387, 1: 1390, 2: 1393, 3: 1396, 4: 1399, 5: 1402, 6: 1404,
    7: 1408, 8: 1411, 9: 1414, 10: 1417, 11: 1420, 12: 1423, 13: 1426,
    14: 1429, 15: 1432, 16: 1435, 17: 1420, 18: 1438, 19: 1441, 20: 1445,
    21: 1450,
}

# CPS3 palette system: two palette sources merge into ColorRAM[512][64]:
#   1. Common palette (AFS 9): 480 banks → ColorRAM[32..511]
#   2. Stage palette (per-stage AFS): 68 banks → ColorRAM[300..367] (overwrites)
# Stage sprite palette base bank in merged ColorRAM.
# Final palette for each tile = ColorRAM[SPRITE_PAL_BASE + palo].
SPRITE_PAL_BASE = 300

# Directory containing runtime ColorRAM dump files (stage_XX_colorram.bin).
# These 65536-byte files contain the definitive 512-bank palette after palConvSrcToRam.
# Generate them by running the game with the debug dump enabled in bg_load.c.
COLORRAM_DUMP_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # project root

# AFS entry for the common palette (loaded into ColorRAM[32..511]).
COMMON_PAL_AFS = 9
# ColorRAM slot where the common palette starts.
COMMON_PAL_SLOT = 32
# ColorRAM slot where the stage palette starts.
STAGE_PAL_SLOT = 300


def afs_entry_to_stage(apfn):
    """Map an AFS entry number to a stage index by finding which stage's range it falls in."""
    for stage_idx in range(22):
        pal = STAGE_PAL_AFS[stage_idx]
        tile = STAGE_TILE_AFS[stage_idx]
        lo, hi = min(pal, tile), max(pal, tile)
        if lo <= apfn <= hi:
            return stage_idx
    return None


# ── AFS helpers ────────────────────────────────────────────────────────────────


def read_afs(filepath):
    with open(filepath, "rb") as f:
        f.read(4)
        ec = struct.unpack("<I", f.read(4))[0]
        entries = []
        for _ in range(ec):
            o, s = struct.unpack("<II", f.read(8))
            entries.append((o, s))
    return entries


def read_afs_file(filepath, entry):
    with open(filepath, "rb") as f:
        f.seek(entry[0])
        return f.read(entry[1])


# ── CPS3 LZ decompressor ──────────────────────────────────────────────────────


def lz_ext_p6_fx(src, dst_size):
    """CPS3 LZ decompressor for sprite chip data."""
    dst = bytearray(dst_size)
    si = di = 0
    while di < dst_size and si < len(src):
        t = src[si]
        si += 1
        c = t & 0xC0
        if c == 0x00:
            dst[di] = t
            di += 1
        elif c == 0x40:
            t &= 0x3F
            back = (t >> 2) + 1
            length = (t & 3) + 2
            p = di - back
            for _ in range(length):
                if di >= dst_size:
                    break
                dst[di] = dst[p] if 0 <= p < di else 0
                di += 1
                p += 1
        elif c == 0x80:
            if si >= len(src):
                break
            t = ((t & 0x3F) << 8) | src[si]
            si += 1
            back = (t >> 6) + 1
            length = (t & 0x3F) + 2
            p = di - back
            for _ in range(length):
                if di >= dst_size:
                    break
                dst[di] = dst[p] if 0 <= p < di else 0
                di += 1
                p += 1
        else:
            fg = t & 0x30
            length = (t & 0x0F) + 2
            for _ in range(length):
                if di >= dst_size or si >= len(src):
                    break
                dst[di] = fg | (src[si] >> 4)
                di += 1
                if di >= dst_size:
                    break
                dst[di] = fg | (src[si] & 0x0F)
                di += 1
                si += 1
    return bytes(dst)


# ── Unswizzle table ────────────────────────────────────────────────────────────

_seed = [
    0x0000, 0x0002, 0x0008, 0x000A, 0x0020, 0x0022, 0x0028, 0x002A,
    0x0080, 0x0082, 0x0088, 0x008A, 0x00A0, 0x00A2, 0x00A8, 0x00AA,
    0x0200, 0x0202, 0x0208, 0x020A, 0x0220, 0x0222, 0x0228, 0x022A,
    0x0280, 0x0282, 0x0288, 0x028A, 0x02A0, 0x02A2, 0x02A8, 0x02AA,
]
_seedAdd = [
    0x0000, 0x0004, 0x0010, 0x0014, 0x0040, 0x0044, 0x0050, 0x0054,
    0x0100, 0x0104, 0x0110, 0x0114, 0x0140, 0x0144, 0x0150, 0x0154,
]
DTL = [0] * 1024
for _i in range(16):
    for _j in range(32):
        DTL[_j + _i * 64] = _seed[_j] + _seedAdd[_i]
    for _j in range(32):
        DTL[_j + _i * 64 + 32] = DTL[_j + _i * 64] + 1


def unswizzle(pdata, td):
    result = bytearray(td * td)
    if td <= 16:
        for y in range(td):
            for x in range(td):
                si = DTL[x + (y << 5)]
                if si < len(pdata):
                    result[y * td + x] = pdata[si]
    else:
        for y in range(td):
            for x in range(td):
                si = DTL[y * 32 + x]
                if si < len(pdata):
                    result[y * td + x] = pdata[si]
    return bytes(result)


# ── Palette loading ────────────────────────────────────────────────────────────


def decode_abgr1555(pal_data):
    """Decode raw ABGR1555 LE palette data into a list of 64-color banks.
    Each bank is a list of 64 RGBA tuples."""
    n_banks = len(pal_data) // 128  # 64 colors × 2 bytes each
    banks = []
    for bank in range(n_banks):
        colors = [(0, 0, 0, 0)] * 64
        for i in range(64):
            off = bank * 128 + i * 2
            if off + 2 > len(pal_data):
                break
            val = struct.unpack_from("<H", pal_data, off)[0]
            # Source format ABGR1555: bits 0-4=B, 5-9=G, 10-14=R, 15=A
            b5 = val & 0x1F
            g5 = (val >> 5) & 0x1F
            r5 = (val >> 10) & 0x1F
            a = 255 if (val & 0x8000) else 0
            r = (r5 * 255 + 15) // 31
            g = (g5 * 255 + 15) // 31
            b = (b5 * 255 + 15) // 31
            colors[i] = (r, g, b, a)
        banks.append(colors)
    return banks


def load_colorram_dump(stage_idx):
    """Load the runtime ColorRAM dump for a given stage.

    The dump file is 65536 bytes: 512 banks × 64 colors × 2 bytes.
    Colors are stored in post-palConvSrcToRam format:
      bits 0-4=R, 5-9=G, 10-14=B, 15=A (R and B swapped from source).

    Returns list of 512 banks (each a list of 64 RGBA tuples), or None."""
    dump_path = os.path.join(COLORRAM_DUMP_DIR,
                             f"stage_{stage_idx:02d}_colorram.bin")
    if not os.path.exists(dump_path):
        return None

    with open(dump_path, "rb") as f:
        cram = f.read()

    if len(cram) != 65536:
        return None

    banks = []
    for bank in range(512):
        colors = [(0, 0, 0, 0)] * 64
        for i in range(64):
            off = bank * 128 + i * 2
            val = struct.unpack_from("<H", cram, off)[0]
            # Post-swap format: R@0-4, G@5-9, B@10-14, A@15
            r5 = val & 0x1F
            g5 = (val >> 5) & 0x1F
            b5 = (val >> 10) & 0x1F
            a = 255 if (val & 0x8000) else 0
            r = (r5 * 255 + 15) // 31
            g = (g5 * 255 + 15) // 31
            b = (b5 * 255 + 15) // 31
            colors[i] = (r, g, b, a)
        banks.append(colors)
    return banks


def build_colorram(afs_path, entries, stage_pal_afs):
    """Build merged ColorRAM[512] palette banks from common + stage palette.

    The CPS3 palette RAM has 512 banks of 64 colors each.
    Two sources populate it:
      - Common palette (AFS 9): 480 banks → slots 32..511
      - Stage palette: 68 banks → slots 300..367 (overwrites common)

    Returns list of 512 banks, each a list of 64 RGBA tuples."""
    # Initialize 512 empty banks
    colorram = [[(0, 0, 0, 0)] * 64 for _ in range(512)]

    # Load common palette
    if COMMON_PAL_AFS < len(entries):
        common_data = read_afs_file(afs_path, entries[COMMON_PAL_AFS])
        common_banks = decode_abgr1555(common_data)
        for i, bank in enumerate(common_banks):
            slot = COMMON_PAL_SLOT + i
            if slot < 512:
                colorram[slot] = bank

    # Overlay stage-specific palette (overwrites common in range 300..367)
    if stage_pal_afs is not None and stage_pal_afs < len(entries):
        stage_data = read_afs_file(afs_path, entries[stage_pal_afs])
        stage_banks = decode_abgr1555(stage_data)
        for i, bank in enumerate(stage_banks):
            slot = STAGE_PAL_SLOT + i
            if slot < 512:
                colorram[slot] = bank

    return colorram


# ── Frame extraction ───────────────────────────────────────────────────────────


def extract_sprite_frame(data, to_tex, frame_idx, pal_banks, colcd_base=None):
    """Extract a single sprite frame (CG) from a texgrpdat-format binary.

    Args:
        data: full AFS entry data bytes
        to_tex: offset to texture table within data
        frame_idx: 0-based frame index within this group
        pal_banks: list of palette banks, each a list of 64 RGBA tuples
        colcd_base: palette base offset in pal_banks (overrides SPRITE_PAL_BASE)

    Returns:
        PIL Image or None
    """
    # Read frame count from first u32
    if len(data) < 4:
        return None
    num_frames = struct.unpack_from("<I", data, 0)[0] // 4
    if frame_idx >= num_frames:
        return None

    foff = struct.unpack_from("<I", data, frame_idx * 4)[0]
    if foff + 2 > len(data):
        return None
    cnt = struct.unpack_from("<H", data, foff)[0]
    if cnt == 0:
        return None

    # Parse chip tiles
    tiles = []
    eoff = foff + 2
    for _ in range(cnt):
        if eoff + 8 > len(data):
            break
        dx, dy = struct.unpack_from("<hh", data, eoff)
        attr, code = struct.unpack_from("<HH", data, eoff + 4)
        eoff += 8

        tpos = to_tex + code * 4
        if tpos + 4 > len(data):
            continue
        toff = struct.unpack_from("<I", data, tpos)[0]
        aoff = to_tex + toff
        if aoff >= len(data):
            continue

        wh = data[aoff]
        wm = (wh & 3) + 1
        td = wm * 8
        ts = (wm * wm) << 6
        dw = (wh & 0xE0) >> 2
        dh = (wh & 0x1C) * 2
        if dw == 0 or dh == 0:
            continue

        raw = lz_ext_p6_fx(data[aoff + 1:], ts)
        px = unswizzle(raw, td)
        tiles.append((dx, dy, attr, code, td, dw, dh, px))

    if not tiles:
        return None

    # Compute positions (handle flip)
    all_xflip = all((t[2] & 0x8000) != 0 for t in tiles)
    cx = cy = 0.0
    pts = []
    for dx, dy, attr, code, td, dw, dh, px in tiles:
        if all_xflip:
            cx += dx
        else:
            cx -= dx
        cy -= dy
        if all_xflip:
            draw_x = int(cx) - (dw if (attr & 0x8000) else 0)
            draw_y = int(cy) + (dh if (attr & 0x4000) else 0)
        else:
            draw_x = int(cx)
            draw_y = int(cy)
        pts.append((draw_x, draw_y))

    # Bounding box
    mnx = min(p[0] for p in pts)
    mny = min(p[1] for p in pts)
    mxx = max(pts[i][0] + tiles[i][5] for i in range(len(tiles)))
    mxy = max(pts[i][1] + tiles[i][6] for i in range(len(tiles)))
    sw = mxx - mnx
    sh = mxy - mny
    if sw <= 0 or sh <= 0:
        return None

    # Composite with proper alpha handling
    img = Image.new("RGBA", (sw, sh), (0, 0, 0, 0))
    pxi = img.load()
    for i, (dx, dy, attr, code, td, dw, dh, pdata) in enumerate(tiles):
        tx = pts[i][0] - mnx
        ty = pts[i][1] - mny
        flip_x = (attr & 0x8000) != 0 and not all_xflip
        flip_y = (attr & 0x4000) != 0 and not all_xflip
        # Select palette bank: tile palo + colcd base offset
        base = colcd_base if colcd_base is not None else SPRITE_PAL_BASE
        tile_pal_idx = (attr & 0xF) + base
        pal = pal_banks[tile_pal_idx] if tile_pal_idx < len(pal_banks) else pal_banks[0]
        for yo in range(dh):
            for xo in range(dw):
                rx = (dw - 1 - xo) if flip_x else xo
                ry = (dh - 1 - yo) if flip_y else yo
                sx = rx if dw <= td else min(rx * td // dw, td - 1)
                sy = ry if dh <= td else min(ry * td // dh, td - 1)
                v = pdata[sy * td + sx]
                if v == 0:
                    continue  # Transparent pixel
                ddx = tx + xo
                ddy = ty + yo
                if 0 <= ddx < sw and 0 <= ddy < sh:
                    if v < len(pal):
                        pxi[ddx, ddy] = pal[v]
                    else:
                        pxi[ddx, ddy] = (128, 128, 128, 255)
    return img


# ── Group extraction ───────────────────────────────────────────────────────────


def find_best_colcd(data, to_tex, frame_idx, pal_banks, rt_sprite_path,
                    search_range=range(280, 420)):
    """Find the optimal colcd base for a frame by comparing against a runtime sprite.

    Renders the frame with each candidate colcd, downsamples the runtime reference
    from 4x, and returns the colcd with the highest pixel match percentage.

    Returns (best_colcd, match_pct) or (SPRITE_PAL_BASE, 0) if no match found."""
    if not os.path.exists(rt_sprite_path):
        return SPRITE_PAL_BASE, 0.0

    rt = Image.open(rt_sprite_path).convert("RGBA")
    rt_1x = rt.resize((rt.width // 4, rt.height // 4), Image.LANCZOS)
    rt_px = rt_1x.load()

    best_pct = 0.0
    best_colcd = SPRITE_PAL_BASE

    for colcd in search_range:
        try:
            img = extract_sprite_frame(data, to_tex, frame_idx, pal_banks,
                                       colcd_base=colcd)
        except Exception:
            continue
        if img is None or img.size != rt_1x.size:
            continue

        ex_px = img.load()
        score = 0
        total = 0
        for y in range(img.height):
            for x in range(img.width):
                ec = ex_px[x, y]
                rc = rt_px[x, y]
                if ec[3] > 0 and rc[3] > 0:
                    total += 1
                    if (abs(ec[0] - rc[0]) <= 24 and
                            abs(ec[1] - rc[1]) <= 24 and
                            abs(ec[2] - rc[2]) <= 24):
                        score += 1

        if total > 0:
            pct = score * 100.0 / total
            if pct > best_pct:
                best_pct = pct
                best_colcd = colcd

    return best_colcd, best_pct


# Path to colcd_map.csv (generated by the runtime logger in aboutspr.c)
COLCD_MAP_CSV = os.path.join(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))), "colcd_map.csv")


def load_colcd_map(csv_path=None):
    """Load the runtime-captured colcd mapping from CSV.

    Returns dict: {(stage, cg_number): colcd}
    """
    path = csv_path or COLCD_MAP_CSV
    if not os.path.exists(path):
        return {}
    import csv as csv_mod
    mapping = {}
    with open(path) as f:
        for row in csv_mod.DictReader(f):
            key = (int(row["stage"]), int(row["cg_number"]))
            mapping[key] = int(row["colcd"])
    return mapping

def extract_group(afs_path, entries, grp_entry, pal_banks, output_dir):
    """Extract all CG frames for a texgrpdat group.

    Args:
        afs_path: path to SF33RD.AFS
        entries: AFS entry table
        grp_entry: tuple (group_idx, num_of_1st, apfn, to_tex, desc)
        pal_banks: list of 64-color palette banks
        output_dir: output directory

    Returns:
        number of frames extracted
    """
    group_idx, num_of_1st, apfn, to_tex, desc = grp_entry

    if apfn < 0 or apfn >= len(entries):
        return 0

    data = read_afs_file(afs_path, entries[apfn])
    if len(data) < 4:
        return 0

    num_frames = struct.unpack_from("<I", data, 0)[0] // 4
    if num_frames == 0 or num_frames > 10000:
        return 0

    # Verify texture data is accessible
    if to_tex >= len(data):
        print(f"  WARNING: to_tex ({to_tex}) >= data size ({len(data)}), skipping")
        return 0

    grp_dir = os.path.join(output_dir, f"group_{group_idx:02d}")
    os.makedirs(grp_dir, exist_ok=True)

    extracted = 0
    rt_sprites_dir = os.path.join(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))), "assets", "sprites")

    # Load colcd map from runtime CSV
    colcd_map = load_colcd_map()
    stage = afs_entry_to_stage(apfn)

    for fi in range(num_frames):
        cg_number = num_of_1st + fi

        # 3-tier colcd resolution:
        # 1. CSV lookup (fastest, most accurate)
        colcd = None
        if stage is not None:
            colcd = colcd_map.get((stage, cg_number))

        # 2. Runtime sprite comparison (slow but catches missing CSV entries)
        if colcd is None:
            rt_path = os.path.join(rt_sprites_dir,
                                   f"sprite_{group_idx}_{cg_number}.png")
            if os.path.exists(rt_path):
                colcd, pct = find_best_colcd(
                    data, to_tex, fi, pal_banks, rt_path)
                if pct < 30.0:
                    colcd = SPRITE_PAL_BASE

        # 3. Default: SPRITE_PAL_BASE (=300, universal stage palette slot)

        img = extract_sprite_frame(data, to_tex, fi, pal_banks,
                                   colcd_base=colcd)
        if img is not None:
            img.save(os.path.join(grp_dir,
                                 f"sprite_{group_idx}_{cg_number}.png"))
            extracted += 1

    # Save metadata
    meta = {
        "group_index": group_idx,
        "description": desc,
        "afs_entry": apfn,
        "num_of_1st": num_of_1st,
        "to_tex": to_tex,
        "num_frames": num_frames,
        "extracted": extracted,
    }
    with open(os.path.join(grp_dir, "metadata.json"), "w") as f:
        json.dump(meta, f, indent=2)

    return extracted


# ── Main ───────────────────────────────────────────────────────────────────────


def main():
    afs_path = r"C:\Users\dov\AppData\Roaming\CrowdedStreet\3SX\resources\SF33RD.AFS"
    output_dir = "output/stage_sprites"

    # Parse args
    args = sys.argv[1:]
    target_group = None
    target_stage = None
    list_only = False

    i = 0
    while i < len(args):
        if args[i] == "--group" and i + 1 < len(args):
            target_group = int(args[i + 1])
            i += 2
        elif args[i] == "--stage" and i + 1 < len(args):
            target_stage = int(args[i + 1])
            i += 2
        elif args[i] == "--list":
            list_only = True
            i += 1
        elif args[i] == "--output" and i + 1 < len(args):
            output_dir = args[i + 1]
            i += 2
        elif args[i] == "all":
            i += 1
        else:
            i += 1

    if list_only:
        print(f"{'Grp':>4} {'CG Range':>14} {'AFS':>5} {'to_tex':>7}  {'Stage':>6}  Description")
        print("─" * 75)
        for idx, first, apfn, tex, desc in TEXGRPDAT:
            stage = afs_entry_to_stage(apfn)
            stage_str = f"  {stage:>2}" if stage is not None else "   -"
            print(f" {idx:>3}  {first:>5}-???      {apfn:>5}  {tex:>6}  {stage_str}    {desc}")
        return

    entries = read_afs(afs_path)
    os.makedirs(output_dir, exist_ok=True)

    # Filter groups
    groups_to_extract = []
    for grp in TEXGRPDAT:
        idx, first, apfn, tex, desc = grp
        if target_group is not None and idx != target_group:
            continue
        if target_stage is not None:
            stage = afs_entry_to_stage(apfn)
            if stage != target_stage:
                continue
        groups_to_extract.append(grp)

    if not groups_to_extract:
        print("No matching groups found.")
        return

    print(f"\n🎬 Stage Sprite Extraction")
    print(f"📁 Output: {output_dir}")
    print(f"📊 Groups: {len(groups_to_extract)}")
    print(f"{'─' * 60}")

    total_frames = 0
    for grp in groups_to_extract:
        idx, first, apfn, tex, desc = grp
        stage = afs_entry_to_stage(apfn)

        # Load palette: prefer runtime ColorRAM dump, fall back to AFS reconstruction
        pal_banks = None
        if stage is not None:
            pal_banks = load_colorram_dump(stage)
            if pal_banks:
                print(f"    Using runtime ColorRAM dump for stage {stage:02d}")
        if pal_banks is None:
            stage_pal_afs = STAGE_PAL_AFS.get(stage) if stage is not None else None
            pal_banks = build_colorram(afs_path, entries, stage_pal_afs)
            print(f"    Using AFS-reconstructed palette (no dump available)")

        stage_str = f"stage {stage:02d}" if stage is not None else "effect"
        print(f"\n  Group {idx:02d} ({stage_str}): {desc}")
        print(f"    AFS={apfn} CG_start={first} to_tex={tex}")

        extracted = extract_group(afs_path, entries, grp, pal_banks, output_dir)
        total_frames += extracted
        print(f"    Extracted: {extracted} frames")

    print(f"\n{'─' * 60}")
    print(f"🎉 Total: {total_frames} frames from {len(groups_to_extract)} groups")
    print(f"   Output: {output_dir}/")


if __name__ == "__main__":
    main()
