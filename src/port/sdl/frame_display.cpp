#include "port/sdl/frame_display.h"

#include "common.h"
#include "imgui.h"
#include "imgui_wrapper.h"
#include <cstdio>

#include "port/sdl/sdl_app.h"
#include "port/sdl/training_menu.h"
#include "sf33rd/Source/Game/training/training_state.h"

#include <deque>
#include <vector>

const size_t MAX_FRAME_HISTORY = 120; // 2 seconds of frames

struct FrameRecord {
    TrainingFrameState p1_state;
    TrainingFrameState p2_state;
    s32 g_frame;
};

static std::deque<FrameRecord> frame_history;
static s32 last_recorded_frame = -1;
static s32 consecutive_idle_frames = 0;   // Track how long both players have been idle
static bool has_started_tracking = false; // Prevents showing framebar before match really starts

void frame_display_init() {
    frame_history.clear();
    last_recorded_frame = -1;
    consecutive_idle_frames = 0;
    has_started_tracking = false;
}

static ImU32 get_color_for_state(TrainingFrameState state) {
    switch (state) {
    case FRAME_STATE_STARTUP:
        return IM_COL32(0, 255, 0, 255); // Green
    case FRAME_STATE_ACTIVE:
        return IM_COL32(255, 0, 0, 255); // Red
    case FRAME_STATE_RECOVERY:
        return IM_COL32(0, 100, 255, 255); // Blue
    case FRAME_STATE_HITSTUN:
        return IM_COL32(255, 128, 0, 255); // Orange
    case FRAME_STATE_BLOCKSTUN:
        return IM_COL32(255, 255, 0, 255); // Yellow
    case FRAME_STATE_DOWN:
        return IM_COL32(80, 0, 0, 255); // Dark Red
    case FRAME_STATE_IDLE:
    default:
        return IM_COL32(60, 60, 60, 150); // Dark Gray (Empty)
    }
}

// Build a "Startup XF / Total XF / Advantage XF" string like SF6's frame bar.
// last_startup / last_active / last_recovery come from TrainingPlayerState after a move resolves.
// advantage_value is the final computed advantage (+/- frames).
// Shows "--" for each field when no move has been tracked yet.
static void build_stats_string(char* buf, size_t buf_sz, const TrainingPlayerState& ps, bool advantage_from_opponent) {
    // Startup
    if (ps.last_startup > 0)
        snprintf(buf, buf_sz, "Startup %dF", (int)ps.last_startup);
    else
        snprintf(buf, buf_sz, "Startup --");

    // Total (startup + active + recovery)
    char tmp[64];
    s32 total = (s32)ps.last_startup + (s32)ps.last_active + (s32)ps.last_recovery;
    if (total > 0)
        snprintf(tmp, sizeof(tmp), " / Total %dF", total);
    else
        snprintf(tmp, sizeof(tmp), " / Total --");
    strncat(buf, tmp, buf_sz - strlen(buf) - 1);

    // Advantage — positive/negative/zero
    if (!advantage_from_opponent) {
        // Use this player's own advantage_value
        if (ps.advantage_active) {
            strncat(buf, " / Advantage ...", buf_sz - strlen(buf) - 1);
        } else if (ps.last_startup > 0 || ps.last_active > 0) {
            // A move was tracked
            if (ps.advantage_value > 0)
                snprintf(tmp, sizeof(tmp), " / Advantage +%d", (int)ps.advantage_value);
            else if (ps.advantage_value < 0)
                snprintf(tmp, sizeof(tmp), " / Advantage %d", (int)ps.advantage_value);
            else
                snprintf(tmp, sizeof(tmp), " / Advantage 0");
            strncat(buf, tmp, buf_sz - strlen(buf) - 1);
        } else {
            strncat(buf, " / Advantage --", buf_sz - strlen(buf) - 1);
        }
    }
}

void frame_display_render() {
    if (!g_training_menu_settings.show_frame_meter || show_training_menu)
        return;

    s32 current_frame = g_training_state.frame_number;

    // Only record a new frame when:
    //   1. The frame counter advanced (once per engine frame), AND
    //   2. We're in a match, AND
    //   3. At least one player is NOT idle (pause bar when both players are idle).
    // Use current_frame_state for the pause check — is_idle alone was insufficient for standing
    // attacks (pat_status stays 0, same as neutral). current_frame_state already handles all cases.
    bool both_idle = (g_training_state.p1.current_frame_state == FRAME_STATE_IDLE) &&
                     (g_training_state.p2.current_frame_state == FRAME_STATE_IDLE);

    // Clear history if idle for 1.5 seconds (90 frames at 60fps)
    if (both_idle && g_training_state.is_in_match) {
        if (current_frame != last_recorded_frame) {
            consecutive_idle_frames++;
            if (consecutive_idle_frames >= 90 && !frame_history.empty()) {
                frame_history.clear();
            }
        }
    } else {
        consecutive_idle_frames = 0;
    }

    if (current_frame != last_recorded_frame && g_training_state.is_in_match && !both_idle) {
        FrameRecord rec;
        rec.p1_state = g_training_state.p1.current_frame_state;
        rec.p2_state = g_training_state.p2.current_frame_state;
        rec.g_frame = current_frame;

        frame_history.push_back(rec);
        if (frame_history.size() > MAX_FRAME_HISTORY) {
            frame_history.pop_front();
        }

        last_recorded_frame = current_frame;
        has_started_tracking = true;
    }

    if (!has_started_tracking)
        return;

    ImGuiIO& io = ImGui::GetIO();

    SDL_FRect game_rect = get_letterbox_rect((int)io.DisplaySize.x, (int)io.DisplaySize.y);

    float scale = game_rect.h / 480.0f;
    if (scale <= 0.1f)
        scale = 0.1f;

    float box_width = 4.0f * scale;  // Each frame is 4 pixels wide
    float box_height = 4.0f * scale; // Square
    float padding = 2.0f * scale;

    float text_font_scale = scale * 1.8f;
    float orig_scale = io.FontGlobalScale;
    io.FontGlobalScale = text_font_scale;
    float text_height = ImGui::GetTextLineHeight();
    io.FontGlobalScale = orig_scale;

    float total_width = (MAX_FRAME_HISTORY * (box_width + 1.0f)) + (padding * 2.0f);
    // Layout (top to bottom):
    // padding | P1 text | padding | P1 bar | padding | P2 bar | padding | P2 text | padding
    float total_height =
        padding + text_height + padding + box_height + padding + box_height + padding + text_height + padding;
    // Position just below the life bars at the top of the screen.
    // SF3's native 224p HUD occupies ~45px at top for life bars and names; scale that up.
    ImVec2 window_pos(game_rect.x + (game_rect.w - total_width) * 0.5f, game_rect.y + 64.0f * scale);

    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(total_width, total_height), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.7f));

    if (ImGui::Begin("Frame Meter",
                     nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs)) {

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos(); // Get absolute top-left of window
        p.x += padding;
        p.y += padding;

        // ---- P1 stats text (above P1 bar) ----
        io.FontGlobalScale = text_font_scale;

        char p1_stats[128] = "";
        build_stats_string(p1_stats, sizeof(p1_stats), g_training_state.p1, false);

        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y));
        // Advantage color: green = positive, red = negative, white = neutral/pending
        if (g_training_state.p1.advantage_value > 0 && !g_training_state.p1.advantage_active)
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", p1_stats);
        else if (g_training_state.p1.advantage_value < 0 && !g_training_state.p1.advantage_active &&
                 (g_training_state.p1.last_startup > 0 || g_training_state.p1.last_active > 0))
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", p1_stats);
        else
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.9f), "%s", p1_stats);

        io.FontGlobalScale = orig_scale;

        // ---- P1 Bar ----
        ImVec2 start_p1(p.x, p.y + text_height + padding);
        for (size_t i = 0; i < frame_history.size(); i++) {
            ImVec2 topleft(start_p1.x + i * (box_width + 1.0f), start_p1.y);
            ImVec2 botright(topleft.x + box_width, topleft.y + box_height);
            draw_list->AddRectFilled(topleft, botright, get_color_for_state(frame_history[i].p1_state));
        }

        // ---- P2 Bar ----
        ImVec2 start_p2(p.x, start_p1.y + box_height + padding);
        for (size_t i = 0; i < frame_history.size(); i++) {
            ImVec2 topleft(start_p2.x + i * (box_width + 1.0f), start_p2.y);
            ImVec2 botright(topleft.x + box_width, topleft.y + box_height);
            draw_list->AddRectFilled(topleft, botright, get_color_for_state(frame_history[i].p2_state));
        }

        // ---- P2 stats text (below P2 bar) ----
        // P2 is the dummy — show its advantage relative to P1's last resolved advantage (negated).
        // If P1 just hit P2 and is +5, P2 is -5.
        io.FontGlobalScale = text_font_scale;

        char p2_stats[128] = "";
        // For P2 stats, mirror P1's resolved values but flip the advantage sign.
        // Build manually: P2 rarely attacks so we show its own state if it attacked,
        // otherwise show the receiver's perspective (negated P1 advantage).
        bool p2_has_move = (g_training_state.p2.last_startup > 0 || g_training_state.p2.last_active > 0);
        if (p2_has_move) {
            build_stats_string(p2_stats, sizeof(p2_stats), g_training_state.p2, false);
        } else {
            // P2 is the dummy recipient — show "--" for startup/total but flip P1's advantage
            s32 p2_adv = -g_training_state.p1.advantage_value;
            bool p1_move_done = (g_training_state.p1.last_startup > 0 || g_training_state.p1.last_active > 0);
            if (p1_move_done && !g_training_state.p1.advantage_active) {
                if (p2_adv > 0)
                    snprintf(p2_stats, sizeof(p2_stats), "Startup -- / Total -- / Advantage +%d", (int)p2_adv);
                else
                    snprintf(p2_stats, sizeof(p2_stats), "Startup -- / Total -- / Advantage %d", (int)p2_adv);
            } else {
                snprintf(p2_stats, sizeof(p2_stats), "Startup -- / Total -- / Advantage --");
            }
        }

        ImGui::SetCursorScreenPos(ImVec2(start_p2.x, start_p2.y + box_height + padding));
        // Advantage color from P2's perspective
        s32 p2_adv_val = p2_has_move ? g_training_state.p2.advantage_value : -g_training_state.p1.advantage_value;
        bool p2_move_resolved = p2_has_move
                                    ? (!g_training_state.p2.advantage_active &&
                                       (g_training_state.p2.last_startup > 0 || g_training_state.p2.last_active > 0))
                                    : (!g_training_state.p1.advantage_active &&
                                       (g_training_state.p1.last_startup > 0 || g_training_state.p1.last_active > 0));
        if (p2_move_resolved && p2_adv_val > 0)
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", p2_stats);
        else if (p2_move_resolved && p2_adv_val < 0)
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", p2_stats);
        else
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.9f), "%s", p2_stats);

        io.FontGlobalScale = orig_scale;
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void frame_display_shutdown() {
    frame_history.clear();
    consecutive_idle_frames = 0;
    has_started_tracking = false;
}
