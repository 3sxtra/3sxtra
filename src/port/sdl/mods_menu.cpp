/**
 * @file mods_menu.cpp
 * @brief ImGui mods overlay menu — F3-toggled panel for modding options.
 *
 * Follows the same pattern as shader_menu.cpp (F2). Provides a fullscreen
 * ImGui window with collapsible sections for each mod feature:
 *   - HD Stage Backgrounds toggle + status
 *   - Stage Rendering controls
 *   - Sprite Display controls (from debug menu)
 *   - Stage / Effects controls (from debug menu)
 *   - Audio controls (from debug menu)
 *
 * Debug options that manipulate engine state are gatekept to in-game only
 * (Play_Game != 0) to prevent breaking the game while in menus.
 */
#include "port/sdl/mods_menu.h"
#include "imgui.h"
#include "port/config.h"
#include "port/sdl_bezel.h"
#include <SDL3/SDL.h>

extern "C" {
/* Modded stage system */
void ModdedStage_SetEnabled(bool enabled);
bool ModdedStage_IsEnabled(void);
bool ModdedStage_IsActiveForCurrentStage(void);
int ModdedStage_GetLayerCount(void);
int ModdedStage_GetLoadedStageIndex(void);
void ModdedStage_SetDisableRendering(bool disabled);
bool ModdedStage_IsRenderingDisabled(void);
void ModdedStage_SetAnimationsDisabled(bool disabled);
bool ModdedStage_IsAnimationsDisabled(void);

/* Shader bypass for HD stages — when true, skip librashader on HD backgrounds */
extern bool mods_menu_shader_bypass_enabled;

/* Engine debug options — Debug_w is either a macro (DEBUG builds)
 * or an extern array (release builds), both provided by debug_config.h */
#include "sf33rd/Source/Game/debug/debug_config.h"

/* Game state — 0 = in menus, 1-2 = in gameplay */
extern unsigned char Play_Game;
}

/* -------------------------------------------------------------------------- */
/* Helpers                                                                     */
/* -------------------------------------------------------------------------- */

/** @brief Whether engine debug options are safe to modify right now. */
static inline bool is_in_game(void) {
    return Play_Game != 0;
}

/** @brief Checkbox that reads/writes a Debug_w bool flag. Disabled when not in game. */
static void debug_checkbox(const char* label, DebugOption opt) {
    bool disabled = !is_in_game();
    if (disabled)
        ImGui::BeginDisabled();

    bool val = (Debug_w[opt] != 0);
    if (ImGui::Checkbox(label, &val)) {
        Debug_w[opt] = val ? 1 : 0;
    }

    if (disabled)
        ImGui::EndDisabled();
}

/** @brief Integer slider tied to a Debug_w entry. Disabled when not in game. */
static void debug_slider(const char* label, DebugOption opt, int max_val) {
    bool disabled = !is_in_game();
    if (disabled)
        ImGui::BeginDisabled();

    int ival = Debug_w[opt];
    if (ImGui::SliderInt(label, &ival, 0, max_val)) {
        Debug_w[opt] = (s8)ival;
    }

    if (disabled)
        ImGui::EndDisabled();
}

static void render_centered_text(const char* text) {
    ImVec2 text_size = ImGui::CalcTextSize(text);
    float window_width = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (window_width - text_size.x) * 0.5f);
    ImGui::TextUnformatted(text);
}

static void HelpMarker(const char* desc) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static const ImVec4 COLOR_WARN = ImVec4(1.0f, 0.7f, 0.3f, 1.0f);
static const ImVec4 COLOR_OK = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);

/* -------------------------------------------------------------------------- */
/* Track previous Play_Game to auto-reset debug options on exit to menus       */
/* -------------------------------------------------------------------------- */
static unsigned char s_prev_play_game = 0;

/** @brief Reset any debug options that could break menus when leaving gameplay. */
static void reset_debug_on_exit_game(void) {
    Debug_w[DEBUG_NO_DISP_SHADOW] = 0;
    Debug_w[DEBUG_NO_DISP_SPR_PAL] = 0;
    Debug_w[DEBUG_NO_DISP_SPR_CP3] = 0;
    Debug_w[DEBUG_NO_DISP_SPR_RGB] = 0;
    Debug_w[DEBUG_NO_DISP_TYPE_SB] = 0;
    Debug_w[DEBUG_BG_DRAW_OFF] = 0;
    Debug_w[DEBUG_BLUE_BACK] = 0;
    Debug_w[DEBUG_EFF_NOT_MOVE] = 0;
    Debug_w[DEBUG_PUB_BGM_OFF] = 0;
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */

extern "C" void mods_menu_init(void) {
    /* Nothing to initialize yet */
}

extern "C" void mods_menu_render(int window_width, int window_height) {
    /* Auto-reset debug options when transitioning from gameplay to menus */
    if (s_prev_play_game != 0 && Play_Game == 0) {
        reset_debug_on_exit_game();
    }
    s_prev_play_game = Play_Game;

    /* Match shader_menu font scaling */
    float font_scale = (float)window_height / 480.0f;
    ImGui::GetIO().FontGlobalScale = font_scale;

    ImVec2 window_size((float)window_width, (float)window_height);
    ImVec2 window_pos(0, 0);

    ImGui::SetNextWindowPos(window_pos);
    ImGui::SetNextWindowSize(window_size);
    ImGui::Begin("Mods", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    /* Title */
    render_centered_text("MODS");
    ImGui::Separator();
    ImGui::Spacing();

    /* ===== HD STAGE BACKGROUNDS ===== */
    {
        bool stage_on = ModdedStage_IsEnabled();
        if (ImGui::Checkbox("Enable HD Backgrounds", &stage_on)) {
            ModdedStage_SetEnabled(stage_on);
        }
        HelpMarker("Replaces original stage backgrounds with high-definition PNG layers.\nRequires HD stage files in "
                   "the stages/ folder.");
        if (stage_on && ModdedStage_IsActiveForCurrentStage()) {
            ImGui::SameLine();
            ImGui::TextColored(COLOR_OK, "(Active)");
        }

        bool bypass = mods_menu_shader_bypass_enabled;
        if (ImGui::Checkbox("Bypass Shaders on HD Stages", &bypass)) {
            mods_menu_shader_bypass_enabled = bypass;
        }
        HelpMarker("Skips CRT/scanline shader filters on HD background layers.\nUseful if HD art already looks clean.");
        if (bypass) {
            ImGui::SameLine();
            ImGui::TextColored(COLOR_WARN, "(No CRT/filters)");
        }
    }

    ImGui::Separator();

    /* ===== BEZELS ===== */
    {
        bool bezel_on = BezelSystem_IsVisible();
        if (ImGui::Checkbox("Enable Arcade Bezels", &bezel_on)) {
            BezelSystem_SetVisible(bezel_on);
            Config_SetBool(CFG_KEY_BEZEL_ENABLED, bezel_on);
        }
        HelpMarker("Shows decorative arcade cabinet artwork around the game viewport.");
    }

    ImGui::Separator();

    /* ===== STAGE RENDERING ===== */
    {
        bool render_off = ModdedStage_IsRenderingDisabled();
        if (ImGui::Checkbox("Disable All Stage Rendering", &render_off)) {
            ModdedStage_SetDisableRendering(render_off);
        }
        HelpMarker("Hides all background layers (original + HD).\nUseful for recording clean gameplay footage.");

        bool anims_off = ModdedStage_IsAnimationsDisabled();
        if (ImGui::Checkbox("Disable Stage Animations", &anims_off)) {
            ModdedStage_SetAnimationsDisabled(anims_off);
        }
        HelpMarker("Freezes animated background elements while keeping the stage visible.");

        debug_checkbox("BG Draw Off", DEBUG_BG_DRAW_OFF);
        HelpMarker("Engine-level toggle: disables original background rendering.");
        debug_checkbox("Blue Background", DEBUG_BLUE_BACK);
        HelpMarker("Replaces the stage with a solid blue backdrop.\nUseful for compositing or visibility.");
    }

    ImGui::Separator();

    /* ===== DEBUG OPTIONS (gameplay only) ===== */
    {
        if (!is_in_game()) {
            ImGui::TextColored(COLOR_WARN, "Debug options: in-game only");
        }

        debug_checkbox("Hide Shadows", DEBUG_NO_DISP_SHADOW);
        HelpMarker("Removes character drop-shadows from the ground.");
        debug_checkbox("Hide Palette Sprites", DEBUG_NO_DISP_SPR_PAL);
        HelpMarker("Hides palette-indexed sprites (most character art).");
        debug_checkbox("Hide CPS3 Sprites", DEBUG_NO_DISP_SPR_CP3);
        HelpMarker("Hides CPS3-rendered sprite layers.");
        debug_checkbox("Hide RGB Sprites", DEBUG_NO_DISP_SPR_RGB);
        HelpMarker("Hides RGB sprite overlays (some effects and UI).");
        debug_slider("Sprite Type SB", DEBUG_NO_DISP_TYPE_SB, 3);
        HelpMarker("Controls sub-type sprite rendering level (0=all, 3=none).");
        debug_checkbox("Freeze Effects", DEBUG_EFF_NOT_MOVE);
        HelpMarker("Pauses all visual effects (fireballs, sparks, etc.) in place.");
        debug_checkbox("Mute BGM", DEBUG_PUB_BGM_OFF);
        HelpMarker("Silences background music while keeping sound effects active.");
    }

    ImGui::Spacing();
    ImGui::Separator();

    render_centered_text("Press F3 to close this menu");

    ImGui::End();

    /* Reset global font scale */
    ImGui::GetIO().FontGlobalScale = 1.0f;
}

extern "C" void mods_menu_shutdown(void) {
    /* Nothing to clean up yet */
}
