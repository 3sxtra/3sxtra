#include "port/ui/native_imgui.h"

#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/engine/workuser_select.h"
#include "sf33rd/Source/Game/effect/effect.h" // For frw pool and push_effect_work
#include "sf33rd/Source/Game/effect/eff04.h"
#include "sf33rd/Source/Game/effect/eff45.h"
#include "sf33rd/Source/Game/effect/eff57.h"
#include "sf33rd/Source/Game/effect/eff61.h"
#include "sf33rd/Source/Game/rendering/texgroup.h"

#include <string.h>

#define UI_MAX_ELEMENTS 22
#define UI_SLOT_MIN 105  // Must be strictly < 128 (EFFECT_MAX). avoids 0x64 (100)
#define UI_SLOT_MAX 126

typedef struct {
    uint32_t id_hash;
    int effect_slot;
    int last_frame_seen;
} UIElementNode;

static UIElementNode s_elements[UI_MAX_ELEMENTS];
static int s_frame_counter = 0;

static int s_current_x = 0;
static int s_current_y = 0;
static NativeUIDir s_dir = UI_DIR_VERTICAL;

static int s_focus_index = 0;
static int s_current_index = 0;
static int s_max_index = 0;
static int s_graphic_offset = 0;
static int s_master_player = 0;

static bool s_confirm_pressed = false;

static u16 s_letter_type = 0x7047;

void NativeUI_SetLetterType(u16 type) {
    s_letter_type = type;
}

// Scope ID Stack
static int s_id_stack[16];
static int s_id_stack_size = 0;

// Grid navigation
static int s_grid_columns = 1;
static int s_focus_delta = 0;

// Scroll state
static int s_scroll_max_visible = 0;
static int s_scroll_current_offset = 0;
static bool s_in_scroll_list = false;

void NativeUI_PushID(int dynamic_id) {
    if (s_id_stack_size < 16) {
        s_id_stack[s_id_stack_size++] = dynamic_id;
    }
}

void NativeUI_PopID(void) {
    if (s_id_stack_size > 0) {
        s_id_stack_size--;
    }
}

void NativeUI_PushGrid(int columns) {
    s_grid_columns = columns > 0 ? columns : 1;
}

void NativeUI_PopGrid(void) {
    s_grid_columns = 1;
}

// FNV-1a Hash with Stack Included
static uint32_t HashId(const char* label) {
    uint32_t hash = 2166136261u;
    for (int i = 0; label[i] != '\0'; i++) {
        hash ^= (uint8_t)label[i];
        hash *= 16777619;
    }
    for (int i = 0; i < s_id_stack_size; i++) {
        uint8_t* p = (uint8_t*)&s_id_stack[i];
        for (int j = 0; j < 4; j++) {
            hash ^= p[j];
            hash *= 16777619;
        }
    }
    return hash;
}

// Effect Slot Allocator
static int AllocSlot(uint32_t id_hash, bool* out_is_new) {
    if (out_is_new) *out_is_new = false;
    
    // Check if it already exists
    for (int i = 0; i < UI_MAX_ELEMENTS; i++) {
        if (s_elements[i].id_hash == id_hash) {
            s_elements[i].last_frame_seen = s_frame_counter;
            return s_elements[i].effect_slot;
        }
    }

    // Allocate new slot
    for (int i = 0; i < UI_MAX_ELEMENTS; i++) {
        if (s_elements[i].id_hash == 0) {
            s_elements[i].id_hash = id_hash;
            s_elements[i].effect_slot = UI_SLOT_MIN + i; // Map directly
            if (s_elements[i].effect_slot > UI_SLOT_MAX) {
                return -1; // Out of memory/slots gracefully!
            }
            s_elements[i].last_frame_seen = s_frame_counter;
            if (out_is_new) *out_is_new = true;
            return s_elements[i].effect_slot;
        }
    }
    return -1;
}

void NativeUI_Clear(void) {
    // Explicitly kill any remaining tasks managed by IMGUI
    for (int i = 0; i < UI_MAX_ELEMENTS; i++) {
        if (s_elements[i].id_hash != 0) {
            int slot = s_elements[i].effect_slot;
            for (int k = 0; k < EFFECT_MAX; k++) {
                WORK_Other* w = (WORK_Other*)frw[k];
                if (w->wu.be_flag && (w->wu.id == 57 || w->wu.id == 61) && w->wu.dir_old == slot) {
                    push_effect_work((WORK*)w);
                }
            }
            Order[slot] = 0;
            s_elements[i].id_hash = 0;
        }
    }

    memset(s_elements, 0, sizeof(s_elements));
    s_frame_counter = 0;
    s_focus_index = 0;
    // Do NOT wipe Menu_Cursor_Y[0] here. It destroys state for dispatch targets right after calling NativeUI_Clear.
    s_scroll_current_offset = 0;
    s_in_scroll_list = false;
    s_graphic_offset = 0;
    s_master_player = 0;
    s_letter_type = 0x7047;
}

void NativeUI_Begin(int start_x, int start_y, NativeUIDir dir) {
    s_current_x = start_x;
    s_current_y = start_y;
    s_dir = dir;
    s_current_index = 0;
    // s_confirm_pressed is handled in NativeUI_ProcessInput now
    s_frame_counter++;
}

void NativeUI_End(void) {
    s_max_index = s_current_index;

    // Garbage collect unused slots
    for (int i = 0; i < UI_MAX_ELEMENTS; i++) {
        if (s_elements[i].id_hash != 0 && s_elements[i].last_frame_seen != s_frame_counter) {
            int slot = s_elements[i].effect_slot;
            
            // JUDGE PETROV FIX: Hunt down and explicitly kill the CPS3 task
            // instead of spamming Suicide[0]=1 and risking collateral damage.
            for (int k = 0; k < EFFECT_MAX; k++) {
                WORK_Other* w = (WORK_Other*)frw[k];
                if (w->wu.be_flag && (w->wu.id == 57 || w->wu.id == 61) && w->wu.dir_old == slot) {
                    push_effect_work((WORK*)w);
                }
            }
            
            Order[slot] = 0;
            s_elements[i].id_hash = 0;
        }
    }

    // Safety wrap focus and Sync Legacy Cursor Memory so eff61.c legacy loops know the focus!
    if (s_max_index > 0) {
        if (s_focus_index < 0) s_focus_index = s_max_index - 1;
        if (s_focus_index >= s_max_index) s_focus_index = 0;
    }
    Menu_Cursor_Y[0] = s_focus_index;
}

void NativeUI_SetNextPos(int x, int y) {
    s_current_x = x;
    s_current_y = y;
}

static void AdvanceLayout(int width, int height) {
    if (s_dir == UI_DIR_VERTICAL) {
        s_current_y += height;
    } else {
        s_current_x += width;
    }
}

void NativeUI_BeginScrollList(int visible_elements) {
    s_scroll_max_visible = visible_elements;
    s_in_scroll_list = true;
    
    // Automatically shift sliding focus bracket based on target cursor
    if (s_focus_index < s_scroll_current_offset) {
        s_scroll_current_offset = s_focus_index;
    } else if (s_focus_index >= s_scroll_current_offset + s_scroll_max_visible) {
        s_scroll_current_offset = s_focus_index - s_scroll_max_visible + 1;
    }
}

void NativeUI_EndScrollList(void) {
    s_in_scroll_list = false;
}

void NativeUI_SetGraphicOffset(int offset) {
    s_graphic_offset = offset;
}

void NativeUI_SetMasterPlayer(int master_player_id) {
    s_master_player = master_player_id;
}

void NativeUI_SetNextIndex(int explicit_index) {
    s_current_index = explicit_index;
}

void NativeUI_SetFocusIndex(int index) {
    s_focus_index = index;
}

void NativeUI_Header(int header_type) {
    bool is_new = false;
    int slot = AllocSlot((uint32_t)header_type + 0x10000, &is_new);
    if (slot != -1 && is_new) {
        // Initialize if newly created
        Order[slot] = 1;
        Order_Dir[slot] = 8;
        Order_Timer[slot] = 1;
        effect_57_init(slot, (MenuHeader)header_type, 0, 0x3F, 2);
        load_any_texture_patnum(0x7F30, 0xC, 0); // Guarantee texture is in VRAM
    }
}

bool NativeUI_ButtonEx(const char* label, bool disabled) {
    int my_index = s_current_index++;
    
    // Auto-skip disabled elements using the directional delta
    if (s_focus_index == my_index && disabled) {
        s_focus_index += (s_focus_delta != 0) ? s_focus_delta : 1;
    }

    bool is_focused = (s_focus_index == my_index) && !disabled;
    
    // Visually occlude the element if it scrolled out of layout bounds
    bool is_visible = true;
    if (s_in_scroll_list) {
        if (my_index < s_scroll_current_offset || my_index >= s_scroll_current_offset + s_scroll_max_visible) {
            is_visible = false;
        }
    }
    
    if (!is_visible) {
        // Pure Logical Processing: Do NOT allocate visual hashing or layout bounds to prevent 
        // exhausting the 128-element memory cache! The GC will naturally sweep it.
        return false;
    }
    
    uint32_t hash = HashId(label);
    
    bool is_new = false;
    int slot = AllocSlot(hash, &is_new);
    
    if (slot != -1 && is_new) { 
        Order[slot] = 1;
        Order_Dir[slot] = 4;
        int visual_index = my_index - s_scroll_current_offset;
        Order_Timer[slot] = visual_index + 7; // Use visual offset for animation sequence
        
        int graphic_index = my_index + s_graphic_offset;
        // Arg 5 (char_ix) dictates absolute graphic string and Y layout from Slide_Pos_Data_61.
        // Arg 6 (cursor_index) connects to Menu_Cursor_Y[0] for dynamic highlighting.
        effect_61_init(0, slot, 0, s_master_player, graphic_index, my_index, s_letter_type); 
    }
    
    NativeUI_Label(label);

    if (is_focused && s_confirm_pressed) {
        return true;
    }
    
    return false;
}

bool NativeUI_Button(const char* label) {
    return NativeUI_ButtonEx(label, false);
}

void NativeUI_Label(const char* label) {
    AdvanceLayout(0, 16); 
}

void NativeUI_ProcessInput(uint16_t pad_input, uint16_t io_result) {
    s_focus_delta = 0;
    if (pad_input & 0x01) { // UP
        s_focus_delta = -s_grid_columns;
        s_focus_index += s_focus_delta;
        if (s_focus_index < 0) s_focus_index = s_max_index - 1;
    }
    if (pad_input & 0x02) { // DOWN
        s_focus_delta = s_grid_columns;
        s_focus_index += s_focus_delta;
        if (s_focus_index >= s_max_index) s_focus_index = 0;
    }
    if (s_grid_columns > 1) { // Left/Right enabled for Grids
        if (pad_input & 0x04) { // LEFT
            s_focus_delta = -1;
            s_focus_index += s_focus_delta;
            if (s_focus_index < 0) s_focus_index = s_max_index - 1;
        }
        if (pad_input & 0x08) { // RIGHT
            s_focus_delta = 1;
            s_focus_index += s_focus_delta;
            if (s_focus_index >= s_max_index) s_focus_index = 0;
        }
    }
    
    // Set confirmed state securely frame-over-frame
    s_confirm_pressed = (io_result == 0x100);
}
