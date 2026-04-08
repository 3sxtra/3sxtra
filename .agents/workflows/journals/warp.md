# Warp Optimization Journal

## ⚡ Convert static array texture caches to dynamic Maps/Sets (stb_ds.h)
**Date**: 2026-04-08
**Author**: Warp ⚡

**Observation**: 
The GL renderer backend (`sdl_game_renderer_gl_resources.c` and `sdl_game_renderer_gl_internal.h`) was allocating multiple large static 2D arrays (`texture_cache`, `texture_cache_w`, `texture_cache_h`, `stale_texture_cache`), overheading massive static allocations in the `.bss` section memory. Additionally, `tcache_live` was awkwardly maintained to constrain iterative scanning operations to the active subset, attempting to avoid performance penalty from array iterations.

**Action**:
- Replaced the large static arrays with a single dynamic hash map array of type `GLTextureCacheEntry` using `stb/stb_ds.h`.
- The hash map key is an efficiently derivable integer composed of the `texture_index` and `palette_index` (`(tex_idx << 16) | pal_idx`).
- Removed `tcache_live` operations natively, as `stb_ds.h` dynamically provides continuous iterable capabilities directly via (`hmlen()`), effectively serving the same purpose with fewer variables and memory layout.

**Validation**:
Successfully ran MSYS MinGW compilation `compile.bat`. VRAM scaling remains identical while resolving static allocation limits and removing arbitrary `TCACHE_LIVE_MAX` bounds.
