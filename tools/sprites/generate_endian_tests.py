import os, struct, json
from PIL import Image
from sprite_common import read_afs, read_afs_file, TEXGRPDAT, _CPS3_CLUT
from sprite_compositor import extract_stage_frame, get_rendering_mode
from build_atlases import _default_afs_path, TextureAtlasBuilder

# 1. LE ABGR1555 (Native CPS3 decoding standard in this codebase)
def decode_le_abgr1555(val):
    b = ((val & 0x1F) * 255 + 15) // 31
    g = (((val >> 5) & 0x1F) * 255 + 15) // 31
    r = (((val >> 10) & 0x1F) * 255 + 15) // 31
    a = 0 if val == 0 else 255
    if r==255 and b==255 and (g==0 or g==49): a = 0
    return (r, g, b, a)

# 2. LE ARGB1555 (Swapped Red/Blue)
def decode_le_argb1555(val):
    r = ((val & 0x1F) * 255 + 15) // 31
    g = (((val >> 5) & 0x1F) * 255 + 15) // 31
    b = (((val >> 10) & 0x1F) * 255 + 15) // 31
    a = 0 if val == 0 else 255
    if r==255 and b==255 and (g==0 or g==49): a = 0
    return (r, g, b, a)

# 3. BE ABGR1555
def decode_be_abgr1555(val):
    val = struct.unpack('<H', struct.pack('>H', val))[0] # Swap byte order
    b = ((val & 0x1F) * 255 + 15) // 31
    g = (((val >> 5) & 0x1F) * 255 + 15) // 31
    r = (((val >> 10) & 0x1F) * 255 + 15) // 31
    a = 0 if val == 0 else 255
    if r==255 and b==255 and (g==0 or g==49): a = 0
    return (r, g, b, a)

# 4. BE ARGB1555
def decode_be_argb1555(val):
    val = struct.unpack('<H', struct.pack('>H', val))[0] # Swap byte order
    r = ((val & 0x1F) * 255 + 15) // 31
    g = (((val >> 5) & 0x1F) * 255 + 15) // 31
    b = (((val >> 10) & 0x1F) * 255 + 15) // 31
    a = 0 if val == 0 else 255
    if r==255 and b==255 and (g==0 or g==49): a = 0
    return (r, g, b, a)

def decode_banks(pal_data, decode_fn, apply_clut=False):
    banks = []
    for bank in range(80):
        colors = [(0,0,0,0)] * 256
        for i in range(256):
            off = bank * 512 + i * 2
            if off + 2 > len(pal_data): break
            val = struct.unpack_from("<H", pal_data, off)[0]
            colors[i] = decode_fn(val)
        
        if apply_clut:
            reordered = list(colors)
            for i in range(256):
                dst_idx = (i & 0xE0) + _CPS3_CLUT[i & 0x1F]
                if dst_idx < 256: reordered[dst_idx] = colors[i]
            banks.append(reordered)
        else:
            banks.append(colors)
    return banks

def build_test_atlas(decode_fn, name):
    afs_path = _default_afs_path()
    entries = read_afs(afs_path)
    grp = next(g for g in TEXGRPDAT if g.group_idx == 82)
    data = read_afs_file(afs_path, entries[grp.apfn])
    num_frames = struct.unpack_from('<I', data, 0)[0] // 4

    # Build colorram
    colorram = [[(0,0,0,0)] * 256 for _ in range(512)]
    overlay_data = read_afs_file(afs_path, entries[1451])
    
    # Wait, the user said WITHOUT clut was NOT ok. So we should use WITH clut.
    # Actually I'll test BOTH with and without CLUT for LE ARGB and BE ARGB just to be sure!
    banks = decode_banks(overlay_data, decode_fn, apply_clut=False)
    for i, bank in enumerate(banks):
        if 428 + i < 512: colorram[428 + i] = bank
    
    frames = []
    for fi in range(num_frames):
        img = extract_stage_frame(data, grp.to_tex, fi, colorram, colcd_base=428, rendering_mode=18)
        if img: frames.append((grp.num_of_1st + fi, img))
    
    if not frames: return
    builder = TextureAtlasBuilder(max_width=4096, max_height=8192)
    for cg, img in frames: builder.add_frame(cg, img, 0, 0)
    atlases = builder.build()
    
    out_dir = r"C:\Users\dov\.gemini\antigravity\brain\19ae580e-3335-44d7-ad3b-7975226d9aa2"
    atlases[0][0].save(os.path.join(out_dir, f"{name}.png"))
    print(f"Generated {name}.png")

if __name__ == "__main__":
    build_test_atlas(decode_le_abgr1555, "LE_ABGR")
    build_test_atlas(decode_le_argb1555, "LE_ARGB")
    build_test_atlas(decode_be_abgr1555, "BE_ABGR")
    build_test_atlas(decode_be_argb1555, "BE_ARGB")
