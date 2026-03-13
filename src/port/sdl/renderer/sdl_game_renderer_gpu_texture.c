/**
 * @file sdl_game_renderer_gpu_texture.c
 * @brief GPU renderer texture and palette management.
 *
 * Contains texture creation, destruction, upload (with SIMD pixel conversion),
 * palette creation/destruction, and texture dumping — extracted from
 * sdl_game_renderer_gpu.c to reduce file size. All shared state is accessed
 * via the internal header.
 */
#include "sdl_game_renderer_gpu_internal.h"

#include "port/tracy_zones.h"
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"

#include <libgraph.h>
#include <stdio.h>
#include <stdlib.h>

/** @brief Create a CPU-side surface for a game texture (lazy upload). */
void SDLGameRendererGPU_CreateTexture(unsigned int th) {
    const int texture_index = LO_16_BITS(th) - 1;
    if (texture_index < 0 || texture_index >= FL_TEXTURE_MAX)
        return;

    const FLTexture* fl_texture = &flTexture[texture_index];
    const void* pixels = flPS2GetSystemBuffAdrs(fl_texture->mem_handle);
    SDL_PixelFormat pixel_format = SDL_PIXELFORMAT_UNKNOWN;
    int pitch = 0;

    if (surfaces[texture_index]) {
        SDL_DestroySurface(surfaces[texture_index]);
        surfaces[texture_index] = NULL;
    }

    switch (fl_texture->format) {
    case SCE_GS_PSMT8:
        pixel_format = SDL_PIXELFORMAT_INDEX8;
        pitch = fl_texture->width;
        break;
    case SCE_GS_PSMT4:
        pixel_format = SDL_PIXELFORMAT_INDEX4LSB;
        pitch = (fl_texture->width + 1) / 2;
        break;
    case SCE_GS_PSMCT16:
        pixel_format = SDL_PIXELFORMAT_ABGR1555;
        pitch = fl_texture->width * 2;
        break;
    case SCE_GS_PSMCT32:
        pixel_format = SDL_PIXELFORMAT_ABGR8888;
        pitch = fl_texture->width * 4;
        break;
    default:
        return;
    }

    surfaces[texture_index] =
        SDL_CreateSurfaceFrom(fl_texture->width, fl_texture->height, pixel_format, (void*)pixels, pitch);
}

/** @brief Mark a texture for destruction. */
void SDLGameRendererGPU_DestroyTexture(unsigned int texture_handle) {
    const int idx = texture_handle - 1;
    if (idx >= 0 && idx < FL_TEXTURE_MAX) {
        if (surfaces[idx]) {
            SDL_DestroySurface(surfaces[idx]);
            surfaces[idx] = NULL;
        }
        if (!texture_dirty_flags[idx]) {
            texture_dirty_flags[idx] = true;
            dirty_texture_indices[dirty_texture_count++] = idx;
        }
        texture_hash[idx] = 0; // Reset hash on destroy
    }
}

/** @brief Create a CPU-side palette. */
void SDLGameRendererGPU_CreatePalette(unsigned int ph) {
    const int palette_index = HI_16_BITS(ph) - 1;
    if (palette_index < 0 || palette_index >= FL_PALETTE_MAX)
        return;

    const FLTexture* fl_palette = &flPalette[palette_index];
    const void* pixels = flPS2GetSystemBuffAdrs(fl_palette->mem_handle);
    const int color_count = fl_palette->width * fl_palette->height;
    SDL_Color colors[256];
    size_t color_size = (fl_palette->format == SCE_GS_PSMCT32) ? 4 : 2;

    switch (color_count) {
    case 16:
        if (color_size == 4) {
            const Uint32* rgba32 = (const Uint32*)pixels;
            for (int i = 0; i < 16; i++) {
                read_rgba32_color(rgba32[i], &colors[i]);
                colors[i].a = (colors[i].a == 0x80) ? 0xFF : (colors[i].a << 1);
            }
        } else {
            const Uint16* rgba16 = (const Uint16*)pixels;
            for (int i = 0; i < 16; i++)
                read_rgba16_color(rgba16[i], &colors[i]);
        }
        colors[0].a = 0;
        break;
    case 256:
        if (color_size == 4) {
            const Uint32* rgba32 = (const Uint32*)pixels;
            for (int i = 0; i < 256; i++) {
                read_rgba32_color(rgba32[ps2_clut_shuffle[i]], &colors[i]);
                colors[i].a = (colors[i].a == 0x80) ? 0xFF : (colors[i].a << 1);
            }
        } else {
            const Uint16* rgba16 = (const Uint16*)pixels;
            for (int i = 0; i < 256; i++)
                read_rgba16_color(rgba16[ps2_clut_shuffle[i]], &colors[i]);
        }
        colors[0].a = 0;
        break;
    }

    if (palettes[palette_index])
        SDL_DestroyPalette(palettes[palette_index]);
    palettes[palette_index] = SDL_CreatePalette(color_count);
    SDL_SetPaletteColors(palettes[palette_index], colors, 0, color_count);

    // ⚡ Fix: Ensure naturally created palettes (like static PPL UI fonts) are actually
    // queued for upload to the GPU atlas!
    s_palette_uploaded[palette_index] = false;

    bool already_queued = false;
    for (int d = 0; d < s_pal_upload_dirty_count; d++) {
        if (s_pal_upload_dirty_indices[d] == palette_index) {
            already_queued = true;
            break;
        }
    }
    if (!already_queued && s_pal_upload_dirty_count < FL_PALETTE_MAX) {
        s_pal_upload_dirty_indices[s_pal_upload_dirty_count++] = palette_index;
    }
}

/** @brief Mark a palette for destruction. */
void SDLGameRendererGPU_DestroyPalette(unsigned int palette_handle) {
    const int idx = palette_handle - 1;
    if (idx >= 0 && idx < FL_PALETTE_MAX) {
        if (palettes[idx]) {
            SDL_DestroyPalette(palettes[idx]);
            palettes[idx] = NULL;
        }
        if (!palette_dirty_flags[idx]) {
            palette_dirty_flags[idx] = true;
            dirty_palette_indices[dirty_palette_count++] = idx;
        }
        palette_hash[idx] = 0; // Reset hash on destroy
    }
}

void SDLGameRendererGPU_DumpTextures(void) {
    SDL_CreateDirectory("textures");
    int tex_index = 0;
    int count = 0;

    // tex_array_layer[ti] >= 0 means this texture is in the array
    for (int ti = 0; ti < FL_TEXTURE_MAX; ti++) {
        SDL_Surface* surf = surfaces[ti];
        if (!surf || !SDL_ISPIXELFORMAT_INDEXED(surf->format))
            continue;
        if (tex_array_layer[ti] < 0)
            continue;

        for (int pi = 1; pi <= FL_PALETTE_MAX; pi++) {
            SDL_Palette* pal = palettes[pi - 1];
            if (!pal)
                continue;

            char filename[128];
            snprintf(filename, sizeof(filename), "textures/%d_t%d_p%d.tga", tex_index++, ti, pi);
            FILE* f = fopen(filename, "wb");
            if (!f)
                continue;

            const Uint8* pixels = (const Uint8*)surf->pixels;
            const int w = surf->w, h = surf->h;
            uint8_t header[18] = { 0 };
            header[2] = 2;
            header[12] = w & 0xFF;
            header[13] = (w >> 8) & 0xFF;
            header[14] = h & 0xFF;
            header[15] = (h >> 8) & 0xFF;
            header[16] = 32;
            header[17] = 0x20;
            fwrite(header, 1, 18, f);

            for (int i = 0; i < w * h; i++) {
                Uint8 idx;
                if (pal->ncolors == 16) {
                    Uint8 byte = pixels[i / 2];
                    idx = (i & 1) ? (byte >> 4) : (byte & 0x0F);
                } else {
                    idx = pixels[i];
                }
                // GPU read_rgba32_color: R→.r, G→.g, B→.b (correct SDL convention)
                // TGA stores BGRA in memory, so write { b, g, r, a }
                const SDL_Color* c = &pal->colors[idx];
                Uint8 bgra[] = { c->b, c->g, c->r, c->a };
                fwrite(bgra, 1, 4, f);
            }
            fclose(f);
            count++;
        }
    }

    SDL_Log("[TextureDump] Wrote %d texture(s) to textures/", count);
}

void SDLGameRendererGPU_UnlockTexture(unsigned int th) {
    const int idx = th - 1;
    if (idx >= 0 && idx < FL_TEXTURE_MAX) {
        // 1D: free the single layer for this texture
        if (tex_array_layer[idx] >= 0) {
            tex_array_free[tex_array_free_count++] = tex_array_layer[idx];
            tex_array_layer[idx] = -1;
        }
    }
}

void SDLGameRendererGPU_UnlockPalette(unsigned int ph) {
    const int idx = ph - 1;
    if (idx >= 0 && idx < FL_PALETTE_MAX) {
        // Palette changes don't invalidate indexed texture array layers —
        // the palette row is looked up separately by the fragment shader.
        SDLGameRendererGPU_CreatePalette((idx + 1) << 16);
    }
}

/** @brief Prepare a texture for rendering, uploading to the GPU array if needed. */
void SDLGameRendererGPU_SetTexture(unsigned int th) {
    if ((th & 0xFFFF) == 0)
        th = (th & 0xFFFF0000) | 1000;

    // ⚡ Back-to-back early-out: if the same handle was just set, re-push
    // the cached layer/UV without re-doing the lookup or staging work.
    // NOTE: Trace zone intentionally placed BELOW this check — the early-out
    // path fires ~80% of the time, and the trace overhead was ~1.25s/session.
    if (th == s_last_set_texture_handle && texture_count > 0) {
        if (texture_count < MAX_VERTICES) {
            texture_layers[texture_count] = texture_layers[texture_count - 1];
            texture_uv_sx[texture_count] = texture_uv_sx[texture_count - 1];
            texture_uv_sy[texture_count] = texture_uv_sy[texture_count - 1];
            texture_palette_idx[texture_count] = texture_palette_idx[texture_count - 1];
            texture_count++;
        }
        return;
    }
    s_last_set_texture_handle = th;
    TRACE_ZONE_N("GPU:SetTexture");

    const int texture_handle = LO_16_BITS(th);
    const int palette_handle = HI_16_BITS(th);

    if (texture_handle < 1 || texture_handle > FL_TEXTURE_MAX) {
        TRACE_ZONE_END();
        return;
    }

    if (!surfaces[texture_handle - 1]) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Texture %d has no surface!", texture_handle);
        TRACE_ZONE_END();
        return;
    }

    int layer = tex_array_layer[texture_handle - 1]; // 1D: keyed by texture only

    if (layer < 0) {
        // ⚡ Opt10b: Lazily map the staging buffer on first texture cache miss this frame.
        if (tex_array_free_count > 0 && !s_compute_staging_ptr) {
            s_compute_staging_ptr = (u8*)SDL_MapGPUTransferBuffer(device, s_compute_staging_buffer, true);
            s_compute_staging_offset = 0;
        }
        if (tex_array_free_count > 0 && s_compute_staging_ptr) {
            layer = tex_array_free[--tex_array_free_count];
            tex_array_layer[texture_handle - 1] = layer; // 1D

            SDL_Surface* surface = surfaces[texture_handle - 1];
            const FLTexture* fl_texture = &flTexture[texture_handle - 1];

            // Upload as RGBA8 (4 bytes/pixel) to staging buffer
            int w = surface->w;
            int h = surface->h;
            size_t rgba_size = (size_t)w * h * 4;

            if (s_tex_upload_count < MAX_COMPUTE_JOBS && s_compute_staging_offset + rgba_size <= COMPUTE_STORAGE_SIZE) {

                Uint32 out_offset = (Uint32)s_compute_staging_offset;
                u32* dst = (u32*)(s_compute_staging_ptr + s_compute_staging_offset);

                if (fl_texture->format == SCE_GS_PSMT4) {
                    // ⚡ Bolt: SIMD 4-bit indexed → RGBA32 (palette index in R channel)
                    // Process 16 input bytes (32 pixels) at a time via nibble unpack.
                    const u8* src = (const u8*)surface->pixels;
                    int pitch = surface->pitch;
                    const simde__m128i alpha = simde_mm_set1_epi32((int)0xFF000000u);
                    const simde__m128i zero = simde_mm_setzero_si128();
                    const simde__m128i mask = simde_mm_set1_epi8(0x0F);
                    for (int y = 0; y < h; y++) {
                        const u8* row = src + y * pitch;
                        u32* out_row = dst + y * w;
                        int x = 0;
                        for (; x + 31 < w; x += 32) {
                            // Load 16 bytes (32 pixels, each 4 bits)
                            simde__m128i bytes = simde_mm_loadu_si128((const simde__m128i*)(row + x / 2));
                            // Extract low nibbles line
                            simde__m128i lo_nibbles = simde_mm_and_si128(bytes, mask);
                            // Extract high nibbles line
                            simde__m128i hi_nibbles = simde_mm_and_si128(simde_mm_srli_epi16(bytes, 4), mask);
                            // Interleave lo and hi so we get [lo, hi, lo, hi...]
                            // Example: byte0=(h0<<4)|l0 -> we want l0, h0.
                            // unpacklo/hi_epi8 interleaves byte-by-byte.
                            simde__m128i interleaved_lo = simde_mm_unpacklo_epi8(lo_nibbles, hi_nibbles);
                            simde__m128i interleaved_hi = simde_mm_unpackhi_epi8(lo_nibbles, hi_nibbles);
                            // Now interleaved_lo has the first 16 pixels (as 8-bit values),
                            // interleaved_hi has the next 16 pixels.
                            // Expand to 32-bit and OR with 0xFF000000
                            // 1) First 8 pixels from interleaved_lo
                            simde__m128i a0 = simde_mm_unpacklo_epi8(interleaved_lo, zero);
                            simde__m128i a1 = simde_mm_unpackhi_epi8(interleaved_lo, zero);
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + 0),
                                                  simde_mm_or_si128(simde_mm_unpacklo_epi16(a0, zero), alpha));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + 4),
                                                  simde_mm_or_si128(simde_mm_unpackhi_epi16(a0, zero), alpha));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + 8),
                                                  simde_mm_or_si128(simde_mm_unpacklo_epi16(a1, zero), alpha));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + 12),
                                                  simde_mm_or_si128(simde_mm_unpackhi_epi16(a1, zero), alpha));
                            // 2) Next 8 pixels from interleaved_hi
                            simde__m128i a2 = simde_mm_unpacklo_epi8(interleaved_hi, zero);
                            simde__m128i a3 = simde_mm_unpackhi_epi8(interleaved_hi, zero);
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + 16),
                                                  simde_mm_or_si128(simde_mm_unpacklo_epi16(a2, zero), alpha));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + 20),
                                                  simde_mm_or_si128(simde_mm_unpackhi_epi16(a2, zero), alpha));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + 24),
                                                  simde_mm_or_si128(simde_mm_unpacklo_epi16(a3, zero), alpha));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + 28),
                                                  simde_mm_or_si128(simde_mm_unpackhi_epi16(a3, zero), alpha));
                        }
                        // Scalar tail
                        for (; x < w; x += 2) {
                            u8 byte = row[x / 2];
                            out_row[x] = 0xFF000000u | (u32)(byte & 0x0F);
                            if (x + 1 < w)
                                out_row[x + 1] = 0xFF000000u | (u32)((byte >> 4) & 0x0F);
                        }
                    }
                } else if (fl_texture->format == SCE_GS_PSMCT16) {
                    // ⚡ Bolt: SIMD 16-bit direct color → RGBA32
                    // Process 8 pixels at a time using integer SIMD for bit extraction.
                    const u8* src = (const u8*)surface->pixels;
                    int pitch = surface->pitch;
                    const simde__m128i mask5 = simde_mm_set1_epi32(0x1F);
                    const simde__m128i mask_a = simde_mm_set1_epi32(0x8000);
                    const simde__m128i alpha_ff = simde_mm_set1_epi32((int)0xFF000000u);
                    const simde__m128i zero = simde_mm_setzero_si128();
                    for (int y = 0; y < h; y++) {
                        const u16* row = (const u16*)(src + y * pitch);
                        u32* out_row = dst + y * w;
                        int x = 0;
                        for (; x + 7 < w; x += 8) {
                            // Load 8 u16 pixels and expand to 2×4 u32
                            simde__m128i px = simde_mm_loadu_si128((const simde__m128i*)(row + x));
                            simde__m128i lo32 = simde_mm_unpacklo_epi16(px, zero);
                            simde__m128i hi32 = simde_mm_unpackhi_epi16(px, zero);
                            // Process lo32 (4 pixels)
                            for (int half = 0; half < 2; half++) {
                                simde__m128i v = (half == 0) ? lo32 : hi32;
                                simde__m128i r = simde_mm_and_si128(v, mask5);                          // R: bits 0-4
                                simde__m128i g = simde_mm_and_si128(simde_mm_srli_epi32(v, 5), mask5);  // G: bits 5-9
                                simde__m128i b = simde_mm_and_si128(simde_mm_srli_epi32(v, 10), mask5); // B: bits 10-14
                                simde__m128i a = simde_mm_and_si128(v, mask_a);                         // A: bit 15
                                // Scale 5-bit to 8-bit: val * 255 / 31 ≈ (val * 8 + val / 4) = (val << 3) | (val >> 2)
                                r = simde_mm_or_si128(simde_mm_slli_epi32(r, 3), simde_mm_srli_epi32(r, 2));
                                g = simde_mm_or_si128(simde_mm_slli_epi32(g, 3), simde_mm_srli_epi32(g, 2));
                                b = simde_mm_or_si128(simde_mm_slli_epi32(b, 3), simde_mm_srli_epi32(b, 2));
                                // Alpha: 0x8000 → 0xFF000000, 0 → 0
                                a = simde_mm_and_si128(simde_mm_cmpeq_epi32(a, mask_a), alpha_ff);
                                // Pack: a | (b << 16) | (g << 8) | r
                                simde__m128i result = simde_mm_or_si128(a, r);
                                result = simde_mm_or_si128(result, simde_mm_slli_epi32(g, 8));
                                result = simde_mm_or_si128(result, simde_mm_slli_epi32(b, 16));
                                simde_mm_storeu_si128((simde__m128i*)(out_row + x + half * 4), result);
                            }
                        }
                        // Scalar tail
                        for (; x < w; x++) {
                            SDL_Color c;
                            read_rgba16_color(row[x], &c);
                            out_row[x] = (c.a << 24) | (c.b << 16) | (c.g << 8) | c.r;
                        }
                    }
                } else if (fl_texture->format == SCE_GS_PSMT8) {
                    // ⚡ Bolt: SIMD 8-bit indexed → RGBA32 (palette index in R channel)
                    // Process 16 pixels at a time: load 16 u8 → expand to 4×__m128i of u32 → OR with 0xFF000000.
                    const u8* src = (const u8*)surface->pixels;
                    int pitch = surface->pitch;
                    const simde__m128i alpha = simde_mm_set1_epi32((int)0xFF000000u);
                    const simde__m128i zero = simde_mm_setzero_si128();
                    for (int y = 0; y < h; y++) {
                        const u8* row = src + y * pitch;
                        u32* out_row = dst + y * w;
                        int x = 0;
                        // Process 16 bytes (pixels) at a time
                        for (; x + 15 < w; x += 16) {
                            simde__m128i bytes = simde_mm_loadu_si128((const simde__m128i*)(row + x));
                            // Expand bytes → u16 (2 groups)
                            simde__m128i w0 = simde_mm_unpacklo_epi8(bytes, zero);
                            simde__m128i w1 = simde_mm_unpackhi_epi8(bytes, zero);
                            // Expand u16 → u32 (4 groups) and OR with alpha mask
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + 0),
                                                  simde_mm_or_si128(simde_mm_unpacklo_epi16(w0, zero), alpha));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + 4),
                                                  simde_mm_or_si128(simde_mm_unpackhi_epi16(w0, zero), alpha));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + 8),
                                                  simde_mm_or_si128(simde_mm_unpacklo_epi16(w1, zero), alpha));
                            simde_mm_storeu_si128((simde__m128i*)(out_row + x + 12),
                                                  simde_mm_or_si128(simde_mm_unpackhi_epi16(w1, zero), alpha));
                        }
                        // Scalar tail
                        for (; x < w; x++) {
                            out_row[x] = 0xFF000000u | (u32)row[x];
                        }
                    }
                } else {
                    // 32-bit direct (no palette, no conversion needed)
                    memcpy(dst, surface->pixels, rgba_size);
                }

                s_compute_staging_offset += rgba_size;
                // ⚡ Vulkan/SDL_GPU requires copy offsets to be highly aligned (typically 256/512 bytes)
                s_compute_staging_offset = (s_compute_staging_offset + 511) & ~511;

                TextureUploadJob* job = &s_tex_upload_jobs[s_tex_upload_count++];
                job->width = w;
                job->height = h;
                job->layer = layer;
                job->offset = out_offset;
            } else {
                s_compute_drops_last_frame++;
                tex_array_free[tex_array_free_count++] = layer;
                tex_array_layer[texture_handle - 1] = -1; // 1D
                layer = -1;
            }
        }
    }

    if (texture_count >= MAX_VERTICES) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Texture stack overflow!");
        return;
    }

    if (layer < 0)
        layer = 0;

    // Determine palette index for the palette atlas.
    // Only enable palette lookup for INDEXED textures (PSMT4/PSMT8) with a valid palette.
    // Direct-color textures (PSMCT16, 32-bit) always get palIdx = -1 even if palette_handle != 0.
    const FLTexture* fl_tex_info = &flTexture[texture_handle - 1];
    bool is_indexed_format = (fl_tex_info->format == SCE_GS_PSMT4 || fl_tex_info->format == SCE_GS_PSMT8);
    float palIdx = -1.0f;
    if (is_indexed_format) {
        // If no palette assigned, fall back to the identity grayscale ramp at DEFAULT_PALETTE_ROW.
        // Using row 0 samples an uninitialized/wrong palette for native UI (main menu, HUD, etc.).
        palIdx = (palette_handle > 0) ? (float)(palette_handle - 1) : (float)DEFAULT_PALETTE_ROW;
    }

    texture_layers[texture_count] = layer;
    // ⚡ Opt10c: Use fl_tex_info (already loaded) instead of pointer-chasing through surfaces[].
    // Multiply by reciprocal constant instead of dividing by TEX_ARRAY_SIZE.
    texture_uv_sx[texture_count] = (float)fl_tex_info->width * (1.0f / TEX_ARRAY_SIZE);
    texture_uv_sy[texture_count] = (float)fl_tex_info->height * (1.0f / TEX_ARRAY_SIZE);
    texture_palette_idx[texture_count] = palIdx;
    texture_count++;
    TRACE_ZONE_END();
}
