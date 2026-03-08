/**
 * @file sdl_game_renderer_sdl_sw.c
 * @brief CPU-side software rasterizer for the SDL2D renderer backend.
 */
#include "common.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/renderer/sdl_game_renderer.h"
#include "port/sdl/renderer/sdl_game_renderer_internal.h"
#include "port/tracy_zones.h"
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include <SDL3/SDL.h>

#include "port/sdl/renderer/sdl_game_renderer_sdl_sw.h"

// ⚡ Software-frame rendering state — CPU-side compositing into a single surface.
// Uses RGBA8888 to match LRU cache pixel format — zero format conversion.
static SDL_Surface* sw_frame_surface = NULL;    // 384×224 RGBA8888 compositing target
static SDL_Texture* sw_frame_upload_tex = NULL; // Streaming texture for single-upload to GPU

// ⚡ Dirty tile tracking — 16×16 tile grid over 384×224 framebuffer.
// Only tiles touched by current OR previous frame need clearing/redrawing.
// Saves memset + compositing cost on static screens (menus, pause).
enum {
    DT_SIZE = 16,
    DT_COLS = 24,                 // 384 / 16
    DT_ROWS = 14,                 // 224 / 16
    DT_TOTAL = DT_COLS * DT_ROWS, // 336
};
static uint8_t dt_current[DT_TOTAL];     // tiles covered this frame
static uint8_t dt_previous[DT_TOTAL];    // tiles covered last frame
static uint32_t dt_prev_clear_color = 0; // previous frame's RGBA8888 clear color
static bool dt_prev_clear_valid = false; // whether dt_prev_clear_color is initialized

// ⚡ Mark all tiles overlapping a screen-space rect as dirty in dt_current[].
static void dt_mark_rect(float fx, float fy, float fw, float fh) {
    int c0 = (int)SDL_floorf(fx) / DT_SIZE;
    int r0 = (int)SDL_floorf(fy) / DT_SIZE;
    int c1 = (int)SDL_ceilf(fx + fw - 1.0f) / DT_SIZE;
    int r1 = (int)SDL_ceilf(fy + fh - 1.0f) / DT_SIZE;
    if (c0 < 0)
        c0 = 0;
    if (c0 > DT_COLS - 1)
        c0 = DT_COLS - 1;
    if (r0 < 0)
        r0 = 0;
    if (r0 > DT_ROWS - 1)
        r0 = DT_ROWS - 1;
    if (c1 < 0)
        c1 = 0;
    if (c1 > DT_COLS - 1)
        c1 = DT_COLS - 1;
    if (r1 < 0)
        r1 = 0;
    if (r1 > DT_ROWS - 1)
        r1 = DT_ROWS - 1;
    for (int r = r0; r <= r1; r++) {
        for (int c = c0; c <= c1; c++) {
            dt_current[r * DT_COLS + c] = 1;
        }
    }
}
// ⚡ Exact divide by 255 using shifts only — no UDIV instruction.
// Correct for all x ∈ [0, 65534], which covers all u8×u8 products.
#define DIV255(x) (((x) + 1u + (((x) >> 8) & 0xFFu)) >> 8)

// ⚡ Color modulate: channel-wise multiply two RGBA8888 pixels.
static inline uint32_t sw_modulate_rgba8888(uint32_t pixel, uint32_t color) {
    const uint32_t src_r = (pixel >> 24) & 0xFFu;
    const uint32_t src_g = (pixel >> 16) & 0xFFu;
    const uint32_t src_b = (pixel >> 8) & 0xFFu;
    const uint32_t src_a = pixel & 0xFFu;
    const uint32_t mod_r = (color >> 24) & 0xFFu;
    const uint32_t mod_g = (color >> 16) & 0xFFu;
    const uint32_t mod_b = (color >> 8) & 0xFFu;
    const uint32_t mod_a = color & 0xFFu;
    return (((src_r * mod_r + 127u) / 255u) << 24) | (((src_g * mod_g + 127u) / 255u) << 16) |
           (((src_b * mod_b + 127u) / 255u) << 8) | ((src_a * mod_a + 127u) / 255u);
}

// ⚡ Alpha composite (src-over): early-out for α=0/255, full Porter-Duff otherwise.
// Both pixels are RGBA8888: R<<24 | G<<16 | B<<8 | A
static inline uint32_t sw_blend_rgba8888(uint32_t dst_pixel, uint32_t src_pixel) {
    const uint32_t src_a = src_pixel & 0xFFu;
    if (src_a == 0u)
        return dst_pixel;
    if (src_a == 255u)
        return src_pixel;

    const uint32_t dst_a = dst_pixel & 0xFFu;
    const uint32_t inv_src_a = 255u - src_a;
    const uint32_t out_a = src_a + ((dst_a * inv_src_a + 127u) / 255u);
    if (out_a == 0u)
        return 0u;

    const uint32_t src_r = (src_pixel >> 24) & 0xFFu;
    const uint32_t src_g = (src_pixel >> 16) & 0xFFu;
    const uint32_t src_b = (src_pixel >> 8) & 0xFFu;
    const uint32_t dst_r = (dst_pixel >> 24) & 0xFFu;
    const uint32_t dst_g = (dst_pixel >> 16) & 0xFFu;
    const uint32_t dst_b = (dst_pixel >> 8) & 0xFFu;

    // Premultiplied src-over compositing, then un-premultiply
    const uint32_t out_r = ((src_r * src_a + dst_r * dst_a * inv_src_a / 255u) + out_a / 2u) / out_a;
    const uint32_t out_g = ((src_g * src_a + dst_g * dst_a * inv_src_a / 255u) + out_a / 2u) / out_a;
    const uint32_t out_b = ((src_b * src_a + dst_b * dst_a * inv_src_a / 255u) + out_a / 2u) / out_a;
    return (out_r << 24) | (out_g << 16) | (out_b << 8) | out_a;
}
// ⚡ Clamp integer to [lo, hi]
static inline int sw_clamp(int val, int lo, int hi) {
    if (val < lo)
        return lo;
    if (val > hi)
        return hi;
    return val;
}
// ⚡ Pre-multiplied solid blend (ported from MiSTer's blend_solid_argb8888).
// Pre-computes src channel premul values ONCE, then applies per-pixel.
// Fast path for dst_a==255 (opaque destination — the common case).
// All channels in RGBA8888 layout: R<<24|G<<16|B<<8|A
static inline uint32_t sw_blend_solid_rgba8888(uint32_t dst_pixel, uint32_t src_a, uint32_t inv_src_a,
                                               uint32_t src_r_premul, uint32_t src_g_premul, uint32_t src_b_premul) {
    const uint32_t dst_a = dst_pixel & 0xFFu;
    const uint32_t dst_r = (dst_pixel >> 24) & 0xFFu;
    const uint32_t dst_g = (dst_pixel >> 16) & 0xFFu;
    const uint32_t dst_b = (dst_pixel >> 8) & 0xFFu;

    if (dst_a == 255u) {
        // Fast path: opaque destination — no alpha compositing needed
        const uint32_t out_r = (src_r_premul + (dst_r * inv_src_a) + 127u) / 255u;
        const uint32_t out_g = (src_g_premul + (dst_g * inv_src_a) + 127u) / 255u;
        const uint32_t out_b = (src_b_premul + (dst_b * inv_src_a) + 127u) / 255u;
        return (out_r << 24) | (out_g << 16) | (out_b << 8) | 0xFFu;
    }

    const uint32_t out_a = src_a + ((dst_a * inv_src_a + 127u) / 255u);
    if (out_a == 0u)
        return 0u;

    const uint32_t dst_r_premul = dst_r * dst_a;
    const uint32_t dst_g_premul = dst_g * dst_a;
    const uint32_t dst_b_premul = dst_b * dst_a;
    const uint32_t out_r = ((src_r_premul + ((dst_r_premul * inv_src_a + 127u) / 255u)) + (out_a / 2u)) / out_a;
    const uint32_t out_g = ((src_g_premul + ((dst_g_premul * inv_src_a + 127u) / 255u)) + (out_a / 2u)) / out_a;
    const uint32_t out_b = ((src_b_premul + ((dst_b_premul * inv_src_a + 127u) / 255u)) + (out_a / 2u)) / out_a;
    return (out_r << 24) | (out_g << 16) | (out_b << 8) | out_a;
}
#define LERP_FLOAT(a, b, x) ((a) * (1.0f - (x)) + (b) * (x))

void lerp_fcolors(SDL_FColor* dest, const SDL_FColor* a, const SDL_FColor* b, float x) {
    dest->r = LERP_FLOAT(a->r, b->r, x);
    dest->g = LERP_FLOAT(a->g, b->g, x);
    dest->b = LERP_FLOAT(a->b, b->b, x);
    dest->a = LERP_FLOAT(a->a, b->a, x);
}

void SWRaster_Init(void) {}

void SWRaster_Shutdown(void) {
    if (sw_frame_surface != NULL) {
        SDL_DestroySurface(sw_frame_surface);
        sw_frame_surface = NULL;
    }
    if (sw_frame_upload_tex != NULL) {
        SDL_DestroyTexture(sw_frame_upload_tex);
        sw_frame_upload_tex = NULL;
    }
}

// --- Software-Frame Lifecycle ---

static bool ensure_sw_frame_surface(const SWRaster_Context* ctx) {
    if (sw_frame_surface != NULL)
        return true;
    sw_frame_surface = SDL_CreateSurface(ctx->canvas_w, ctx->canvas_h, SDL_PIXELFORMAT_RGBA8888);
    return sw_frame_surface != NULL;
}

static bool ensure_sw_frame_upload_texture(const SWRaster_Context* ctx) {
    if (sw_frame_upload_tex != NULL)
        return true;
    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    sw_frame_upload_tex = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, ctx->canvas_w, ctx->canvas_h);
    if (!sw_frame_upload_tex)
        return false;
    SDL_SetTextureScaleMode(sw_frame_upload_tex, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(sw_frame_upload_tex, SDL_BLENDMODE_NONE);
    return true;
}

// ⚡ Software-rasterize a textured rect task into the software-frame surface.
static bool sw_raster_textured(const SWRaster_Context* ctx, int task_idx) {
    const unsigned int th = ctx->th[task_idx];
    const int tex_handle = LO_16_BITS(th);
    const int pal_handle = HI_16_BITS(th);
    if (tex_handle <= 0 || tex_handle > FL_TEXTURE_MAX)
        return false;
    const int ti = tex_handle - 1;

    int src_tex_w, src_tex_h;
    const uint32_t* src_pixels = NULL;

    if (pal_handle > 0) {
        src_pixels = ctx->lookup_cached_pixels(ti, pal_handle, &src_tex_w, &src_tex_h);
    } else {
        src_pixels = ctx->ensure_nonidx_pixels(ti, &src_tex_w, &src_tex_h);
    }
    if (!src_pixels)
        return false;

    const SDL_FRect* dst_r = &ctx->dst_rect[task_idx];
    const SDL_FRect* src_uv = &ctx->src_rect[task_idx];
    const SDL_FlipMode flip = ctx->flip[task_idx];
    const uint32_t color = ctx->color32[task_idx];

    // Convert UV to pixel coords
    const int src_x = (int)SDL_roundf(src_uv->x * (float)src_tex_w);
    const int src_y = (int)SDL_roundf(src_uv->y * (float)src_tex_h);
    const int src_w = (int)SDL_roundf(src_uv->w * (float)src_tex_w);
    const int src_h = (int)SDL_roundf(src_uv->h * (float)src_tex_h);
    if (src_w <= 0 || src_h <= 0)
        return true; // degenerate — skip

    const int dst_x = (int)SDL_roundf(dst_r->x);
    const int dst_y = (int)SDL_roundf(dst_r->y);
    const int dst_w = (int)SDL_roundf(dst_r->w);
    const int dst_h = (int)SDL_roundf(dst_r->h);
    if (dst_w <= 0 || dst_h <= 0)
        return true; // degenerate — skip

    // Clamp destination to surface bounds
    const int dst_x0 = sw_clamp(dst_x, 0, ctx->canvas_w);
    const int dst_y0 = sw_clamp(dst_y, 0, ctx->canvas_h);
    const int dst_x1 = sw_clamp(dst_x + dst_w, 0, ctx->canvas_w);
    const int dst_y1 = sw_clamp(dst_y + dst_h, 0, ctx->canvas_h);
    if (dst_x1 <= dst_x0 || dst_y1 <= dst_y0)
        return true; // fully clipped

    uint32_t* dst_pixels = (uint32_t*)sw_frame_surface->pixels;
    const int dst_pitch = sw_frame_surface->pitch / (int)sizeof(uint32_t);
    const bool flip_h = (flip & SDL_FLIP_HORIZONTAL) != 0;
    const bool flip_v = (flip & SDL_FLIP_VERTICAL) != 0;
    const bool has_color_mod = (color != 0xFFFFFFFFu);

    if (src_w == dst_w && src_h == dst_h) {
        // ⚡ Exact copy path: 1:1 pixel mapping with flip/clip/color-mod support
        const int clip_left = dst_x0 - dst_x;
        const int clip_top = dst_y0 - dst_y;
        const int src_x_step = flip_h ? -1 : 1;
        const int src_y_step = flip_v ? -1 : 1;
        const int src_start_x = flip_h ? (src_x + src_w - 1 - clip_left) : (src_x + clip_left);
        const int src_start_y = flip_v ? (src_y + src_h - 1 - clip_top) : (src_y + clip_top);

        for (int row = 0; row < (dst_y1 - dst_y0); row++) {
            const int sy = src_start_y + row * src_y_step;
            if (sy < 0 || sy >= src_tex_h)
                continue;
            const uint32_t* src_row = src_pixels + sy * src_tex_w;
            uint32_t* dst_row = dst_pixels + (dst_y0 + row) * dst_pitch + dst_x0;
            int sx = src_start_x;
            for (int col = 0; col < (dst_x1 - dst_x0); col++) {
                if (sx >= 0 && sx < src_tex_w) {
                    uint32_t px = src_row[sx];
                    if (has_color_mod)
                        px = sw_modulate_rgba8888(px, color);
                    dst_row[col] = sw_blend_rgba8888(dst_row[col], px);
                }
                sx += src_x_step;
            }
        }
    } else {
        // ⚡ Scaled copy path: pre-computed LUT eliminates float UV math per pixel.
        // Ported from MiSTer's populate_scaled_lookup_table + try_fast_copy_fast_textured_task.
        const int visible_w = dst_x1 - dst_x0;
        const int visible_h = dst_y1 - dst_y0;

        // Pre-compute per-destination-pixel source coordinate lookup tables
        int src_x_lut[384]; // max ctx->canvas_w
        int src_y_lut[224]; // max ctx->canvas_h
        for (int i = 0; i < visible_w; i++) {
            const int dst_off = (dst_x0 + i) - dst_x;
            const int src_off = (((dst_off * 2) + 1) * src_w) / (dst_w * 2);
            src_x_lut[i] = sw_clamp(flip_h ? (src_x + src_w - 1 - src_off) : (src_x + src_off), 0, src_tex_w - 1);
        }
        for (int i = 0; i < visible_h; i++) {
            const int dst_off = (dst_y0 + i) - dst_y;
            const int src_off = (((dst_off * 2) + 1) * src_h) / (dst_h * 2);
            src_y_lut[i] = sw_clamp(flip_v ? (src_y + src_h - 1 - src_off) : (src_y + src_off), 0, src_tex_h - 1);
        }

        for (int row = 0; row < visible_h; row++) {
            const uint32_t* src_row = src_pixels + src_y_lut[row] * src_tex_w;
            uint32_t* dst_row = dst_pixels + (dst_y0 + row) * dst_pitch + dst_x0;
            for (int col = 0; col < visible_w; col++) {
                uint32_t px = src_row[src_x_lut[col]];
                if (has_color_mod)
                    px = sw_modulate_rgba8888(px, color);
                dst_row[col] = sw_blend_rgba8888(dst_row[col], px);
            }
        }
    }
    return true;
}

// ⚡ Software-rasterize a solid color rect task.
// task_dst_rect and task_color32 must be populated before calling.
static bool sw_raster_solid(const SWRaster_Context* ctx, int task_idx) {
    const SDL_FRect* dst_r = &ctx->dst_rect[task_idx];
    const uint32_t color = ctx->color32[task_idx]; // RGBA8888 format
    const uint32_t src_a = color & 0xFFu;          // Alpha is low byte in RGBA8888
    if (src_a == 0u)
        return true; // fully transparent — skip

    const int x0 = sw_clamp((int)SDL_floorf(dst_r->x), 0, ctx->canvas_w);
    const int y0 = sw_clamp((int)SDL_floorf(dst_r->y), 0, ctx->canvas_h);
    const int x1 = sw_clamp((int)SDL_ceilf(dst_r->x + dst_r->w), 0, ctx->canvas_w);
    const int y1 = sw_clamp((int)SDL_ceilf(dst_r->y + dst_r->h), 0, ctx->canvas_h);
    if (x1 <= x0 || y1 <= y0)
        return true;

    uint32_t* dst_pixels = (uint32_t*)sw_frame_surface->pixels;
    const int dst_pitch = sw_frame_surface->pitch / (int)sizeof(uint32_t);
    const int fill_w = x1 - x0;

    if (src_a == 255u) {
        // Opaque fill — direct write
        for (int y = y0; y < y1; y++) {
            uint32_t* row = dst_pixels + y * dst_pitch + x0;
            for (int i = 0; i < fill_w; i++)
                row[i] = color;
        }
    } else {
        // ⚡ Semi-transparent — pre-multiplied solid blend (MiSTer optimization).
        // Pre-compute src channel premul values ONCE outside the pixel loop.
        const uint32_t inv_src_a = 255u - src_a;
        const uint32_t src_r = (color >> 24) & 0xFFu;
        const uint32_t src_g = (color >> 16) & 0xFFu;
        const uint32_t src_b = (color >> 8) & 0xFFu;
        const uint32_t src_r_premul = src_r * src_a;
        const uint32_t src_g_premul = src_g * src_a;
        const uint32_t src_b_premul = src_b * src_a;
        for (int y = y0; y < y1; y++) {
            uint32_t* row = dst_pixels + y * dst_pitch + x0;
            for (int i = 0; i < fill_w; i++) {
                row[i] = sw_blend_solid_rgba8888(row[i], src_a, inv_src_a, src_r_premul, src_g_premul, src_b_premul);
            }
        }
    }
    return true;
}

// ⚡ Scanline triangle rasterizer with affine UV interpolation.
// Rasterizes one triangle (3 vertices with position, tex_coord) into sw_frame_surface.
// src_pixels is RGBA8888, tex dimensions are src_w × src_h.
typedef struct SwTriVert {
    float x, y, u, v;
} SwTriVert;

static void sw_raster_triangle(const SwTriVert* v0, const SwTriVert* v1, const SwTriVert* v2,
                               const uint32_t* src_pixels, int src_w, int src_h, uint32_t color, uint32_t* dst_pixels,
                               int dst_pitch, int clip_w, int clip_h) {
    // Sort vertices by Y (top to bottom)
    const SwTriVert* top = v0;
    const SwTriVert* mid = v1;
    const SwTriVert* bot = v2;
    if (mid->y < top->y) {
        const SwTriVert* t = top;
        top = mid;
        mid = t;
    }
    if (bot->y < top->y) {
        const SwTriVert* t = top;
        top = bot;
        bot = t;
    }
    if (bot->y < mid->y) {
        const SwTriVert* t = mid;
        mid = bot;
        bot = t;
    }

    const float total_dy = bot->y - top->y;
    if (total_dy < 0.5f)
        return; // Degenerate triangle

    const bool has_color_mod = (color != 0xFFFFFFFFu);

    // Long edge slopes (top→bot, spans entire triangle height)
    const float inv_total_dy = 1.0f / total_dy;
    const float dx_long = (bot->x - top->x) * inv_total_dy;
    const float du_long = (bot->u - top->u) * inv_total_dy;
    const float dv_long = (bot->v - top->v) * inv_total_dy;

    // Upper half: top → mid
    const float upper_dy = mid->y - top->y;
    if (upper_dy >= 0.5f) {
        const float inv_upper_dy = 1.0f / upper_dy;
        const float dx_short = (mid->x - top->x) * inv_upper_dy;
        const float du_short = (mid->u - top->u) * inv_upper_dy;
        const float dv_short = (mid->v - top->v) * inv_upper_dy;

        int y_start = sw_clamp((int)SDL_ceilf(top->y), 0, clip_h);
        int y_end = sw_clamp((int)SDL_ceilf(mid->y), 0, clip_h);
        for (int y = y_start; y < y_end; y++) {
            float dt = (float)y - top->y;
            float xa = top->x + dx_long * dt, ua = top->u + du_long * dt, va = top->v + dv_long * dt;
            float xb = top->x + dx_short * dt, ub = top->u + du_short * dt, vb = top->v + dv_short * dt;
            if (xa > xb) {
                float tmp;
                tmp = xa;
                xa = xb;
                xb = tmp;
                tmp = ua;
                ua = ub;
                ub = tmp;
                tmp = va;
                va = vb;
                vb = tmp;
            }
            int x0 = sw_clamp((int)SDL_ceilf(xa), 0, clip_w);
            int x1 = sw_clamp((int)SDL_ceilf(xb), 0, clip_w);
            float span = xb - xa;
            if (span < 0.5f)
                continue;
            float inv_span = 1.0f / span;
            uint32_t* row = dst_pixels + y * dst_pitch;
            for (int x = x0; x < x1; x++) {
                float frac = ((float)x - xa) * inv_span;
                int tx = sw_clamp((int)((ua + (ub - ua) * frac) * src_w), 0, src_w - 1);
                int ty = sw_clamp((int)((va + (vb - va) * frac) * src_h), 0, src_h - 1);
                uint32_t texel = src_pixels[ty * src_w + tx];
                if (has_color_mod)
                    texel = sw_modulate_rgba8888(texel, color);
                row[x] = sw_blend_rgba8888(row[x], texel);
            }
        }
    }

    // Lower half: mid → bot
    const float lower_dy = bot->y - mid->y;
    if (lower_dy >= 0.5f) {
        const float inv_lower_dy = 1.0f / lower_dy;
        const float dx_short = (bot->x - mid->x) * inv_lower_dy;
        const float du_short = (bot->u - mid->u) * inv_lower_dy;
        const float dv_short = (bot->v - mid->v) * inv_lower_dy;

        int y_start = sw_clamp((int)SDL_ceilf(mid->y), 0, clip_h);
        int y_end = sw_clamp((int)SDL_ceilf(bot->y), 0, clip_h);
        for (int y = y_start; y < y_end; y++) {
            float t_long = (float)y - top->y;
            float t_short = (float)y - mid->y;
            float xa = top->x + dx_long * t_long, ua = top->u + du_long * t_long, va = top->v + dv_long * t_long;
            float xb = mid->x + dx_short * t_short, ub = mid->u + du_short * t_short, vb = mid->v + dv_short * t_short;
            if (xa > xb) {
                float tmp;
                tmp = xa;
                xa = xb;
                xb = tmp;
                tmp = ua;
                ua = ub;
                ub = tmp;
                tmp = va;
                va = vb;
                vb = tmp;
            }
            int x0 = sw_clamp((int)SDL_ceilf(xa), 0, clip_w);
            int x1 = sw_clamp((int)SDL_ceilf(xb), 0, clip_w);
            float span = xb - xa;
            if (span < 0.5f)
                continue;
            float inv_span = 1.0f / span;
            uint32_t* row = dst_pixels + y * dst_pitch;
            for (int x = x0; x < x1; x++) {
                float frac = ((float)x - xa) * inv_span;
                int tx = sw_clamp((int)((ua + (ub - ua) * frac) * src_w), 0, src_w - 1);
                int ty = sw_clamp((int)((va + (vb - va) * frac) * src_h), 0, src_h - 1);
                uint32_t texel = src_pixels[ty * src_w + tx];
                if (has_color_mod)
                    texel = sw_modulate_rgba8888(texel, color);
                row[x] = sw_blend_rgba8888(row[x], texel);
            }
        }
    }
}

// ⚡ Software-rasterize a non-rect quad (2 triangles) into sw_frame_surface.
// Split quad indices {0,1,2,3} into triangles {0,1,2} and {1,2,3}.
static bool sw_raster_quad(const SWRaster_Context* ctx, int task_idx) {
    const unsigned int th = ctx->th[task_idx];
    const int tex_handle = LO_16_BITS(th);
    const int pal_handle = HI_16_BITS(th);
    if (tex_handle <= 0 || tex_handle > FL_TEXTURE_MAX)
        return false;
    const int ti = tex_handle - 1;

    int src_w, src_h;
    const uint32_t* src_pixels = NULL;
    if (pal_handle > 0) {
        src_pixels = ctx->lookup_cached_pixels(ti, pal_handle, &src_w, &src_h);
    } else {
        src_pixels = ctx->ensure_nonidx_pixels(ti, &src_w, &src_h);
    }
    if (!src_pixels)
        return false;

    // Convert vertex color to RGBA8888 for modulation
    const SDL_FColor* fc = &ctx->verts[task_idx][0].color;
    const uint32_t color = ((uint32_t)(fc->r * 255.0f + 0.5f) << 24) | ((uint32_t)(fc->g * 255.0f + 0.5f) << 16) |
                           ((uint32_t)(fc->b * 255.0f + 0.5f) << 8) | (uint32_t)(fc->a * 255.0f + 0.5f);

    // Build SwTriVert array from SDL_Vertex data
    SwTriVert verts[4];
    for (int i = 0; i < 4; i++) {
        verts[i].x = ctx->verts[task_idx][i].position.x;
        verts[i].y = ctx->verts[task_idx][i].position.y;
        verts[i].u = ctx->verts[task_idx][i].tex_coord.x;
        verts[i].v = ctx->verts[task_idx][i].tex_coord.y;
    }

    uint32_t* dst_pixels = (uint32_t*)sw_frame_surface->pixels;
    const int dst_pitch = sw_frame_surface->pitch / (int)sizeof(uint32_t);

    // Rasterize two triangles: {0,1,2} and {1,2,3}
    sw_raster_triangle(&verts[0],
                       &verts[1],
                       &verts[2],
                       src_pixels,
                       src_w,
                       src_h,
                       color,
                       dst_pixels,
                       dst_pitch,
                       ctx->canvas_w,
                       ctx->canvas_h);
    sw_raster_triangle(&verts[1],
                       &verts[2],
                       &verts[3],
                       src_pixels,
                       src_w,
                       src_h,
                       color,
                       dst_pixels,
                       dst_pitch,
                       ctx->canvas_w,
                       ctx->canvas_h);
    return true;
}

// ⚡ Software-rasterize a non-rect SOLID quad (flat color fill via triangle rasterizer).
// Uses a 1×1 white pixel as texture source, with the solid color as the modulation color.
static const uint32_t sw_white_pixel = 0xFFFFFFFFu; // RGBA white
static bool sw_raster_solid_quad(const SWRaster_Context* ctx, int task_idx) {
    // Convert vertex color to RGBA8888
    const SDL_FColor* fc = &ctx->verts[task_idx][0].color;
    const uint32_t color = ((uint32_t)(fc->r * 255.0f + 0.5f) << 24) | ((uint32_t)(fc->g * 255.0f + 0.5f) << 16) |
                           ((uint32_t)(fc->b * 255.0f + 0.5f) << 8) | (uint32_t)(fc->a * 255.0f + 0.5f);

    // Build SwTriVert with UV=(0,0) — all sample the single white pixel
    SwTriVert verts[4];
    for (int i = 0; i < 4; i++) {
        verts[i].x = ctx->verts[task_idx][i].position.x;
        verts[i].y = ctx->verts[task_idx][i].position.y;
        verts[i].u = 0.0f;
        verts[i].v = 0.0f;
    }

    uint32_t* dst_pixels = (uint32_t*)sw_frame_surface->pixels;
    const int dst_pitch = sw_frame_surface->pitch / (int)sizeof(uint32_t);

    sw_raster_triangle(&verts[0],
                       &verts[1],
                       &verts[2],
                       &sw_white_pixel,
                       1,
                       1,
                       color,
                       dst_pixels,
                       dst_pitch,
                       ctx->canvas_w,
                       ctx->canvas_h);
    sw_raster_triangle(&verts[1],
                       &verts[2],
                       &verts[3],
                       &sw_white_pixel,
                       1,
                       1,
                       color,
                       dst_pixels,
                       dst_pitch,
                       ctx->canvas_w,
                       ctx->canvas_h);
    return true;
}

// ⚡ Software-frame render: composite all tasks into sw_frame_surface, upload as one texture.
// Returns true if the entire frame was software-composited; false = fallback to draw calls.
bool SWRaster_RenderFrame(const SWRaster_Context* ctx) {
    TRACE_ZONE_N("SDL2D:SwFrame");

    // ⚡ BENCHMARK TOGGLE: set to true to force hardware path
    static const bool sw_frame_disabled = true;
    if (sw_frame_disabled) {
        TRACE_ZONE_END();
        return false;
    }

    if (!ensure_sw_frame_surface(ctx) || !ensure_sw_frame_upload_texture(ctx)) {
        TRACE_ZONE_END();
        return false;
    }

    // ⚡ Pixel budget heuristic: bail to hardware if total destination pixel work
    // exceeds a threshold. Large-rect screens (VS portraits, backgrounds) are faster
    // on GPU than per-pixel CPU blending. Gameplay sprites (many 16×16 tiles) stay fast.
    // Budget = 2× framebuffer area (384×224 = 86,016 → threshold ~172K pixels).
    {
        uint64_t total_pixels = 0;
        const uint64_t pixel_budget = UINT64_MAX; // disabled for debugging
        for (int i = 0; i < ctx->count; i++) {
            const int idx = ctx->order[i];
            if (ctx->is_rect[idx]) {
                const SDL_FRect* r = &ctx->dst_rect[idx];
                total_pixels += (uint64_t)(r->w > 0 ? r->w : 0) * (uint64_t)(r->h > 0 ? r->h : 0);
            } else {
                // Non-rect: use AABB area as estimate
                const SDL_Vertex* v = ctx->verts[idx];
                float minx = v[0].position.x, miny = v[0].position.y;
                float maxx = minx, maxy = miny;
                for (int k = 1; k < 4; k++) {
                    if (v[k].position.x < minx)
                        minx = v[k].position.x;
                    if (v[k].position.x > maxx)
                        maxx = v[k].position.x;
                    if (v[k].position.y < miny)
                        miny = v[k].position.y;
                    if (v[k].position.y > maxy)
                        maxy = v[k].position.y;
                }
                total_pixels += (uint64_t)(maxx - minx) * (uint64_t)(maxy - miny);
            }
            if (total_pixels > pixel_budget) {
                TRACE_ZONE_END();
                return false;
            }
        }
    }

    // Compute RGBA8888 clear color for this frame
    const Uint8 cr = (ctx->frame_clear_color >> 16) & 0xFF;
    const Uint8 cg = (ctx->frame_clear_color >> 8) & 0xFF;
    const Uint8 cb = ctx->frame_clear_color & 0xFF;
    const Uint8 ca = ctx->frame_clear_color >> 24;
    const uint32_t clear_rgba = (ca != SDL_ALPHA_TRANSPARENT)
                                    ? ((uint32_t)cr << 24) | ((uint32_t)cg << 16) | ((uint32_t)cb << 8) | ca
                                    : 0x000000FFu; // opaque black fallback (R=0,G=0,B=0,A=255)

    // ⚡ Phase 0: Build current-frame tile coverage from all task rects.
    SDL_memset(dt_current, 0, sizeof(dt_current));
    for (int i = 0; i < ctx->count; i++) {
        const int idx = ctx->order[i];
        if (ctx->is_rect[idx]) {
            const SDL_FRect* r = &ctx->dst_rect[idx];
            dt_mark_rect(r->x, r->y, r->w, r->h);
        } else {
            // Non-rect: compute AABB from vertex positions
            const SDL_Vertex* v = ctx->verts[idx];
            float minx = v[0].position.x, miny = v[0].position.y;
            float maxx = minx, maxy = miny;
            for (int k = 1; k < 4; k++) {
                if (v[k].position.x < minx)
                    minx = v[k].position.x;
                if (v[k].position.x > maxx)
                    maxx = v[k].position.x;
                if (v[k].position.y < miny)
                    miny = v[k].position.y;
                if (v[k].position.y > maxy)
                    maxy = v[k].position.y;
            }
            dt_mark_rect(minx, miny, maxx - minx, maxy - miny);
        }
    }

    // ⚡ Compute dirty tile union (current | previous) and selectively clear.
    // If clear color changed, force all tiles dirty.
    const bool clear_color_changed = !dt_prev_clear_valid || (clear_rgba != dt_prev_clear_color);
    int dirty_count = 0;
    uint32_t* dst_pixels = (uint32_t*)sw_frame_surface->pixels;
    const int dst_pitch = sw_frame_surface->pitch / (int)sizeof(uint32_t);

    for (int t = 0; t < DT_TOTAL; t++) {
        if (clear_color_changed || dt_current[t] || dt_previous[t]) {
            dirty_count++;
            // Clear this 16×16 tile to the clear color
            const int col = t % DT_COLS;
            const int row = t / DT_COLS;
            const int px = col * DT_SIZE;
            const int py = row * DT_SIZE;
            for (int y = py; y < py + DT_SIZE; y++) {
                uint32_t* row_ptr = dst_pixels + y * dst_pitch + px;
                SDL_memset4(row_ptr, clear_rgba, DT_SIZE);
            }
        }
    }
    dt_prev_clear_color = clear_rgba;
    dt_prev_clear_valid = true;
    (void)dirty_count; // reserved for Tracy TRACE_PLOT_INT("DirtyTiles", dirty_count)

    // Phase 1: Eligibility check + rasterize
    for (int i = 0; i < ctx->count; i++) {
        const int idx = ctx->order[i];
        // ⚡ Deferred texture: task_texture may be NULL (FlushBatch path).
        // Use task_th (tex+palette handle) to detect textured vs solid tasks.
        const bool is_textured = (LO_16_BITS(ctx->th[idx]) > 0);
        const bool is_rect = ctx->is_rect[idx];

        if (is_rect && is_textured) {
            if (!sw_raster_textured(ctx, idx)) {
                TRACE_ZONE_END();
                return false;
            }
        } else if (is_rect && !is_textured) {
            // Solid rect — dst_rect and color32 were populated at enqueue time
            if (!sw_raster_solid(ctx, idx)) {
                TRACE_ZONE_END();
                return false;
            }
        } else if (!is_rect && is_textured) {
            // ⚡ Non-rect textured geometry — scanline triangle rasterizer
            if (!sw_raster_quad(ctx, idx)) {
                TRACE_ZONE_END();
                return false;
            }
        } else {
            // ⚡ Non-rect solid geometry — triangle rasterizer with flat color
            if (!sw_raster_solid_quad(ctx, idx)) {
                TRACE_ZONE_END();
                return false;
            }
        }
    }

    // Phase 2: Upload to GPU as a single texture
    if (!SDL_UpdateTexture(sw_frame_upload_tex, NULL, sw_frame_surface->pixels, sw_frame_surface->pitch)) {
        TRACE_ZONE_END();
        return false;
    }

    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    SDL_SetRenderTarget(renderer, ctx->canvas);
    const SDL_FRect dst = { 0.0f, 0.0f, (float)ctx->canvas_w, (float)ctx->canvas_h };
    SDL_RenderTexture(renderer, sw_frame_upload_tex, NULL, &dst);

    TRACE_ZONE_END();
    SDL_memcpy(dt_previous, dt_current, sizeof(dt_previous));
    return true;
}
