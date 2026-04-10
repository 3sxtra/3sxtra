#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_MODS

/**
 * @brief Load a high-resolution full-sprite override for a CPS3 character.
 *
 * Looks up assets/sprites/sprite{group}_{cg}.png first, then falls back to
 * assets/sprites/sprite{cg}.png.  Results are cached per cg_number; misses
 * are recorded so the filesystem is only probed once per CG.
 *
 * @param group_index  Texture group index (from obj_group_table[]).
 * @param cg_number    CG pattern number.
 * @return Opaque texture handle (TextureUtil), or NULL if no override exists.
 */
void* LoadFullSpriteOverride(int group_index, int cg_number);

/**
 * @brief Load an HD background tile override.
 *
 * Looks up assets/sprites/bg_{gbix}.png.  Results are cached; misses are
 * @param type   Background texture type (e.g., 0x12 for gameplay, 0x18 for select)
 * @param stage  The current stage number (or 0 for non-gameplay screens).
 * @param gbix   Global background tile index.
 * @return Opaque texture handle (TextureUtil), or NULL if no override exists.
 */
void* LoadBGTileOverride(int type, int stage, int gbix);

/**
 * @brief Clear the background tile override cache (call when stage textures change).
 *
 * Resets cache flags so textures are re-looked up, but does not free textures
 * immediately, as they may remain in use for the current frame.
 */
void ClearBGTileCache(void);

/**
 * @brief Free all cached override textures and reset miss flags.
 *
 * Call during shutdown or when reloading assets.
 */
void SpriteOverride_Shutdown(void);

/**
 * @brief Reset miss flags so previously-missed CG numbers are retried.
 *
 * Call on stage transitions or when assets may have changed.
 * Does not free cached textures — only clears the "not found" flags.
 */
void SpriteOverride_ClearMissFlags(void);

/**
 * @brief Write a CSV listing all CG numbers and BG tiles that had no HD override.
 *
 * Iterates the internal miss arrays and writes missing_sprites.csv next to
 * the executable.  Only produces output when --dump-missing-sprites is set.
 */
void SpriteOverride_DumpMissing(void);

/**
 * @brief Load an HD override for an entire UI texture page.
 *
 * Checks assets/ui/page_{texHandle}_{palHandle}.png.  Results are cached
 * per texHandle|palHandle combo.  On hit, returns an opaque TextureUtil
 * handle (GL texture ID); on miss, returns NULL.
 *
 * @param tex_handle  PPG texture handle (1-based).
 * @param pal_handle  PPG palette handle (0 = no palette).
 * @return Opaque texture handle (TextureUtil), or NULL if no override exists.
 */
void* LoadUIPageOverride(unsigned int tex_handle, unsigned int pal_handle);

#else

static inline void* LoadFullSpriteOverride(int group_index, int cg_number) { (void)group_index; (void)cg_number; return 0; }
static inline void* LoadBGTileOverride(int type, int stage, int gbix) { (void)type; (void)stage; (void)gbix; return 0; }
static inline void ClearBGTileCache(void) {}
static inline void SpriteOverride_Shutdown(void) {}
static inline void SpriteOverride_ClearMissFlags(void) {}
static inline void SpriteOverride_DumpMissing(void) {}
static inline void* LoadUIPageOverride(unsigned int tex_handle, unsigned int pal_handle) { (void)tex_handle; (void)pal_handle; return 0; }

#endif



#ifdef __cplusplus
}
#endif
