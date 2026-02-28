# 3Sxtra Text and UI Rendering Systems Guide

The 3sxtra codebase utilizes two independent rendering stacks for 2D elements: the **Port-Side Overlay System** (for debug, FPS, and modern UI text drawn outside the game framebuffer) and the **Game-Side Screen-Font System** (the original CPS3/PS2 text engine that renders into the game's native 384×224 framebuffer).

---

## 1. Port-Side Overlay Text (SDL Text Renderer)

This system provides a modern, high-resolution text overlay independent of the game's internal resolution. It is used for debug information, netplay status, and non-diagetic UI.

### Architecture
A dispatch layer (`src/port/sdl/sdl_text_renderer.c`) routes calls to one of three backends based on `RendererBackend`:
1. **OpenGL** (`sdl_text_renderer_gl.c`): Uses immediate-mode textured quads with custom GL shaders.
2. **SDL_GPU** (`sdl_text_renderer_gpu.c`): Modern batched vertex buffer implementation with `SDL_GPUShader`, queued per frame.
3. **SDL2D** (`sdl_text_renderer_sdl.c`): Fallback using the built-in `SDL_RenderDebugText` API.

### Font Atlas
Both GL and GPU backends generate a **128×64 single-channel (R8) texture atlas** at initialization using data from `src/port/imgui_font_8x8.h`.
*   Contains 128 ASCII glyphs (U+0000 to U+007F).
*   Arranged in a 16 columns × 8 rows grid.
*   Each glyph is exactly 8×8 pixels, unpacked bit-by-bit.

### Coordinate System & Rendering
The API `SDLTextRenderer_DrawText(text, x, y, scale, r, g, b, target_width, target_height)` uses:
*   `x` and `y` as **absolute pixel coordinates** on the output window.
*   The `scale` argument maps the layout from a virtual 480p space to the actual window height (`target_height / 480.0f`).
*   Glyphs have an 8×10px cell (8px wide, 10px tall stride) and advance 7px horizontally per character.

### Sub-Features
*   **Background Rectangles**: When enabled, a translucent rectangle (default `rgba(0,0,0,0.6)`) with `2px` padding is drawn behind the text bounds as a single 6-vertex quad.
*   **PS2 Debug Buffer (`flPrint`)**: The renderer consumes the legacy PS2 debug ring buffer. Each character receives a 1px black drop-shadow for contrast.

---

## 2. Game-Side Screen Font System (`sc_sub.c`)

This is the original CPS3→PS2 text engine. It renders all authentic in-game HUD text: player names, match timers, combo messages, scores, training data, health bars, and portraits. This system operates *within* the game loop and is subject to the game's pause state and tick rate.

### System Initialization & Asset Loading
The system boots via `Scrscreen_Init()`. It is intimately tied to the **PPG (Proprietary Packed Graphics)** format, which acts as a container for textures and palettes.
1.  **Loading the Archive**: Requests `scrscrn.ppg`.
2.  **Allocating Palettes**: Loads 4 distinct chunks of palette memory: `ppgScrPal` (default HUD), `ppgScrPalFace` (portraits), `ppgScrPalShot` (controller buttons), and `ppgScrPalOpt` (options menus).
3.  **Loading Textures**: Pulls 6+ sequential texture pages into the renderer natively.
4.  **Reusable Geometry**: A reusable quad array `RendererVertex scrscrntex[4]` caches the immediate drawing state.

### The 8×8 Grid Layout
To map text effortlessly without arbitrary pixel math, the native 384×224 resolution is divided into a **48 columns × 28 rows** grid of 8×8 pixel cells.
Almost all core `sc_sub.c` drawing functions accept `(u16 x, u16 y)` representing **grid coordinates**. Internally:
`x = x * 8; y = y * 8;`

### Font Architecture & Text Pipelines
`sc_sub.c` utilizes distinct bitmap font families carved from the PPG atlases.

#### A. Fixed-Width Font (`SSPutStr`)
The standard 8×8 HUD font. 
*   **Decoding**: The character's ASCII value is translated into a UV coordinate within the `scrscrn` texture. 
*   *Note: Includes a `+ 0x80` UV offset, pushing the UVs into the right-half of the texture page.*

#### B. Proportional Font (`SSPutStrPro`)
Used for stylized text requiring kerning.
*   **The Kerning Table**: Relies on a 128-byte array `ascProData`. 
*   **Rendering**: The renderer actively shrinks the geometry of the quad by slicing off empty left/right pixels at the vertex level. The next character's X position is advanced by exactly `(8 - left_trim) - right_trim` pixels.

#### C. Scaled "Bigger" Font (`SSPutStr_Bigger`)
A distinct font sheet used for large hit combos.
*   **Scaling & Gradients**: Multiplies the vertex geometry by a `sc` parameter and applies 4-vertex ARGB gradient lookups.
*   **Half-Spaces**: The `$` character acts as an inline command to advance the cursor by half a width without drawing.

### Font Type Reference
| Font Type | Glyph Size | Texture Page | Rendering Function(s) | Notes |
|---|---|---|---|---|
| **Fixed-Width** | 8×8 px | Page 1 | `SSPutStr`, `SSPutStr2` | Standard HUD text. |
| **Fixed-Width (Num)** | 8×8 px | Page 1 | `SSPutDec` | Digit-only variant. |
| **Proportional** | (1–8) × 8 px | Page 1 | `SSPutStrPro` | Variable width via `ascProData`. |
| **Proportional Scaled**| Var × (8 × sc) | Page 1 | `SSPutStrPro_Scale` | Floats for position/size. |
| **Bigger (Scaled)** | (8 × sc)² px | Page 1 | `SSPutStr_Bigger` | Supports gradients and `$` half-spaces. |
| **Bigger (Numeric)** | 11×8 px | Page 4 | `SSPutDec3` | Unique wide numeric digits. |
| **Tile 8×8** | 8×8 px | Any | `scfont_put` | Arbitrary single-cell tile stamp. |
| **Tile Block** | Multi-cell | Any | `scfont_sqput`, `scfont_sqput2` | Multi-cell region (names, combos). |
| **Score 16×24** | 16×24 px | Page 2 | `score16x24_put` | Large score digits. |

### Visual Styling Mechanics
Almost every drawing call accepts an `atr` (attribute) byte:
*   **Bits 0-5**: Selects the palette index (0-63). Mapped via `ppgSetupCurrentPaletteNumber()`.
*   **Bit 6 `(atr & 0x40)`**: Vertical Flip flag.
*   **Bit 7 `(atr & 0x80)`**: Horizontal Flip flag.

The game eschews a traditional depth buffer for a Painter's Algorithm layer array: `PrioBase[priority]`. HUD text universally locks to `PrioBase[2]`.

---

## 3. Menu UI Backgrounds (Banners & Rectangles)

The game features dynamic colored banners (e.g., the red banner at the top of the Training menu) and translucent grey backdrop rectangles (e.g., in Button Config, Replay menus). These are **not** drawn via simple 2D GL primitives. Instead, they are instantiated as standard CPS3 game objects/sprites using specific effect initializers.

### Top Red Banners (Effect 57)
Managed by `effect_57_init` (in `eff57.c`).
*   It initializes an object (ID `57`) assigned to use the `_sel_pl_char_table` (a global UI sprite table).
*   It is given a specific color palette code (`0x1AC`).
*   It utilizes a slide-in animation routine and loops its rendering updates, ultimately pushing to the sprite engine via `sort_push_request4`.

### Translucent Grey Rectangles (Effect 66)
Managed by `effect_66_init` (in `eff66.c`) and internally referred to as a "Half-Object Flash" effect.
*   Uses predefined dimension arrays (`EFF66_Half_OBJ_Data`) to map out the X/Y bounds and Z depth, depending on the configuration parameter.
*   Like the banners, it pulls graphic data from `_sel_pl_char_table`.
*   Critically, the opacity of these rectangles is controlled by the object's `my_clear_level` attribute (e.g., set to 42 for semi-transparent layers, up to 128 for others).
*   It leverages the sprite sorting engine (`sort_push_request2`/`4`/`A`/`B`) and native hardware alpha-blending for execution.

#### `EFF66_Half_OBJ_Data` Format
Each entry is `{x_offset, width, y_offset, height, charset_id, clear_level, rl_waza}`. The first four fields define the box rectangle:
*   **`y_offset`**: Controls vertical position. **Increasing `y_offset` moves the box UP on screen** (inverted Y-axis).
*   **`height`**: Box height in pixels. Changing height also affects position — **reducing height shifts the box DOWN** because `screen_top ≈ 224 - y_offset - height`. To shrink a box while keeping its top edge stable, increase `y_offset` by the same amount you reduce `height`.
*   **`x_offset`**: Horizontal position (standard: positive = rightward).
*   **`width`**: Box width in pixels.

Custom box entries (negative `option` parameter, e.g., `-0x7FF0`) use `cg_type = option & 0x3FFF` as the index into this array. Network lobby boxes use indices 15 (Internet) and 16 (LAN).

#### Coordinate Alignment with `SSPutStr_Bigger`
`SSPutStr_Bigger` uses **absolute pixel coordinates** in the 384×224 framebuffer, while Effect 66 boxes use sprite-relative coordinates with an inverted Y-axis. Aligning text inside boxes requires coordinating both systems — there is no shared coordinate space, so adjustments must be verified visually.

### Main Menu Entries (Effect 61 CG Sprites)
Unlike most HUD text, the primary menu selection strings (e.g., "GAME OPTION", "BUTTON CONFIG.", "ARCADE") **do not use the standard screen-font text engine** (`sc_sub.c`). Instead, they are rendered entirely as **in-game sprite objects** (CPS3 "CG" sprites), specifically using **Effect 61** (`eff61.c`).

1. **The Strings**: The literal text strings are hardcoded in a constant array `Menu_Letter_Data` (e.g., `"ARCADE"`, `"SYSTEM DIRECTION"`).
2. **Initialization (`effect_61_init`)**: The initialization routine reads the requested string and iterates character by character to construct a chain of sprite "connections" (`WORK_Other_CONN`). 
3. **The CG Font Maps**: The ASCII characters are not parsed by a font engine; their ASCII integer value is directly mathematically added to a **base CG image number** (either `0x7047` for normal font or `0x70A7` for a smaller font). The letter graphics are permanently burned into the game's actual sprite ROMs starting at these indices.
4. **Formatting Tokens**: The string parser supports basic formatting inline tokens:
   * A space (`' '`) creates an X offset of 14px (or 8px for the small font).
   * A hash (`#`) creates a custom half-space offset.
   * A slash (`/`) creates a carriage return/line break (`nx -= 8; ny += 17;`).
5. **Rendering**: Because they are constructed as standard characters with physics/rendering containers (`WORK_Other`), they submit to the game's main object rendering pipeline every frame via `sort_push_request3(&ewk->wu);` alongside player characters and fireballs, rather than the UI painter's algorithm.

#### Vertical Positioning (`Slide_Pos_Data_61`)
Each menu entry's screen position is controlled by `Slide_Pos_Data_61[char_ix]` in `sel_data.c`, a `{x, y}` table:
*   **Increasing `y` moves the item UP on screen** (same inverted Y-axis as Effect 66).
*   The position is **relative to the background origin** (`bg_w.bgw[my_family-1].wxy`), not absolute pixel coordinates.
*   Network lobby entries occupy indices **67–73** (NETWORK LOBBY title, AUTO-CONN, CONNECT, etc.).
*   When repositioning menu items, any `SSPutStr_Bigger` text rendered alongside (e.g., toggle ON/OFF values) must be updated separately to match, since the two systems use independent coordinate spaces.

---

## 4. Hardware Rendering Abstraction Pipeline

The system is definitively distinct because the game relies exclusively on **indexed palettes**, which modern GPUs do not support natively.

### Step 1: `Renderer_SetTexture`
`Renderer_SetTexture` glues the CPS3 PPG code to the hardware. It extracts the raw `textureId` and the active palette generated by `ppgSetupCurrentPaletteNumber()`. It packs these two bounds into a single 32-bit handle:
```c
u16 palHandle = ppgGetCurrentPaletteHandle();
texCode = (u32)textureId | ((u32)palHandle << 16);
```

### Step 2: GPU/GL Context Decoding
The backend unpacks the texture and palette indices. Since standard 2D textures cannot swap palettes per-draw call dynamically, the engine utilizes a **Virtual Texture Array Layering system**:
*   It looks up a 2D cache array `tex_array_layer[texture][palette]`.
*   If the unique combination hasn't been drawn yet, it allocates a new `layer` in a hardware 3D Texture Array (`GL_TEXTURE_2D_ARRAY` or `SDL_GPUTexture`).

### Step 3: Compute Shader Palettization
*   **OpenGL**: The CPU iterates over the 8-bit or 4-bit indexed `surface->pixels`, looks up colors, and uploads explicit RGBA pixels into the layer map using `glTexSubImage3D`.
*   **SDL_GPU**: The CPU pushes raw indexed data + palette buffer into a Staging Ring Buffer. A dispatched Compute Pipeline executes `s_compute_jobs` directly on the GPU, unpacking into RGBA pixels in VRAM natively to ensure zero main-thread CPU stall.

### Step 4: Batched Quad Rendering
Finally, `Renderer_DrawSprite` routes the vertex coordinates to `draw_quad`. The vertex is injected with the specific `layer` index determined above. A unified Fragment/Pixel Shader takes `uv` + `layer` and outputs the pixel exactly as the original hardware would have with full 60FPS compatibility.
