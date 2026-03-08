with open("src/port/sdl/renderer/sdl_game_renderer_sdl_sw.c", "r") as f:
    text = f.read()

text = text.replace("static void lerp_fcolors", "void lerp_fcolors")
text = text.replace(
    "ensure_sw_frame_surface(void)",
    "ensure_sw_frame_surface(const SWRaster_Context* ctx)",
)
text = text.replace(
    "ensure_sw_frame_upload_texture(void)",
    "ensure_sw_frame_upload_texture(const SWRaster_Context* ctx)",
)
text = text.replace(
    "if (!ensure_sw_frame_surface() || !ensure_sw_frame_upload_texture())",
    "if (!ensure_sw_frame_surface(ctx) || !ensure_sw_frame_upload_texture(ctx))",
)
text = '#include "common.h"\n' + text

with open("src/port/sdl/renderer/sdl_game_renderer_sdl_sw.c", "w") as f:
    f.write(text)
print("Files fixed.")
