cpp_file = "src/port/sdl/renderer/sdl_game_renderer_sdl.c"
with open(cpp_file, "r") as f:
    orig = f.readlines()


def get_lines(start, end):
    return orig[start - 1 : end]


out_c = []
out_c.append('#include "port/sdl/renderer/sdl_game_renderer_sdl_sw.h"\n')
out_c.append('#include "port/sdl/app/sdl_app.h"\n')
out_c.append('#include "port/tracy_zones.h"\n')
out_c.append("\n")

# dt_mark_rect etc (86-132)
out_c.extend(get_lines(86, 132))
# DIV255 & mod (188-233)
out_c.extend(get_lines(188, 233))
# clamp (242-248)
out_c.extend(get_lines(241, 248))
# sw_blend_solid (250-280)
out_c.extend(get_lines(250, 280))
# LERP_FLOAT & lerp_fcolors (390-397)
out_c.extend(get_lines(390, 397))

# SWRaster init/shutdown
out_c.append("\nvoid SWRaster_Init(void) {}\n")
out_c.append("\nvoid SWRaster_Shutdown(void) {\n")
out_c.append("    if (sw_frame_surface != NULL) {\n")
out_c.append("        SDL_DestroySurface(sw_frame_surface);\n")
out_c.append("        sw_frame_surface = NULL;\n")
out_c.append("    }\n")
out_c.append("    if (sw_frame_upload_tex != NULL) {\n")
out_c.append("        SDL_DestroyTexture(sw_frame_upload_tex);\n")
out_c.append("        sw_frame_upload_tex = NULL;\n")
out_c.append("    }\n")
out_c.append("}\n\n")


# ensure_sw_frame... (620-641)
out_c.extend(get_lines(620, 641))
# sw_raster_* (720-1267)
out_c.extend(get_lines(720, 1267))

sw_code = "".join(out_c)
sw_code = sw_code.replace("task_dst_rect[", "ctx->dst_rect[")
sw_code = sw_code.replace("task_src_rect[", "ctx->src_rect[")
sw_code = sw_code.replace("task_color32[", "ctx->color32[")
sw_code = sw_code.replace("task_flip[", "ctx->flip[")
sw_code = sw_code.replace("task_is_rect[", "ctx->is_rect[")
sw_code = sw_code.replace("task_verts[", "ctx->verts[")
sw_code = sw_code.replace("task_z[", "ctx->z[")
sw_code = sw_code.replace("task_th[", "ctx->th[")
sw_code = sw_code.replace("task_texture[", "ctx->texture[")
sw_code = sw_code.replace("render_task_count", "ctx->count")
sw_code = sw_code.replace("render_task_order[", "ctx->order[")
sw_code = sw_code.replace("cps3_width", "ctx->canvas_w")
sw_code = sw_code.replace("cps3_height", "ctx->canvas_h")
sw_code = sw_code.replace("sw_lookup_cached_pixels(", "ctx->lookup_cached_pixels(")
sw_code = sw_code.replace("sw_ensure_nonidx_pixels(", "ctx->ensure_nonidx_pixels(")
sw_code = sw_code.replace("cps3_canvas", "ctx->canvas")
sw_code = sw_code.replace("flPs2State.FrameClearColor", "ctx->frame_clear_color")

sw_code = sw_code.replace(
    "sw_raster_textured(int task_idx)",
    "sw_raster_textured(const SWRaster_Context* ctx, int task_idx)",
)
sw_code = sw_code.replace(
    "sw_raster_solid(int task_idx)",
    "sw_raster_solid(const SWRaster_Context* ctx, int task_idx)",
)
sw_code = sw_code.replace(
    "sw_raster_quad(int task_idx)",
    "sw_raster_quad(const SWRaster_Context* ctx, int task_idx)",
)
sw_code = sw_code.replace(
    "sw_raster_solid_quad(int task_idx)",
    "sw_raster_solid_quad(const SWRaster_Context* ctx, int task_idx)",
)

sw_code = sw_code.replace("sw_raster_textured(idx)", "sw_raster_textured(ctx, idx)")
sw_code = sw_code.replace("sw_raster_solid(idx)", "sw_raster_solid(ctx, idx)")
sw_code = sw_code.replace("sw_raster_quad(idx)", "sw_raster_quad(ctx, idx)")
sw_code = sw_code.replace("sw_raster_solid_quad(idx)", "sw_raster_solid_quad(ctx, idx)")

sw_code = sw_code.replace(
    "static bool sw_render_frame(void)",
    "bool SWRaster_RenderFrame(const SWRaster_Context* ctx)",
)

# Add dt_previous rotation
# We'll just replace the last 'return true;' inside the function.
frames = sw_code.rsplit("return true;", 1)
sw_code = (
    frames[0]
    + "SDL_memcpy(dt_previous, dt_current, sizeof(dt_previous));\n    return true;"
    + frames[1]
)

with open("src/port/sdl/renderer/sdl_game_renderer_sdl_sw.c", "w") as f:
    f.write(sw_code)

h_code = """#pragma once

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

void SWRaster_Init(void);
void SWRaster_Shutdown(void);

void lerp_fcolors(SDL_FColor* dest, const SDL_FColor* a, const SDL_FColor* b, float x);

typedef struct {
    int count;
    const int* order;
    const bool* is_rect;
    const unsigned int* th;
    const SDL_Vertex (*verts)[4];
    const SDL_FRect* src_rect;
    const SDL_FRect* dst_rect;
    const SDL_FlipMode* flip;
    const uint32_t* color32;

    int canvas_w;
    int canvas_h;
    SDL_Texture* canvas;

    const uint32_t* (*lookup_cached_pixels)(int ti, int palette_handle, int* out_w, int* out_h);
    const uint32_t* (*ensure_nonidx_pixels)(int ti, int* out_w, int* out_h);
    
    uint32_t frame_clear_color;
} SWRaster_Context;

bool SWRaster_RenderFrame(const SWRaster_Context* ctx);
"""
with open("src/port/sdl/renderer/sdl_game_renderer_sdl_sw.h", "w") as f:
    f.write(h_code)

new_sdl = []
new_sdl.extend(get_lines(1, 85))
new_sdl.extend(get_lines(133, 187))
new_sdl.extend(get_lines(234, 240))
new_sdl.extend(get_lines(249, 249))
new_sdl.extend(get_lines(281, 389))
new_sdl.extend(get_lines(398, 619))
new_sdl.extend(get_lines(642, 719))
new_sdl.extend(get_lines(1268, 1276))
new_sdl.extend(get_lines(1279, 1421))
new_sdl.append("    SWRaster_Shutdown();\n")
new_sdl.extend(get_lines(1431, 1487))

render_call = """
    SWRaster_Context swctx = {
        .count = render_task_count,
        .order = render_task_order,
        .is_rect = task_is_rect,
        .th = task_th,
        .verts = task_verts,
        .src_rect = task_src_rect,
        .dst_rect = task_dst_rect,
        .flip = task_flip,
        .color32 = task_color32,
        .canvas_w = cps3_width,
        .canvas_h = cps3_height,
        .canvas = cps3_canvas,
        .lookup_cached_pixels = sw_lookup_cached_pixels,
        .ensure_nonidx_pixels = sw_ensure_nonidx_pixels,
        .frame_clear_color = flPs2State.FrameClearColor
    };
    if (SWRaster_RenderFrame(&swctx)) {
        TRACE_PLOT_INT("SoftwareFrame", 1);
        TRACE_ZONE_END();
        return;
    }
"""
new_sdl.append(render_call)
new_sdl.extend(get_lines(1495, len(orig)))

sdl_code = "".join(new_sdl)
sdl_code = sdl_code.replace(
    '#include "port/sdl/renderer/sdl_game_renderer_internal.h"',
    '#include "port/sdl/renderer/sdl_game_renderer_internal.h"\n#include "port/sdl/renderer/sdl_game_renderer_sdl_sw.h"',
)

with open("src/port/sdl/renderer/sdl_game_renderer_sdl.c", "w") as f:
    f.write(sdl_code)

# Add to CMakeLists.txt
cmakepath = "CMakeLists.txt"
with open(cmakepath, "r") as f:
    cmake = f.read()

cmake = cmake.replace(
    "src/port/sdl/renderer/sdl_game_renderer_sdl.c",
    "src/port/sdl/renderer/sdl_game_renderer_sdl.c\\n    src/port/sdl/renderer/sdl_game_renderer_sdl_sw.c",
)

with open(cmakepath, "w") as f:
    f.write(cmake)

print("Extraction script done!")
