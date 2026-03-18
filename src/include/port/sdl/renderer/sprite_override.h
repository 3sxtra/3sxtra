#pragma once

#ifdef __cplusplus
extern "C" {
#endif

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
 * recorded so the filesystem is only probed once per gbix.
 *
 * @param gbix  Global background tile index.
 * @return Opaque texture handle (TextureUtil), or NULL if no override exists.
 */
void* LoadBGTileOverride(int gbix);

/**
 * @brief Free all cached override textures and reset miss flags.
 *
 * Call during shutdown or when reloading assets.
 */
void SpriteOverride_Shutdown(void);

#ifdef __cplusplus
}
#endif
