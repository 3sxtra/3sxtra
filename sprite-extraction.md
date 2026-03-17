# Sprite Extraction & Override System

## Overview

3SX supports dumping, extracting, upscaling, and overriding character sprite textures at runtime. There are two complementary systems:

1. **Runtime tile override** — dumps/loads 16x16 tiles during gameplay via `--dump-sprites` / `--sprites-path`
2. **Offline extraction** — reconstructs full animation frames from raw AFS archive data without running the game

## AFS File Structure

The game's assets are stored in `SF33RD.AFS` (typically at `%APPDATA%/CrowdedStreet/3SX/resources/`). Each character has three key files:

| File | Example | Contents |
|------|---------|----------|
| `plXX.bin` | `pl01.bin` (Alex) | Sprite tile data + animation frame definitions |
| `plXXpl.bin` | `pl01pl.bin` | 14 costume palettes (28 rows x 64 colors x 2 bytes = 3584 bytes) |
| `plXXplm.bin` | `pl01plm.bin` | Effect palettes (super flash, stun, etc.) |

### Character AFS Indices (from `texgrpdat`)

| Index | Character | AFS # | `to_tex` offset |
|-------|-----------|-------|-----------------|
| 1 | Gill | 1460 | 210820 (0x337A4) |
| 2 | Alex | 1465 | 116432 (0x1C6D0) |
| 3 | Ryu | 1468 | 72828 (0x11C7C) |
| 4 | Yun | 1472 | 114816 (0x1C080) |
| 5 | Dudley | 1476 | 110728 (0x1B088) |
| 6 | Necro | 1479 | 116636 (0x1C79C) |
| 7 | Hugo | 1483 | 158400 (0x26AC0) |
| 8 | Ibuki | 1487 | 151744 (0x250C0) |
| 9 | Elena | 1492 | 142292 (0x22BD4) |
| 10 | Oro | 1495 | 137680 (0x219D0) |
| 11 | Yang | 1499 | 116892 (0x1C89C) |
| 12 | Ken | 1502 | 71900 (0x118DC) |
| 13 | Sean | 1506 | 80596 (0x13AD4) |
| 14 | Urien | 1510 | 135428 (0x21104) |
| 15 | Akuma | 1514 | 116116 (0x1C574) |
| 16 | Chun-Li | 1518 | 144584 (0x234C8) |
| 17 | Makoto | 1522 | 177724 (0x2B63C) |
| 18 | Q | 1525 | 222124 (0x363AC) |
| 19 | Twelve | 1528 | 131348 (0x20114) |
| 20 | Remy | 1531 | 125420 (0x1E9EC) |

**Important:** The `TexGroupData` struct fields are: `num_of_1st, apfn, conv, ix1st, use, to_tex, to_chd`. The `to_tex` field (6th value) is the texture table offset, NOT `to_chd` (7th value).

## plXX.bin Internal Structure

Each character `.bin` file has two main sections:

### Trans Table (offset 0)

The file starts with an array of `u32` offsets, one per animation frame. Each offset points to a frame definition:

```
u16 tile_count
TileMapEntry[tile_count]:
    s16 delta_x      // X position delta from previous tile
    s16 delta_y      // Y position delta from previous tile
    u16 attr          // Attribute flags (bit 15 = x-flip, bit 14 = y-flip)
    u16 code          // Index into the texture table
```

### Texture Table (at `to_tex` offset)

An array of `u32` offsets (relative to `to_tex`), one per tile. Each offset points to a `TEX` structure:

```
u8 wh             // Encodes tile dimensions and display size
u8 data[...]      // Compressed tile pixel data (lz_ext_p6_fx format)
```

The `wh` byte encodes:
- `wh & 0x03` → tile data multiplier: `(wh & 3) + 1` = 1-4, tile_dim = multiplier * 8
- `(wh & 0xE0) >> 2` → display width in pixels
- `(wh & 0x1C) * 2` → display height in pixels
- Tile data size = `(multiplier * multiplier) << 6` bytes

Tile sizes: wm=1 → 8x8 (64B), wm=2 → 16x16 (256B), wm=3 → 24x24 (576B), wm=4 → 32x32 (1024B)

## Decompression: lz_ext_p6_fx

The game uses a custom LZ compression variant. Each byte's top 2 bits determine the operation:

| Bits | Operation |
|------|-----------|
| `0x00` | Literal byte (value = byte itself) |
| `0x40` | Short back-reference: 6-bit field encodes offset (bits 2-5) and length (bits 0-1) + 2 |
| `0x80` | Long back-reference: 14-bit field (current byte + next byte) encodes offset (bits 6-13) and length (bits 0-5) + 2 |
| `0xC0` | Palette nibble expansion: bits 4-5 = palette group flag, bits 0-3 = length + 2. Each source byte produces 2 output bytes (high nibble, low nibble) OR'd with the flag |

## PS2 Pixel Unswizzling: dctex_linear

Decompressed tile pixel data is in PS2 GS memory swizzle order. The `dctex_linear` lookup table converts from raster (x,y) to swizzled source index.

The table is generated from two seed arrays:

```python
seed = [0x0000, 0x0002, 0x0008, 0x000A, 0x0020, 0x0022, 0x0028, 0x002A,
        0x0080, 0x0082, 0x0088, 0x008A, 0x00A0, 0x00A2, 0x00A8, 0x00AA,
        0x0200, 0x0202, 0x0208, 0x020A, 0x0220, 0x0222, 0x0228, 0x022A,
        0x0280, 0x0282, 0x0288, 0x028A, 0x02A0, 0x02A2, 0x02A8, 0x02AA]
seedAdd = [0x0000, 0x0004, 0x0010, 0x0014, 0x0040, 0x0044, 0x0050, 0x0054,
           0x0100, 0x0104, 0x0110, 0x0114, 0x0140, 0x0144, 0x0150, 0x0154]

for i in range(16):
    for j in range(32):
        dctex_linear[j + i*64] = seed[j] + seedAdd[i]
    for j in range(32):
        dctex_linear[j + i*64 + 32] = dctex_linear[j + i*64] + 1
```

Usage per tile size:
- **8x8 and 16x16 tiles:** `pixel[y][x] = data[dctex_linear[x + (y << 5)]]`
- **32x32 tiles:** `pixel[y][x] = data[dctex_linear[y * 32 + x]]`

## Tile Positioning

Animation frames define tiles with delta positions. Walking the deltas:

```python
cx, cy = 0.0, 0.0
for tile in frame.tiles:
    cx -= tile.delta_x
    cy -= tile.delta_y   # Y is inverted (PS2 coordinate system)
    # Tile top-left is at (cx, cy)
```

Display dimensions (`dw`, `dh`) define how much of the tile data is visible. When `dw <= tile_dim`, it's a crop (1:1 pixels). When `dw > tile_dim`, it's a nearest-neighbor stretch.

## Palette Format

### plXXpl.bin Structure

Each palette file contains `28 rows x 64 colors x 2 bytes` = 3584 bytes (or 7168 for Gill).

- **Rows 0-13:** 14 selectable costume palettes
- **Rows 14-15:** Reserved
- **Rows 16-21:** Effect palettes (brightness levels, etc.)
- **Rows 22-27:** Additional effects

### Color Format: ABGR1555 (16-bit, little-endian)

```
Bit 15:    Alpha (1 = opaque, 0 = transparent)
Bits 10-14: Red (5 bits, scale to 0-255: value * 255 / 31)
Bits 5-9:   Green (5 bits)
Bits 0-4:   Blue (5 bits)
```

**Note:** This is ABGR order (blue in low bits), NOT ARGB. The game's `read_rgba16_color` reads bits 0-4 as R, but the palette files store B in the low bits.

### 14 Costume Palettes

Selected by button press at character select:

| Index | Button | Index | Button |
|-------|--------|-------|--------|
| 0 | Jab (LP) | 7 | Forward (MK) |
| 1 | Strong (MP) | 8 | Roundhouse (HK) |
| 2 | Fierce (HP) | 9 | Start+Short |
| 3 | Start+Jab | 10 | Start+Forward |
| 4 | Start+Strong | 11 | Start+Roundhouse |
| 5 | Start+Fierce | 12 | LP+MK+HP |
| 6 | Short (LK) | 13 | Twelve Clone |

## Runtime Override System

### CLI Flags

```bash
3sx --dump-sprites <path>     # Dump tiles and full sprites during gameplay
3sx --sprites-path <path>     # Load hi-res tile overrides
```

### Tile Override Pipeline

1. Each 256x256 indexed atlas texture contains 16x16 tile slots
2. When a tile changes (`UnlockTexture`), the renderer:
   - Hashes each 16x16 region (FNV-1a of raw INDEX8 pixel data)
   - Looks up `<sprites_path>/<hash>.png` for a 64x64 hi-res override
   - Composites matching tiles into a parallel 1024x1024 RGBA atlas
   - At render time, uses the hi-res atlas instead of the original

### 4x Resolution

`TEXTURE_SCALE = 4` in `sdl_game_renderer.c`:
- Canvas: 1536x896 (4x of 384x224)
- Vertex positions scaled 4x in `draw_quad`
- Hi-res tile atlases: 1024x1024 (4x of 256x256)
- Original 256x256 textures upscaled by GPU NEAREST

## Upscaling Workflow

### Tile-based (runtime)

```bash
# 1. Dump tiles during gameplay
3sx --dump-sprites output/tiles

# 2. AI upscale (16x16 -> 64x64)
realesrgan-ncnn-vulkan -i output/tiles -o output/tiles_hires -n realesrgan-x4plus-anime -s 4

# 3. Restore alpha from originals
python3 restore_alpha.py output/tiles output/tiles_hires

# 4. Copy to override folder
cp output/tiles_hires/*.png output/tiles/

# 5. Run with overrides
3sx --sprites-path output/tiles
```

### Full sprite (offline extraction)

```bash
# Extract full animation frames from AFS
python3 tools/extract_sprites.py SF33RD.AFS output/sprites_extracted

# Or use tools/colorize_alex.py as reference for per-character extraction
```

## Key Source Files

| File | Purpose |
|------|---------|
| `src/port/sdl/sdl_game_renderer.c` | Tile override system, hi-res atlas, PNG dump/load |
| `src/sf33rd/Source/Game/rendering/mtrans.c` | Character sprite rendering, tile decompression, full sprite dump |
| `src/sf33rd/Source/Game/rendering/texcash.c` | MTS tile cache (page counts, lifetimes) |
| `src/sf33rd/Source/Game/rendering/color3rd.c` | Palette loading, COL struct, color_file table |
| `src/sf33rd/Source/Common/PPGFile.c` | PPG format, dctex_linear table, lz decompression |
| `src/sf33rd/Source/Game/rendering/texgroup.c` | texgrpdat table, AFS file loading |
| `tools/extract_sprites.py` | Offline AFS/tile extraction |
| `tools/colorize_alex.py` | Full frame composition with palette |
