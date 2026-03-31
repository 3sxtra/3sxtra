# Pre-Processing CPS3 Assets for Modern Platforms

## Current State of the Rendering Pipeline

The 3SX engine, currently heavily reliant on legacy DC/PS2 hybrid code, employs an incredibly complex, tile-based hardware sprite extraction rendering pipeline for its visual assets. 

### 1. Source Data and Packing
* **Format:** All visual assets are bundled into `SF33RD.AFS`. This is a raw binary container storing `.bin` files containing the character, effect, UI, and background data.
* **Internal Structure:** Each `.bin` file includes:
  * Animation frame definitions (tile counts, X/Y deltas).
  * Texture table arrays pointing to compressed tiles.
  * Compressed pixel data using a custom, hardware-specific compression algorithm: `lz_ext_p6_fx` (LZ77 variant).
  * Color palette definitions (ABGR1555 16-bit format).

### 2. The Runtime Decoding & Caching Process
The engine currently fetches these chunks using `fsOpen` and routes them into the `MTSBase` cache (`texcash.c`). 
1. The game streams raw compressed chunks into memory.
2. An LZ77 GPU compute shader (`lz77_decode.gpu.comp`) or CPU-bound routines decompress the raw bytecode.
3. The pixels are unswizzled using the `dctex_linear` table, a remnant of **PS2 GS memory swizzle ordering**, to reconstruct the true raster layout.
4. The pixels are held as 8-bit index arrays in memory (ColorRAM indices).

### 3. Rendering Dispatch (Mode 17, 18, 33)
When rendering a sprite (`mtrans.c` and `aboutspr.c`), the game invokes `Mtrans_use_trans_mode()`. Every active object has a `my_mts` identifier mapping to one of three hardware-emulated rendering paths:
* **Mode 17:** Characters and hit-boxes. It ignores sprite attributes and uses a base palette offset universally.
* **Mode 18:** Most UI and Effects. Uses a 9-bit mask applied to `attr` data for dynamic palette colors. Often contains ROM garbage in unused bits.
* **Mode 33:** Stage Backgrounds. Uses a strict 4-bit mask and uses bits 5-11 for bounding box/scaling parameters.

### 4. Overrides and High-Res Mods
Currently, high-res mods (like characters) are supported via `sprite_override.c` and `sdl_game_renderer.c`. They work by hashing the raw 16x16 8-bit index tiles *after* decompression and looking up a matching `hash.png` file on disk. This is a very computationally expensive bottleneck as it still requires all the CPS3 legacy overhead.

---

## Proposed Modernization Pipeline

To permanently pre-process CPS3 assets into a format suited for modern platforms, we must decouple the asset storage from the legacy hardware bounds.

### Step 1: Pre-Process the AFS Archive (Offline Extraction)
We already have the capability to extract these natively using the `tools/sprites/extract_sprites.py` script.
Instead of treating `extract_sprites.py` as a modding tool, we should integrate it as an offline asset compilation step.
1. **Extraction:** Dump all AFS entries into complete, un-swizzled, full-frame PNG sequences (or sprite atlases).
2. **Palette Application:** Apply the exact `ColorRAM` ABGR1555 palettes offline via the Mode 17/18/33 extraction rules.
3. **Metadata Generation:** Dump all animation deltas, timings, and hitboxes into human-readable, mod-friendly `JSON` files.

### Step 2: New Native Asset Formats
The new engine pipeline should abandon raw index loading and `dctex_linear` unswizzling at runtime.
* **Graphics:** Use standard `KTX2` (GPU compressed, optimal for loading directly into VRAM) or `PNG` atlases.
* **Metadata:** Parse `.json` for animation frames and hitboxes.
* **Audio:** Keep the current ADX streaming (as it's lightweight), or convert offline to standard `Ogg Vorbis`.

### Step 3: Engine Architecture Changes
1. **Bypass `mtrans.c` & `PPGFile.c`:**
   * Modify `texgroup.c` (`fsOpen` requests) to load the new JSON and KTX2 assets instead of requesting sectors from `fs_sys.c`.
   * Disable the LZ77 Compute Shader and PS2 Unswizzle arrays as they will be obsolete.
2. **Modern Render Graph:**
   * Replace `Mtrans_use_trans_mode()` with standard modern 2D Sprite Batches (via SDL_GPU).
   * Instead of generating an index atlas and applying a palette shader pass on the GPU, just upload the pre-colored RGBA textures directly.
   * *For Palette Swapping:* Supply a grayscale or masked texture array and use a simple uniform buffer for the current palette swap (e.g., Ryu P1 vs P2), rather than legacy `ColorRAM`.

### Benefits
* **Performance:** Eliminates CPU-side decompression, unswizzling, and complex palette indexing. Loading times drop to near zero via direct memory mapping of KTX2.
* **Modding:** HD Mods are native. Modders can edit PNGs/JSONs directly without extracting/repacking AFS files or guessing MD5 hashes for 16x16 tiles.
* **Portability:** Strips away the last vestiges of PS2 and Dreamcast hardware logic (`dctex_linear`, Mode 17/18/33).