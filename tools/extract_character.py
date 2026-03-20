#!/usr/bin/env python3
"""
Extract full animation frames from AFS archive for a character.
Uses the offline extraction pipeline: AFS -> decompress -> unswizzle -> palette -> composite.
"""

import struct
import os
import sys
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from PIL import Image

# ============================================================
# AFS Reader
# ============================================================

def read_afs(filepath):
    with open(filepath, "rb") as f:
        f.read(4)  # magic
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

# ============================================================
# dctex_linear table
# ============================================================

_seed = [0x0000,0x0002,0x0008,0x000A,0x0020,0x0022,0x0028,0x002A,
         0x0080,0x0082,0x0088,0x008A,0x00A0,0x00A2,0x00A8,0x00AA,
         0x0200,0x0202,0x0208,0x020A,0x0220,0x0222,0x0228,0x022A,
         0x0280,0x0282,0x0288,0x028A,0x02A0,0x02A2,0x02A8,0x02AA]
_seedAdd = [0x0000,0x0004,0x0010,0x0014,0x0040,0x0044,0x0050,0x0054,
            0x0100,0x0104,0x0110,0x0114,0x0140,0x0144,0x0150,0x0154]
DTL = [0] * 1024
for _i in range(16):
    for _j in range(32):
        DTL[_j + _i * 64] = _seed[_j] + _seedAdd[_i]
    for _j in range(32):
        DTL[_j + _i * 64 + 32] = DTL[_j + _i * 64] + 1

# ============================================================
# Decompression
# ============================================================

def lz_ext_p6_fx(src, dst_size):
    dst = bytearray(dst_size)
    si = di = 0
    while di < dst_size and si < len(src):
        t = src[si]; si += 1
        c = t & 0xC0
        if c == 0x00:
            dst[di] = t; di += 1
        elif c == 0x40:
            t &= 0x3F
            back = (t >> 2) + 1; length = (t & 3) + 2
            p = di - back
            for _ in range(length):
                if di >= dst_size: break
                dst[di] = dst[p] if 0 <= p < di else 0
                di += 1; p += 1
        elif c == 0x80:
            if si >= len(src): break
            t = ((t & 0x3F) << 8) | src[si]; si += 1
            back = (t >> 6) + 1; length = (t & 0x3F) + 2
            p = di - back
            for _ in range(length):
                if di >= dst_size: break
                dst[di] = dst[p] if 0 <= p < di else 0
                di += 1; p += 1
        else:
            fg = t & 0x30; length = (t & 0x0F) + 2
            for _ in range(length):
                if di >= dst_size or si >= len(src): break
                dst[di] = fg | (src[si] >> 4); di += 1
                if di >= dst_size: break
                dst[di] = fg | (src[si] & 0x0F); di += 1
                si += 1
    return bytes(dst)

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

# ============================================================
# Palette
# ============================================================

def decode_abgr1555(word):
    b = (word & 0x1F) * 255 // 31
    g = ((word >> 5) & 0x1F) * 255 // 31
    r = ((word >> 10) & 0x1F) * 255 // 31
    a = 255 if (word & 0x8000) else 0
    return (r, g, b, a)

def load_palette(pal_data, row):
    """Load palette row (64 colors) from plXXpl.bin data."""
    pal = [(0, 0, 0, 0)] * 64
    for i in range(64):
        word = struct.unpack_from("<H", pal_data, row * 128 + i * 2)[0]
        pal[i] = decode_abgr1555(word)
    return pal

# ============================================================
# Frame extraction
# ============================================================

def extract_frame(data, to_tex, frame_idx, pal):
    """Extract and composite a single animation frame.
    Returns a PIL Image or None if the frame is empty.
    """
    num_frames = struct.unpack_from("<I", data, 0)[0] // 4
    if frame_idx >= num_frames:
        return None

    foff = struct.unpack_from("<I", data, frame_idx * 4)[0]
    cnt = struct.unpack_from("<H", data, foff)[0]
    if cnt == 0:
        return None

    # Parse tiles
    tiles = []
    eoff = foff + 2
    for _ in range(cnt):
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

    # Detect per-tile flip state
    all_xflip = all((t[2] & 0x8000) != 0 for t in tiles)

    # Walk positions and compute draw positions
    # Runtime: x accumulation is cx -= dx (or cx += dx when global flip)
    # Draw offset: x - dw when attr & 0x8000, y + dh when attr & 0x4000
    cx = cy = 0.0
    pts = []
    for dx, dy, attr, code, td, dw, dh, px in tiles:
        if all_xflip:
            cx += dx
        else:
            cx -= dx
        cy -= dy

        # Draw offset only when global flip (all_xflip)
        # For mixed frames, per-tile attr only affects pixel mirroring, not position
        if all_xflip:
            draw_x = int(cx) - (dw if (attr & 0x8000) else 0)
            draw_y = int(cy) + (dh if (attr & 0x4000) else 0)
        else:
            draw_x = int(cx)
            draw_y = int(cy)
        pts.append((draw_x, draw_y))

    # Bounding box using draw positions
    mnx = min(p[0] for p in pts)
    mny = min(p[1] for p in pts)
    mxx = max(pts[i][0] + tiles[i][5] for i in range(len(tiles)))
    mxy = max(pts[i][1] + tiles[i][6] for i in range(len(tiles)))
    sw = mxx - mnx
    sh = mxy - mny

    if sw <= 0 or sh <= 0:
        return None

    # Composite
    img = Image.new("RGBA", (sw, sh), (0, 0, 0, 0))
    pxi = img.load()

    for i, (dx, dy, attr, code, td, dw, dh, pdata) in enumerate(tiles):
        tx = pts[i][0] - mnx
        ty = pts[i][1] - mny
        # Per-tile pixel flip: only for mixed frames (global flip XORs with per-tile)
        # all_xflip: global flip cancels per-tile flip → no pixel mirror
        # mixed: no global flip, per-tile attr controls pixel mirror
        flip_x = (attr & 0x8000) != 0 and not all_xflip
        flip_y = (attr & 0x4000) != 0 and not all_xflip

        for yo in range(dh):
            for xo in range(dw):
                rx = (dw - 1 - xo) if flip_x else xo
                ry = (dh - 1 - yo) if flip_y else yo
                sx = rx if dw <= td else min(rx * td // dw, td - 1)
                sy = ry if dh <= td else min(ry * td // dh, td - 1)
                v = pdata[sy * td + sx]
                if v == 0:
                    continue
                ddx = tx + xo
                ddy = ty + yo
                if 0 <= ddx < sw and 0 <= ddy < sh:
                    pxi[ddx, ddy] = pal[v % 64] if v < 64 else (128, 128, 128, 255)

    return img

# ============================================================
# Character definitions
# ============================================================

CHARACTERS = {
    "gill":    {"apfn": 1460, "pal_apfn": 1463, "to_tex": 210820,  "num_of_1st": 0},
    "alex":    {"apfn": 1465, "pal_apfn": 1466, "to_tex": 116432,  "num_of_1st": 1568},
    "ryu":     {"apfn": 1468, "pal_apfn": 1470, "to_tex": 72828,   "num_of_1st": 2592},
    "yun":     {"apfn": 1472, "pal_apfn": 1473, "to_tex": 114816,  "num_of_1st": 3552},
    "dudley":  {"apfn": 1476, "pal_apfn": 1477, "to_tex": 110728,  "num_of_1st": 4992},
    "necro":   {"apfn": 1479, "pal_apfn": 1480, "to_tex": 116636,  "num_of_1st": 6144},
    "hugo":    {"apfn": 1483, "pal_apfn": 1484, "to_tex": 158400,  "num_of_1st": 7392},
    "ibuki":   {"apfn": 1487, "pal_apfn": 1489, "to_tex": 151744,  "num_of_1st": 8384},
    "elena":   {"apfn": 1492, "pal_apfn": 1493, "to_tex": 142292,  "num_of_1st": 10208},
    "oro":     {"apfn": 1495, "pal_apfn": 1497, "to_tex": 137680,  "num_of_1st": 11776},
    "yang":    {"apfn": 1499, "pal_apfn": 1500, "to_tex": 116892,  "num_of_1st": 13280},
    "ken":     {"apfn": 1502, "pal_apfn": 1504, "to_tex": 71900,   "num_of_1st": 14656},
    "sean":    {"apfn": 1506, "pal_apfn": 1508, "to_tex": 80596,   "num_of_1st": 15712},
    "urien":   {"apfn": 1510, "pal_apfn": 1512, "to_tex": 135428,  "num_of_1st": 16800},
    "akuma":   {"apfn": 1514, "pal_apfn": 1516, "to_tex": 116116,  "num_of_1st": 18272},
    "chunli":  {"apfn": 1518, "pal_apfn": 1520, "to_tex": 144584,  "num_of_1st": 19456},
    "makoto":  {"apfn": 1522, "pal_apfn": 1523, "to_tex": 177724,  "num_of_1st": 21120},
    "q":       {"apfn": 1525, "pal_apfn": 1526, "to_tex": 222124,  "num_of_1st": 23008},
    "twelve":  {"apfn": 1528, "pal_apfn": 1529, "to_tex": 131348,  "num_of_1st": 24704},
    "remy":    {"apfn": 1531, "pal_apfn": 1533, "to_tex": 125420,  "num_of_1st": 25856},
}

# ============================================================
# Worker function for multiprocessing
# ============================================================

def _extract_one_frame(args):
    """Extract a single frame. Designed to run in a worker process."""
    afs_path, apfn, pal_apfn, to_tex, num_of_1st, fi, output_dir, pal_idx = args
    entries = read_afs(afs_path)
    data = read_afs_file(afs_path, entries[apfn])
    pal_data = read_afs_file(afs_path, entries[pal_apfn])
    pal = load_palette(pal_data, pal_idx)
    img = extract_frame(data, to_tex, fi, pal)
    if img is None:
        return 0
    cg = num_of_1st + fi
    path = os.path.join(output_dir, f"sprite_{cg}.png")
    img.save(path)
    return 1

# ============================================================
# Single-character extraction
# ============================================================

def extract_character(afs_path, char_name, char, frame_range, output_dir, pal_idx, workers=None):
    """Extract frames for one character. Returns (extracted_count, total_frames)."""
    entries = read_afs(afs_path)
    data = read_afs_file(afs_path, entries[char["apfn"]])

    num_frames = struct.unpack_from("<I", data, 0)[0] // 4

    # Resolve frame range
    if frame_range == "all":
        frames = list(range(num_frames))
    elif "-" in frame_range:
        start, end = frame_range.split("-")
        frames = list(range(int(start), int(end) + 1))
    else:
        frames = [int(frame_range)]

    frames = [fi for fi in frames if fi < num_frames]
    os.makedirs(output_dir, exist_ok=True)

    print(f"  {char_name}: {num_frames} frames (cg {char['num_of_1st']}\u2013{char['num_of_1st'] + num_frames - 1}), palette {pal_idx}")

    t0 = time.perf_counter()

    if workers == 1 or len(frames) <= 4:
        # Single-process path
        pal = load_palette(read_afs_file(afs_path, entries[char["pal_apfn"]]), pal_idx)
        count = 0
        for fi in frames:
            img = extract_frame(data, char["to_tex"], fi, pal)
            if img is None:
                continue
            cg = char["num_of_1st"] + fi
            path = os.path.join(output_dir, f"sprite_{cg}.png")
            img.save(path)
            count += 1
    else:
        # Multi-process path
        task_args = [
            (afs_path, char["apfn"], char["pal_apfn"], char["to_tex"],
             char["num_of_1st"], fi, output_dir, pal_idx)
            for fi in frames
        ]
        with ProcessPoolExecutor(max_workers=workers) as pool:
            results = pool.map(_extract_one_frame, task_args, chunksize=32)
            count = sum(results)

    elapsed = time.perf_counter() - t0
    print(f"    \u2192 {count} frames in {elapsed:.1f}s to {output_dir}/")
    return count, num_frames

# ============================================================
# Info mode
# ============================================================

def print_info(afs_path):
    """Print a summary table of all characters with frame counts and CG ranges."""
    entries = read_afs(afs_path)
    print(f"{'Character':<10} {'Frames':>6} {'CG Start':>9} {'CG End':>9} {'AFS#':>5}")
    print("-" * 45)
    total = 0
    for name in sorted(CHARACTERS.keys(), key=lambda k: CHARACTERS[k]["num_of_1st"]):
        char = CHARACTERS[name]
        data = read_afs_file(afs_path, entries[char["apfn"]])
        num_frames = struct.unpack_from("<I", data, 0)[0] // 4
        cg_start = char["num_of_1st"]
        cg_end = cg_start + num_frames - 1
        print(f"{name:<10} {num_frames:>6} {cg_start:>9} {cg_end:>9} {char['apfn']:>5}")
        total += num_frames
    print("-" * 45)
    print(f"{'TOTAL':<10} {total:>6}")

# ============================================================
# Main
# ============================================================

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <afs_path> <character|all|--info> [frame_range|all] [output_dir] [palette_idx]")
        print(f"  character: {', '.join(sorted(CHARACTERS.keys()))}")
        print(f"  frame_range: N, N-M, or 'all' (default: all)")
        print(f"  palette_idx: 0-13 (default: 0 = Jab/LP)")
        print(f"\nExamples:")
        print(f"  {sys.argv[0]} SF33RD.AFS --info")
        print(f"  {sys.argv[0]} SF33RD.AFS alex all output/alex")
        print(f"  {sys.argv[0]} SF33RD.AFS all all output/sprites")
        sys.exit(1)

    afs_path = sys.argv[1]
    char_name = sys.argv[2].lower()

    # --info mode: just print the table
    if char_name == "--info":
        print_info(afs_path)
        return

    frame_range = sys.argv[3] if len(sys.argv) > 3 else "all"
    output_dir = sys.argv[4] if len(sys.argv) > 4 else "output/sprites"
    pal_idx = int(sys.argv[5]) if len(sys.argv) > 5 else 0

    # Build character list
    if char_name == "all":
        chars = sorted(CHARACTERS.keys(), key=lambda k: CHARACTERS[k]["num_of_1st"])
    elif char_name in CHARACTERS:
        chars = [char_name]
    else:
        print(f"Unknown character: {char_name}")
        print(f"Available: {', '.join(sorted(CHARACTERS.keys()))}")
        sys.exit(1)

    grand_total = 0
    for name in chars:
        char = CHARACTERS[name]
        # When extracting all characters, each gets a subdirectory
        char_dir = os.path.join(output_dir, name) if len(chars) > 1 else output_dir
        count, _ = extract_character(afs_path, name, char, frame_range, char_dir, pal_idx)
        grand_total += count

    if len(chars) > 1:
        print(f"\nTotal: {grand_total} frames across {len(chars)} characters")

if __name__ == "__main__":
    main()

