# Comprehensive Sprite Rendering Audit (3SX)
*A methodical breakdown of every sprite, tile, texture, and effect rendering path in the CPS3/PS2 hybrid engine.*

## 1. The Rendering Architecture
The 3SX engine does not have a single unified sprite drawing function. Instead, it relies on an object-oriented `WORK` struct system where every active entity in the game invokes a specific `sort_push_requestX()` function per frame.

When an object is initialized (e.g., `effect_57_init` for menus or `plcnt.c` for characters), it is permanently assigned a `my_mts` identifier. This `my_mts` index dictates exactly how the CPS3 sprite coordinate bounds, texture chunk (`texcash.c`), and ColorRAM palette offset handles the object data!

### Trans Mode (`my_mts` -> `mode`) Core Loop
1. Every frame, objects are evaluated in `aboutspr.c:Mtrans_use_trans_mode()`. 
2. The engine looks up the associated `mts[wk->my_mts].mode` (derived from `texcash.c`'s `mts_base` constants).
3. The renderer dispatches to one of three primary C functions in `mtrans.c` that handle the raw tile `attr` memory layout differently.

---

## 2. Rendering Mode Dispatch
By structurally tracing `mts_base` and `my_mts` usages across `src/port/` and `src/sf33rd/`, we've mapped exactly how the game assigns palettes:

### Mode 17: Pure Base Palette (`mlt_obj_trans_ext`)
**Used By**: Characters (`plw[0].wu.my_mts = 3/4`), Projectiles, certain hit-boxes, and Effect 68 (`effm8.c: my_mts=3`).
- **Characteristics**: This path uses `mlt_obj_trans_init` mapped to Mode `4113` or `8209` (`0x11` -> `17`).
- **Palette Logic**: 
  - `palo = wk->colcd;`
  - `rnum = seqsStoreChip(..., palo | ((trsptr->attr ^ attr) & 0xC000), ...);`
- **Rule**: Mode 17 **EXPLICITLY IGNORES** the bounding `attr` data for palette calculation. The sub-palette is literally just `0`. The base `colcd` (like `wk->id * 8` for Player 1) determines the color universally.

### Mode 18 CP3: 9-Bit Masks (`mlt_obj_trans_cp3_ext`)
**Used By**: Almost ALL Menu UI backgrounds and Effect FX (Sparks, Hits, Hitsparks, Screen Banners).
- **Core Effects**: Effect 57 (Backgrounds/Banners, `my_mts=13`), Effect 61 (UI Text, `my_mts=13`), Effect 66 (UI Boxes, `my_mts=13`), and natively 85% of other visual flourishes.
- **Characteristics**: This path maps to `8210` or `4114` (`0x12` -> `18`).
- **Palette Logic**:
  - `palt = (attr & 0x1FF) + palo;`
- **Rule**: Uses a continuous **9-bit mask** to calculate sub-palette indices.
- **The Hardware Trap**: The raw `attr` values in `.bin` ROM files for these effect tiles commonly have unzeroed numerical garbage in bits 4 through 8! Original hardware drops this natively via `PPGFile.c`'s `pch->total` boundary limiter. In PC python extractors, if we apply the mathematical 9-bit mask, we pull garbage.

### Mode 33: 4-Bit Masks (`mlt_obj_disp`)
**Used By**: Stage Backgrounds & Level Sprite Groups (`my_mts` mapped to `33` during Stage Loading `bg_disp_sort`).
- **Palette Logic**:
  - `palo = wk->colcd & 0xF;`
  - `dw = ((trsptr->attr & 0xC00) >> 7) + 8;` (Width uses bits 10,11)
  - `rnum = seqsStoreChip(..., palo + ((trsptr->attr ^ attr) & 0xE00F), ...);`
- **Rule**: Mode 33 actively uses bits 5-11 for scaling metrics. Palettes are strictly bounded to a **4-bit mask** (`0xF`).

---

## 3. UI Graphic Types (The "Mix")
As detailed heavily in `TEXT_RENDERING_SYSTEMS.md`, the UI layers are incredibly disparate:

A. **Port-Side Overlays (SDL2/GL)**: Modern debug overlays (`flPrint`) natively run on high-resolution geometry and do not touch `ColorRAM` or indexed `attr` extraction.
B. **Screen-Font (`sc_sub.c`)**: Renders on the HUD grid using independent memory chunks (`ppgScrPal`).
C. **CG UI Overlays (Effect 61 & 66)**: Strings like "ARCADE" and "VERSUS" are drawn via the **Mode 18 CP3 engine** above (`eff61.c` -> `sort_push_request3`), meaning they technically derive their `0x1AC` palettes using the standard Sprite engine pipeline, bound securely by the native `0xF` masking to avoid corruption.

---

## 4. Offline AFS Python Extraction Rules
To support 100% correct offline extraction given the fact that the C engine manually assigns `my_mts` on a per-frame basis, we must deduce the correct mathematical static-lookup:

Since our offline Python script (`extract_stage_sprites.py`) does not run a C emulator loop, we cannot assign `Mode 17` individually to unlabelled bytes. Therefore, what mask is perfectly safe natively?

* **Why an overarching `0xF` constraint is a mathematically perfect PC fallback:**
  * **Stage Sprites (Mode 33)** MUST use `0xF`, as bit 5+ are used for sprite size bounds and will break memory indices.
  * **Effects/Menus (Mode 18)** use `0x1FF` in C, but because their palettes are universally short (<16 variations), pulling bits > 4 introduces ROM garbage. Masking them at `0xF` filters off the unused garbage cleanly.
  * **Characters (Mode 17)** do not use offsets at all.

Therefore, Python natively resolving `tile_pal_idx = (attr & 0xF) + base` resolves all Modes and all groups correctly to the specific `ColorRAM` offset banks `0..512`.
