import os, struct
from PIL import Image
from sprite_common import _CPS3_CLUT, decode_palette_banks
from sprite_compositor import extract_stage_frame, load_colorram_dump, _composite, TOTAL_BANKS, _parse_tiles, _compute_positions
from extract_sprites import _default_afs_path, read_afs, read_afs_file, TEXGRPDAT

afs_path = _default_afs_path()
entries = read_afs(afs_path)
grp = next(g for g in TEXGRPDAT if g.group_idx == 82)
data = read_afs_file(afs_path, entries[grp.apfn])

out_dir = r'D:\3sxtra\tools\sprites\output\gallery'
os.makedirs(out_dir, exist_ok=True)

def force_extract(fi, bank_data):
    tiles = _parse_tiles(data, grp.to_tex, fi)
    if not tiles: return None
    pts, all_xflip = _compute_positions(tiles)
    def palette_fn(attr, tile_idx):
        return bank_data
    return _composite(tiles, pts, all_xflip, palette_fn)

html = ['<html><body style="background:#222; color:#fff;"><h1>Palette Search Gallery</h1>']

overlay_data = read_afs_file(afs_path, entries[1451])
banks_clut = decode_palette_banks(overlay_data, apply_clut=True)

html.append('<h2>AFS 1451 Banks (LE ABGR, CLUT)</h2><div style="display:flex; flex-wrap:wrap;">')
for i, bank in enumerate(banks_clut):
    img = force_extract(37170 - grp.num_of_1st, bank)
    if img:
        img = img.crop((0, 0, img.width, img.height))
        img_path = f'afs1451_{i}.png'
        img.save(os.path.join(out_dir, img_path))
        html.append(f'<div style="padding:10px;text-align:center;"><img src="{img_path}"><br>Bank {i}</div>')
html.append('</div>')

colorram = load_colorram_dump(0)
html.append('<h2>Stage 0 RAM Dump Banks</h2><div style="display:flex; flex-wrap:wrap;">')
for i, bank in enumerate(colorram):
    img = force_extract(37170 - grp.num_of_1st, bank)
    if img:
        img_path = f'ram_{i}.png'
        img.save(os.path.join(out_dir, img_path))
        html.append(f'<div style="padding:10px;text-align:center;"><img src="{img_path}"><br>Bank {i}</div>')
html.append('</div></body></html>')

with open(os.path.join(out_dir, 'gallery.html'), 'w', encoding='utf-8') as f:
    f.write('\n'.join(html))
print('Generated gallery in output/gallery')
