# Renderer Plugin System

3SX supports an optional renderer plugin that can override sprites and background tiles with high-resolution PNG replacements at runtime. The plugin is fully backend-agnostic — it works with **all** rendering modes: OpenGL, SDL_GPU, SDL2D, and SDL2D Classic.

## Architecture

The base code defines a plugin interface (`renderer_export_t` / `renderer_import_t`) in [`renderer_plugin.h`](../src/include/port/renderer_plugin.h). At startup, if `--renderer <name>` is specified, the base code loads `<name>.dll` (or `lib<name>.so`) from the executable directory. The plugin receives an import table with engine functions (logging, coordinate conversion, texture load/draw via `TextureUtil`), and returns an export table with the override functions it implements.

If no `--renderer` flag is specified, the game renders normally with zero overhead — all plugin call sites are guarded by `RENDERER_HAS_PLUGIN()`.

## Building

The plugin builds automatically alongside the main executable:

```
cmake -S . -B build -G Ninja
cmake --build build
```

This produces `renderer_hd.dll` (or `librenderer_hd.so` on Linux) in the build directory, which is automatically copied next to the `3sx` executable.

## Usage

```
3sx --renderer renderer_hd --sprites-path /path/to/sprites
```

- `--renderer <name>` — loads plugin DLL from the executable directory.
- `--sprites-path <path>` — parsed by the plugin from the forwarded `argc`/`argv`.

### File naming conventions

Place HD sprite PNGs in the sprites directory:

| Type | Filename | Example |
|------|----------|---------|
| Character sprites | `sprite_{group}_{cg}.png` | `sprite_2_1569.png` |
| Portraits / UI | `sprite_{cg}.png` (fallback) | `sprite_28284.png` |
| Background tiles | `bg_{gbix}.png` | `bg_132.png` |

## Plugin Interface (API v2)

The plugin exports a single function:

```c
renderer_export_t* GetRendererAPI(const renderer_import_t* import);
```

### Export table (`renderer_export_t`)

| Field | Description |
|-------|-------------|
| `api_version` | Must match `RENDERER_PLUGIN_API_VERSION` (2) |
| `Init(argc, argv)` | Called with the application's command-line arguments |
| `Shutdown()` | Called on unload |
| `render_scale` | Desired canvas scale (e.g. 4 for HD) |
| `TryRenderSprite(group, cg, x, y, z, flip_x, color)` | Load + draw a sprite override. Returns `true` if handled. |
| `LoadBGTileOverride(gbix)` | Returns a `void*` texture handle, or `NULL` |
| `DrawBGTile(tex, x, y, w, h, z, vtxCol)` | Draws a background tile at the given screen rect |
| `ClearBGTileCache()` | Called when stage textures change |
| `ClearSpriteCache()` | Called to flush the sprite override cache |

### Import table (`renderer_import_t`)

| Field | Description |
|-------|-------------|
| `Log(fmt, ...)` | Printf-style logging |
| `ConvScreenFZ(z)` | Converts Z depth for render sorting |
| `TextureLoad(path)` | Loads a PNG into a backend-agnostic texture handle |
| `TextureFree(handle)` | Frees a texture |
| `TextureGetSize(handle, &w, &h)` | Queries texture dimensions |
| `TextureDrawQuadEx(handle, x, y, w, h, z, flip_x, flip_y)` | Draws a textured quad; works on all backends |
| `cps3_width` | CPS3 screen width (384) |
| `cps3_height` | CPS3 screen height (224) |

## Writing a Custom Plugin

1. Implement `GetRendererAPI` in a shared library
2. Store the import table pointer
3. Return a populated `renderer_export_t` with `api_version = RENDERER_PLUGIN_API_VERSION`
4. Parse your configuration from `argc`/`argv` in `Init`
5. Use `import->TextureLoad` / `import->TextureDrawQuadEx` for all rendering

Name the output `<name>.dll` (Windows) or `lib<name>.so` (Linux), then launch with `--renderer <name>`.
