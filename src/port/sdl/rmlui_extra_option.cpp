/**
 * @file rmlui_extra_option.cpp
 * @brief RmlUi Extra Option (4-page) data model.
 *
 * Replaces the CPS3 effect_C4/effect_40/effect_45/effect_57/effect_66
 * objects spawned by Setup_Next_Page() for Extra Option pages.
 *
 * Key globals:
 *   save_w[1].extra_option.contents[Menu_Page][row] — toggle values
 *   Menu_Page, Menu_Cursor_Y[0], Ex_Page_Data[], Ex_Title_Data[][],
 *   Ex_Letter_Data[][][] — data tables in ex_data.h / ex_data.c
 */

#include "port/sdl/rmlui_extra_option.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/menu/ex_data.h"
#include "structs.h"
} // extern "C"

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

// Max rows per page: Ex_Page_Data = {7, 7, 4, 6}
// Last row on each page is the footer (DEFAULT SETTING / nav).
// Row labels: Ex_Title_Data[page][row]
// Value labels: Ex_Letter_Data[page][row][value_index]

struct ExtraOptionCache {
    int page;
    int cursor;
    int values[4][8]; // 4 pages × up to 8 rows
};
static ExtraOptionCache s_cache = {};

// Helper: get the label portion of Ex_Title_Data (strip trailing /.....)
static Rml::String get_title_label(int page, int row) {
    if (page < 0 || page > 3 || row < 0 || row >= 7)
        return "";
    const char* raw = (const char*)Ex_Title_Data[page][row];
    if (!raw)
        return "";
    // Find '/' separator — label ends there
    const char* slash = strchr(raw, '/');
    if (slash) {
        return Rml::String(raw, slash - raw);
    }
    return Rml::String(raw);
}

// Helper: get value display string
static Rml::String get_value_label(int page, int row, int value) {
    if (page < 0 || page > 3 || row < 0 || row >= 7)
        return "";
    if (value < 0 || value >= 17)
        return "";
    const char* raw = (const char*)Ex_Letter_Data[page][row][value];
    if (!raw)
        return "";
    return Rml::String(raw);
}

// ─── Init ────────────────────────────────────────────────────────
extern "C" void rmlui_extra_option_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("extra_option");
    if (!ctor)
        return;

    ctor.BindFunc("extra_page", [](Rml::Variant& v) { v = (int)Menu_Page; });
    ctor.BindFunc("extra_page_max", [](Rml::Variant& v) { v = 3; }); // 0-indexed max

    ctor.BindFunc("extra_cursor", [](Rml::Variant& v) { v = (int)Menu_Cursor_Y[0]; });

    ctor.BindFunc("extra_row_count", [](Rml::Variant& v) {
        int page = Menu_Page;
        if (page < 0 || page > 3)
            page = 0;
        v = (int)Ex_Page_Data[page];
    });

    // Per-row label and value — we bind up to 7 rows (max across all pages)
    for (int r = 0; r < 7; r++) {
        {
            char name[32];
            snprintf(name, sizeof(name), "extra_label_%d", r);
            int row = r;
            ctor.BindFunc(Rml::String(name), [row](Rml::Variant& v) { v = get_title_label(Menu_Page, row); });
        }
        {
            char name[32];
            snprintf(name, sizeof(name), "extra_value_%d", r);
            int row = r;
            ctor.BindFunc(Rml::String(name), [row](Rml::Variant& v) {
                int page = Menu_Page;
                if (page < 0 || page > 3)
                    page = 0;
                int val = save_w[1].extra_option.contents[page][row];
                v = get_value_label(page, row, val);
            });
        }
    }

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi ExtraOption] Data model registered");
}

// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_extra_option_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    int page = (int)Menu_Page;
    if (page < 0 || page > 3)
        page = 0;

    if (page != s_cache.page) {
        s_cache.page = page;
        s_model_handle.DirtyVariable("extra_page");
        s_model_handle.DirtyVariable("extra_row_count");
        // All labels and values change on page switch
        for (int r = 0; r < 7; r++) {
            char name[32];
            snprintf(name, sizeof(name), "extra_label_%d", r);
            s_model_handle.DirtyVariable(name);
            snprintf(name, sizeof(name), "extra_value_%d", r);
            s_model_handle.DirtyVariable(name);
        }
    }

    int cur = (int)Menu_Cursor_Y[0];
    if (cur != s_cache.cursor) {
        s_cache.cursor = cur;
        s_model_handle.DirtyVariable("extra_cursor");
    }

    // Check values on current page for changes
    for (int r = 0; r < (int)Ex_Page_Data[page]; r++) {
        int val = save_w[1].extra_option.contents[page][r];
        if (val != s_cache.values[page][r]) {
            s_cache.values[page][r] = val;
            char name[32];
            snprintf(name, sizeof(name), "extra_value_%d", r);
            s_model_handle.DirtyVariable(name);
        }
    }
}

// ─── Show / Hide ─────────────────────────────────────────────────
extern "C" void rmlui_extra_option_show(void) {
    rmlui_wrapper_show_document("extra_option");
    if (s_model_handle) {
        s_model_handle.DirtyVariable("extra_page");
        s_model_handle.DirtyVariable("extra_cursor");
        s_model_handle.DirtyVariable("extra_row_count");
        for (int r = 0; r < 7; r++) {
            char name[32];
            snprintf(name, sizeof(name), "extra_label_%d", r);
            s_model_handle.DirtyVariable(name);
            snprintf(name, sizeof(name), "extra_value_%d", r);
            s_model_handle.DirtyVariable(name);
        }
    }
}

extern "C" void rmlui_extra_option_hide(void) {
    rmlui_wrapper_hide_document("extra_option");
}

// ─── Shutdown ────────────────────────────────────────────────────
extern "C" void rmlui_extra_option_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_document("extra_option");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
        if (ctx)
            ctx->RemoveDataModel("extra_option");
        s_model_registered = false;
    }
}
