#!/usr/bin/env python3
"""
Shared CPS3 sprite utilities — AFS I/O, LZ decompression, unswizzle, palette decode.

Used by extract_sprites.py and extract_stage.py.
"""

import struct
from collections import namedtuple


# ══════════════════════════════════════════════════════════════════════════════
# Named Types
# ══════════════════════════════════════════════════════════════════════════════

TileChip = namedtuple("TileChip", "dx dy attr code td dw dh px")
"""A single decoded CPS3 sprite chip tile.

Fields:
    dx, dy   : delta position from previous tile
    attr     : attribute word (flip flags in high bits, palette in low bits)
    code     : tile code index into texture data
    td       : tile dimension in pixels (8, 16, 24, or 32)
    dw, dh   : draw width and height in pixels
    px       : unswizzled pixel data (bytes, indices into palette)
"""

TexGroupEntry = namedtuple("TexGroupEntry", "group_idx num_of_1st apfn to_tex desc")
"""A texgrpdat entry — one CPS3 sprite group.

Fields:
    group_idx  : engine group index
    num_of_1st : CG number of the first frame in this group
    apfn       : AFS entry number for the group's sprite data
    to_tex     : byte offset within the AFS entry to texture data
    desc       : human-readable description
"""


# ══════════════════════════════════════════════════════════════════════════════
# CPS3 Palette System Constants
# ══════════════════════════════════════════════════════════════════════════════

COLORS_PER_BANK = 64
PALETTE_BANK_BYTES = COLORS_PER_BANK * 2  # 128 bytes (64 colors × 2 bytes)
TOTAL_BANKS = 512
CHAR_PAL_ROWS = 28  # character palette: 28 rows of 64 colors

# ══════════════════════════════════════════════════════════════════════════════
# AFS Archive I/O
# ══════════════════════════════════════════════════════════════════════════════

AFS_MAGIC = b"AFS\x00"
MAX_AFS_ENTRIES = 100_000


def read_afs(filepath):
    """Read AFS archive header, returning list of (offset, size) tuples.

    Validates magic bytes and caps entry count to prevent OOM on corrupt files.
    """
    with open(filepath, "rb") as f:
        magic = f.read(4)
        if magic != AFS_MAGIC:
            raise ValueError(f"Not a valid AFS file: {filepath} (magic={magic!r})")
        ec = struct.unpack("<I", f.read(4))[0]
        if ec > MAX_AFS_ENTRIES:
            raise ValueError(
                f"AFS entry count {ec} exceeds maximum {MAX_AFS_ENTRIES} in {filepath}"
            )
        entries = []
        for _ in range(ec):
            o, s = struct.unpack("<II", f.read(8))
            entries.append((o, s))
    return entries


def read_afs_file(filepath, entry):
    """Read a single AFS entry's raw data."""
    with open(filepath, "rb") as f:
        f.seek(entry[0])
        return f.read(entry[1])


# ══════════════════════════════════════════════════════════════════════════════
# CPS3 LZ Decompression
# ══════════════════════════════════════════════════════════════════════════════


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


# ══════════════════════════════════════════════════════════════════════════════
# CPS3 Tile Unswizzle
# ══════════════════════════════════════════════════════════════════════════════


def _build_dtl_table():
    """Build the CPS3 unswizzle lookup table (1024 entries)."""
    seed = [
        0x0000,
        0x0002,
        0x0008,
        0x000A,
        0x0020,
        0x0022,
        0x0028,
        0x002A,
        0x0080,
        0x0082,
        0x0088,
        0x008A,
        0x00A0,
        0x00A2,
        0x00A8,
        0x00AA,
        0x0200,
        0x0202,
        0x0208,
        0x020A,
        0x0220,
        0x0222,
        0x0228,
        0x022A,
        0x0280,
        0x0282,
        0x0288,
        0x028A,
        0x02A0,
        0x02A2,
        0x02A8,
        0x02AA,
    ]
    seed_add = [
        0x0000,
        0x0004,
        0x0010,
        0x0014,
        0x0040,
        0x0044,
        0x0050,
        0x0054,
        0x0100,
        0x0104,
        0x0110,
        0x0114,
        0x0140,
        0x0144,
        0x0150,
        0x0154,
    ]
    table = [0] * 1024
    for i in range(16):
        for j in range(32):
            table[j + i * 64] = seed[j] + seed_add[i]
        for j in range(32):
            table[j + i * 64 + 32] = table[j + i * 64] + 1
    return table


DTL = _build_dtl_table()


def unswizzle(pdata, td):
    """Unswizzle CPS3 tile pixel data using the DTL lookup table."""
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


# ══════════════════════════════════════════════════════════════════════════════
# Palette Decode (ABGR1555 LE)
# ══════════════════════════════════════════════════════════════════════════════


def decode_color_abgr1555(val, idx=1):
    """Decode a single ABGR1555 LE color value → (R, G, B, A).

    Uses accurate (c5 * 255 + 15) // 31 rounding.
    Index 0 with val==0 → fully transparent.
    """
    b5 = val & 0x1F
    g5 = (val >> 5) & 0x1F
    r5 = (val >> 10) & 0x1F
    r = (r5 * 255 + 15) // 31
    g = (g5 * 255 + 15) // 31
    b = (b5 * 255 + 15) // 31

    # Index 0 is typically transparent, and CPS3 palettes often use pure magenta
    # as a colorkey for transparency in offline rips/data. Sometimes off-magenta is used.
    if r == 255 and b == 255 and (g == 0 or g == 49):
        return (0, 0, 0, 0)

    if idx == 0 and val == 0:
        return (0, 0, 0, 0)

    return (r, g, b, 255)


# CPS3 CLUT reorder table (from palConvRowTim2CI8Clut in color3rd.c).
# Swaps indices 8-15 ↔ 16-23 within each 32-entry block.
_CPS3_CLUT = [
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
]


def _clut_reorder_bank(bank):
    """Reorder a palette bank using the CPS3 CLUT table.

    The engine applies palConvRowTim2CI8Clut when uploading palettes to the GPU:
    dst[(i & 0xE0) + clut[i & 0x1F]] = src[i].
    Pixel indices from lz_ext_p6_fx expect this reordered layout.
    """
    reordered = list(bank)  # copy
    for i in range(len(bank)):
        dst_idx = (i & 0xE0) + _CPS3_CLUT[i & 0x1F]
        if dst_idx < len(reordered):
            reordered[dst_idx] = bank[i]
    return reordered


def decode_palette_banks(pal_data, apply_clut=False):
    """Decode raw ABGR1555 LE palette data into a list of 64-color banks.

    Source format: bits 0-4=B, 5-9=G, 10-14=R, 15=blend (not alpha).
    Index 0 = transparent.
    """
    n_banks = len(pal_data) // PALETTE_BANK_BYTES
    banks = []
    for bank in range(n_banks):
        colors = [(0, 0, 0, 0)] * COLORS_PER_BANK
        for i in range(COLORS_PER_BANK):
            off = bank * PALETTE_BANK_BYTES + i * 2
            if off + 2 > len(pal_data):
                break
            val = struct.unpack_from("<H", pal_data, off)[0]
            colors[i] = decode_color_abgr1555(val, i)

        if apply_clut:
            # Apply CPS3 native CLUT swap (8-15 <-> 16-23)
            reordered = list(colors)
            for i in range(len(colors)):
                dst_idx = (i & 0xE0) + _CPS3_CLUT[i & 0x1F]
                if dst_idx < len(reordered):
                    reordered[dst_idx] = colors[i]
            banks.append(reordered)
        else:
            banks.append(colors)

    return banks


# ══════════════════════════════════════════════════════════════════════════════
# Shared Data Tables (single source of truth)
# ══════════════════════════════════════════════════════════════════════════════

# Stage names (internal index 0-21)
STAGE_NAMES = [
    "gill_boss",  # 00 - Gill (Final Boss)
    "alex_newyork",  # 01 - Alex (New York)
    "ryu_japan",  # 02 - Ryu (Japan)
    "yun_hongkong",  # 03 - Yun (Hong Kong)
    "dudley_england",  # 04 - Dudley (England)
    "necro_russia",  # 05 - Necro/Q (Russia)
    "hugo_germany",  # 06 - Hugo (Germany)
    "ibuki_japan",  # 07 - Ibuki (Japan)
    "elena_kenya",  # 08 - Elena (Kenya)
    "oro_brazil",  # 09 - Oro (Brazil)
    "yang_hongkong",  # 10 - Yang (Hong Kong Alt)
    "ken_newyork",  # 11 - Ken (New York Alt)
    "sean_brazil",  # 12 - Sean (Brazil)
    "urien_egypt",  # 13 - Urien (Egypt)
    "akuma_japan",  # 14 - Akuma (Japan)
    "chunli_china",  # 15 - Chun-Li (China)
    "makoto_japan",  # 16 - Makoto (Japan)
    "necro_alt",  # 17 - Q/Necro Alt (reuses bg)
    "twelve",  # 18 - Twelve
    "remy",  # 19 - Remy
    "bonus_car",  # 20 - Bonus Stage (Car)
    "bonus_parry",  # 21 - Bonus Stage (Parry)
]

# Stage palette AFS entries (from bg_data.c)
STAGE_PAL_AFS = {
    0: 1383,
    1: 1388,
    2: 1391,
    3: 1394,
    4: 1397,
    5: 1400,
    6: 1403,
    7: 1406,
    8: 1409,
    9: 1412,
    10: 1415,
    11: 1418,
    12: 1421,
    13: 1424,
    14: 1427,
    15: 1430,
    16: 1433,
    17: 1418,
    18: 1436,
    19: 1439,
    20: 1442,
    21: 1447,
}

# Stage tile AFS entries (from bg_data.c)
STAGE_TILE_AFS = {
    0: 1387,
    1: 1390,
    2: 1393,
    3: 1396,
    4: 1399,
    5: 1402,
    6: 1404,
    7: 1408,
    8: 1411,
    9: 1414,
    10: 1417,
    11: 1420,
    12: 1423,
    13: 1426,
    14: 1429,
    15: 1432,
    16: 1435,
    17: 1420,
    18: 1438,
    19: 1441,
    20: 1445,
    21: 1450,
}

# ── texgrpdat table (from texgroup.c) ──────────────────────────────────────────

TEXGRPDAT = [
    # group_idx  num_of_1st  apfn    to_tex   desc
    TexGroupEntry(20, 27040, 1452, 436, "System/common"),
    TexGroupEntry(22, 27104, 1454, 134308, "System/select object JP"),
    TexGroupEntry(24, 29152, 1455, 14180, "System/intro"),
    TexGroupEntry(25, 29344, 1456, 92368, "System/endings"),
    TexGroupEntry(26, 30640, 1461, 5788, "System/super flash"),
    TexGroupEntry(29, 30896, 1457, 3448, "System/continue"),
    TexGroupEntry(32, 31152, 1446, 120700, "Bonus/car smash"),
    TexGroupEntry(33, 32432, 1444, 2580, "Bonus/misc"),
    TexGroupEntry(34, 36896, 1462, 4212, "System/training HUD"),
    TexGroupEntry(37, 32560, 1458, 25088, "System/win/lose"),
    TexGroupEntry(42, 34352, 1401, 1704, "Stage 05 (Necro/Russia) sprites"),
    TexGroupEntry(43, 34384, 1410, 2180, "Stage 08 (Elena/Kenya) sprites"),
    TexGroupEntry(44, 34448, 1389, 19772, "Stage 01 (Alex/NY) sprites"),
    TexGroupEntry(45, 34576, 1395, 3256, "Stage 03 (Yun/HK) sprites"),
    TexGroupEntry(46, 34672, 1428, 1672, "Stage 14 (Akuma/Japan) sprites"),
    TexGroupEntry(47, 34704, 1405, 1832, "Stage 06 (Hugo/Germany) sprites"),
    TexGroupEntry(48, 34736, 1413, 2220, "Stage 09 (Oro/Brazil) sprites"),
    TexGroupEntry(49, 34832, 1425, 296, "Stage 13 (Urien/Egypt) sprites"),
    TexGroupEntry(50, 34864, 1398, 12208, "Stage 04 (Dudley/England) sprites"),
    TexGroupEntry(51, 34960, 1434, 4016, "Stage 16 (Makoto/Japan) sprites"),
    TexGroupEntry(52, 35024, 1386, 4568, "Stage 00 (Gill/Boss) sprites"),
    TexGroupEntry(53, 35120, 1407, 1100, "Stage 07 (Ibuki/Japan) sprites"),
    TexGroupEntry(54, 35152, 1443, 64, "Stage 20 (Bonus car) sprites"),
    TexGroupEntry(55, 35184, 1440, 7372, "Stage 19 (Remy) sprites"),
    TexGroupEntry(56, 35328, 1431, 17548, "Stage 15 (Chun-Li/China) sprites"),
    TexGroupEntry(58, 35648, 1392, 2100, "Stage 02 (Ryu/Japan) sprites"),
    TexGroupEntry(59, 35744, 1448, 8496, "Stage 21 (Bonus parry) sprites"),
    TexGroupEntry(61, 35904, 74, 54492, "Effect/common (PPG 74)"),
    TexGroupEntry(62, 36096, 34, 6552, "Effect/flames"),
    TexGroupEntry(63, 36160, 35, 1540, "Effect/sparks"),
    TexGroupEntry(64, 36192, 36, 11480, "Effect/hit FX A"),
    TexGroupEntry(65, 36288, 37, 2300, "Effect/hit FX B"),
    TexGroupEntry(66, 36320, 38, 1712, "Effect/dust"),
    TexGroupEntry(67, 36352, 39, 6460, "Effect/shadow"),
    TexGroupEntry(68, 36384, 40, 2332, "Effect/projectiles A"),
    TexGroupEntry(69, 36416, 41, 3412, "Effect/projectiles B"),
    TexGroupEntry(70, 36448, 42, 444, "Effect/guard"),
    TexGroupEntry(71, 36480, 43, 1072, "Effect/block"),
    TexGroupEntry(72, 36512, 44, 2832, "Effect/super A"),
    TexGroupEntry(73, 36544, 45, 4676, "Effect/super B"),
    TexGroupEntry(74, 36576, 46, 5992, "Effect/super C"),
    TexGroupEntry(75, 36608, 47, 8904, "Effect/EX moves A"),
    TexGroupEntry(76, 36640, 48, 14508, "Effect/EX moves B"),
    TexGroupEntry(77, 36704, 49, 1536, "Effect/misc A"),
    TexGroupEntry(78, 36736, 50, 2968, "Effect/misc B"),
    TexGroupEntry(79, 36768, 51, 6628, "Effect/misc C"),
    TexGroupEntry(80, 36800, 52, 2868, "Effect/misc D"),
    TexGroupEntry(81, 36864, 53, 4776, "Effect/misc E"),
    TexGroupEntry(82, 37024, 1459, 42292, "System/menu objects"),
    TexGroupEntry(89, 37408, 1384, 2488, "Stage 00 (Gill/Boss) chip A"),
    TexGroupEntry(90, 37536, 1385, 12984, "Stage 00 (Gill/Boss) chip B"),
    TexGroupEntry(91, 34576, 1416, 3256, "Stage 10 (Yang/HK) sprites"),
    TexGroupEntry(92, 34448, 1419, 19772, "Stage 11 (Ken/NY) sprites"),
    TexGroupEntry(93, 34736, 1422, 2220, "Stage 12 (Sean/Brazil) sprites"),
    TexGroupEntry(94, 34352, 1437, 1704, "Stage 18 (Twelve) sprites"),
    TexGroupEntry(98, 27104, 1453, 134044, "System/select object EN"),
]
