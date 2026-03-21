"""Find the correct colcd for EVERY frame in group 52 by comparing against runtime sprites."""
import struct, sys, os
from PIL import Image
sys.path.insert(0, r'd:\3sxtra\tools')
import extract_stage_sprites as ess

cram_banks = ess.load_colorram_dump(0)
AFS = r'C:\Users\dov\AppData\Roaming\CrowdedStreet\3SX\resources\SF33RD.AFS'
with open(AFS, 'rb') as f:
    f.read(4)
    ec = struct.unpack('<I', f.read(4))[0]
    entries = [struct.unpack('<II', f.read(8)) for _ in range(ec)]
    f.seek(entries[1386][0])
    data = f.read(entries[1386][1])

to_tex = 4568
num_frames = struct.unpack_from('<I', data, 0)[0] // 4

print(f"Scanning {num_frames} frames in group 52...")
print(f"{'CG':>6} {'best_colcd':>10} {'match%':>7} {'score':>6} {'total':>6}")
print("-" * 45)

colcd_counts = {}
for fi in range(num_frames):
    cg = 35024 + fi
    rt_path = os.path.join(r'D:\3sxtra\assets\sprites', f'sprite_52_{cg}.png')
    if not os.path.exists(rt_path):
        print(f"{cg:6d}  NO RUNTIME SPRITE")
        continue

    rt = Image.open(rt_path).convert('RGBA')
    rt_1x = rt.resize((rt.width // 4, rt.height // 4), Image.LANCZOS)
    rt_px = rt_1x.load()

    best_pct = 0
    best_colcd = -1
    best_score = 0
    best_total = 0

    for colcd in range(280, 420):
        ess.SPRITE_PAL_BASE = colcd
        try:
            img = ess.extract_sprite_frame(data, to_tex, fi, cram_banks)
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
                    if abs(ec[0]-rc[0]) <= 24 and abs(ec[1]-rc[1]) <= 24 and abs(ec[2]-rc[2]) <= 24:
                        score += 1

        if total > 0:
            pct = score * 100 / total
            if pct > best_pct:
                best_pct = pct
                best_colcd = colcd
                best_score = score
                best_total = total

    if best_colcd >= 0:
        colcd_counts[best_colcd] = colcd_counts.get(best_colcd, 0) + 1
    print(f"{cg:6d} {best_colcd:10d} {best_pct:6.1f}% {best_score:6d} {best_total:6d}")

ess.SPRITE_PAL_BASE = 348
print("\nColcd frequency:")
for cd, cnt in sorted(colcd_counts.items(), key=lambda x: -x[1]):
    print(f"  colcd={cd:3d}: {cnt} frames")
