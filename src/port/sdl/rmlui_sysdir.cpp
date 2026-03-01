/**
 * @file rmlui_sysdir.cpp
 * @brief RmlUi System Direction (Dipswitch) data model.
 *
 * Replaces CPS3's effect objects in System_Direction() and Direction_Menu()
 * with an RmlUi overlay showing the paged dipswitch toggle table.
 *
 * Key globals:
 *   system_dir[1].contents[page][row] — toggle values
 *   Menu_Cursor_Y[0] — cursor position
 *   Menu_Page, Page_Max — pagination
 *   Page_Data[10] — rows per page
 *   Letter_Data_51[10][6][4] — value label strings
 *   msgSysDirTbl — row label + description strings
 *   Direction_Working[1], Convert_Buff[3][0][0] — top-level page selector
 */

#include "port/sdl/rmlui_sysdir.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <cstring>

extern "C" {
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/menu/dir_data.h"
#include "sf33rd/Source/Game/message/en/msgtable_en.h"
#include "sf33rd/Source/Game/system/work_sys.h"
} // extern "C"

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

// Track whether we're in the top-level nav or a sub-page
static bool s_in_subpage = false;

struct SysdirCache {
    int cursor_y;
    int page;
    int page_max;
    int page_count;  // rows on current page
    int dir_working; // top-level page selector value
    bool in_subpage;
    // Per-row label + value cache (max 6 rows per page)
    char row_labels[6][64];
    char row_values[6][16];
    // Description for focused row
    char desc[128];
};
static SysdirCache s_cache = {};

#define DIRTY_INT(nm, expr)                                                                                            \
    do {                                                                                                               \
        int _v = (expr);                                                                                               \
        if (_v != s_cache.nm) {                                                                                        \
            s_cache.nm = _v;                                                                                           \
            s_model_handle.DirtyVariable(#nm);                                                                         \
        }                                                                                                              \
    } while (0)

#define DIRTY_BOOL(nm, expr)                                                                                           \
    do {                                                                                                               \
        bool _v = (expr);                                                                                              \
        if (_v != s_cache.nm) {                                                                                        \
            s_cache.nm = _v;                                                                                           \
            s_model_handle.DirtyVariable(#nm);                                                                         \
        }                                                                                                              \
    } while (0)

// ─── Helpers ─────────────────────────────────────────────────────

/// Build the label string for a given page/row from msgSysDirTbl.
/// Even indices (page*12 + row*2) are the label entries.
/// Strips trailing dots (used for fixed-width font alignment in native).
static const char* get_row_label(int page, int row) {
    static char s_label_buf[64];
    int idx = page * 12 + row * 2;
    if (!msgSysDirTbl[0] || !msgSysDirTbl[0]->msgAdr)
        return "";
    s8** entry = msgSysDirTbl[0]->msgAdr[idx];
    if (!entry || !entry[0])
        return "";
    const char* raw = (const char*)entry[0];
    /* Copy and strip trailing dots */
    size_t len = strlen(raw);
    if (len >= sizeof(s_label_buf))
        len = sizeof(s_label_buf) - 1;
    memcpy(s_label_buf, raw, len);
    s_label_buf[len] = '\0';
    while (len > 0 && s_label_buf[len - 1] == '.') {
        s_label_buf[--len] = '\0';
    }
    return s_label_buf;
}

/// Build the description string for a given page/row from msgSysDirTbl.
/// Odd indices (page*12 + row*2 + 1) are the description entries.
static void get_row_desc(int page, int row, char* buf, int bufsize) {
    int idx = page * 12 + row * 2 + 1;
    if (!msgSysDirTbl[0] || !msgSysDirTbl[0]->msgAdr) {
        buf[0] = '\0';
        return;
    }
    s8 num_lines = msgSysDirTbl[0]->msgNum[idx];
    s8** entry = msgSysDirTbl[0]->msgAdr[idx];
    if (!entry) {
        buf[0] = '\0';
        return;
    }
    buf[0] = '\0';
    for (int i = 0; i < num_lines && entry[i]; i++) {
        if (i > 0)
            strncat(buf, " ", bufsize - strlen(buf) - 1);
        strncat(buf, (const char*)entry[i], bufsize - strlen(buf) - 1);
    }
}

/// Get the value label string for a given page/row/value.
static const char* get_value_label(int page, int row, int value) {
    if (page < 0 || page >= 10 || row < 0 || row >= 6 || value < 0 || value >= 4)
        return "";
    s8* lbl = Letter_Data_51[page][row][value];
    return lbl ? (const char*)lbl : "";
}

// ─── Init ────────────────────────────────────────────────────────
extern "C" void rmlui_sysdir_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("sysdir");
    if (!ctor)
        return;

    ctor.BindFunc("cursor_y", [](Rml::Variant& v) { v = (int)Menu_Cursor_Y[0]; });
    ctor.BindFunc("page", [](Rml::Variant& v) { v = (int)Menu_Page; });
    ctor.BindFunc("page_max", [](Rml::Variant& v) { v = (int)Page_Max; });
    ctor.BindFunc("page_count", [](Rml::Variant& v) {
        if (Menu_Page >= 0 && Menu_Page < 10)
            v = (int)Page_Data[Menu_Page];
        else
            v = 0;
    });
    ctor.BindFunc("dir_working", [](Rml::Variant& v) { v = (int)Convert_Buff[3][0][0]; });
    ctor.BindFunc("in_subpage", [](Rml::Variant& v) { v = s_in_subpage; });

    // Per-row labels (0-5)
    for (int i = 0; i < 6; i++) {
        Rml::String name = "row_label_" + Rml::ToString(i);
        int idx = i; // capture
        ctor.BindFunc(name, [idx](Rml::Variant& v) {
            if (!s_in_subpage) {
                v = Rml::String("");
                return;
            }
            int pg = Menu_Page;
            if (pg < 0 || pg >= 10 || idx >= Page_Data[pg]) {
                v = Rml::String("");
                return;
            }
            v = Rml::String(get_row_label(pg, idx));
        });
    }

    // Per-row values (0-5)
    for (int i = 0; i < 6; i++) {
        Rml::String name = "row_value_" + Rml::ToString(i);
        int idx = i;
        ctor.BindFunc(name, [idx](Rml::Variant& v) {
            if (!s_in_subpage) {
                v = Rml::String("");
                return;
            }
            int pg = Menu_Page;
            if (pg < 0 || pg >= 10 || idx >= Page_Data[pg]) {
                v = Rml::String("");
                return;
            }
            int val = system_dir[1].contents[pg][idx];
            v = Rml::String(get_value_label(pg, idx, val));
        });
    }

    // The last row's value (page nav: ←/EXIT/→)
    ctor.BindFunc("nav_row_value", [](Rml::Variant& v) {
        if (!s_in_subpage) {
            v = Rml::String("");
            return;
        }
        int pg = Menu_Page;
        if (pg < 0 || pg >= 10) {
            v = Rml::String("");
            return;
        }
        int menu_max = Page_Data[pg];
        int val = system_dir[1].contents[pg][menu_max];
        switch (val) {
        case 0:
            v = Rml::String("\xe2\x97\x80");
            break; // ◀
        case 1:
            v = Rml::String("EXIT");
            break;
        case 2:
            v = Rml::String("\xe2\x96\xb6");
            break; // ▶
        default:
            v = Rml::String("EXIT");
            break;
        }
    });

    // Description text for focused row
    ctor.BindFunc("row_desc", [](Rml::Variant& v) {
        if (!s_in_subpage) {
            v = Rml::String("");
            return;
        }
        int pg = Menu_Page;
        int row = Menu_Cursor_Y[0];
        if (pg < 0 || pg >= 10) {
            v = Rml::String("");
            return;
        }
        int menu_max = Page_Data[pg];
        if (row >= menu_max) {
            // Nav row — show nav description
            int nav_val = system_dir[1].contents[pg][menu_max];
            // msgSysDirAdr indices 116..118 = ← / EXIT / →
            int desc_idx = 116 + nav_val; // 0x74 + val
            if (msgSysDirTbl[0] && msgSysDirTbl[0]->msgAdr) {
                s8** entry = msgSysDirTbl[0]->msgAdr[desc_idx];
                if (entry && entry[0]) {
                    v = Rml::String((const char*)entry[0]);
                    return;
                }
            }
            v = Rml::String("");
            return;
        }
        char buf[128];
        get_row_desc(pg, row, buf, sizeof(buf));
        v = Rml::String(buf);
    });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi SysDir] Data model registered");
}

// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_sysdir_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    DIRTY_INT(cursor_y, (int)Menu_Cursor_Y[0]);
    DIRTY_INT(page, (int)Menu_Page);
    DIRTY_INT(page_max, (int)Page_Max);
    DIRTY_INT(dir_working, (int)Convert_Buff[3][0][0]);
    DIRTY_BOOL(in_subpage, s_in_subpage);

    int pg = Menu_Page;
    if (pg >= 0 && pg < 10) {
        int pc = (int)Page_Data[pg];
        DIRTY_INT(page_count, pc);
    }

    // Always dirty the row labels/values and description since the underlying
    // data can change from input without any cache-able scalar diff.
    // This is cheap — RmlUi only re-renders if DOM actually changes.
    if (s_in_subpage) {
        s_model_handle.DirtyVariable("row_label_0");
        s_model_handle.DirtyVariable("row_label_1");
        s_model_handle.DirtyVariable("row_label_2");
        s_model_handle.DirtyVariable("row_label_3");
        s_model_handle.DirtyVariable("row_label_4");
        s_model_handle.DirtyVariable("row_label_5");
        s_model_handle.DirtyVariable("row_value_0");
        s_model_handle.DirtyVariable("row_value_1");
        s_model_handle.DirtyVariable("row_value_2");
        s_model_handle.DirtyVariable("row_value_3");
        s_model_handle.DirtyVariable("row_value_4");
        s_model_handle.DirtyVariable("row_value_5");
        s_model_handle.DirtyVariable("nav_row_value");
        s_model_handle.DirtyVariable("row_desc");
    }
}

// ─── Show / Hide ─────────────────────────────────────────────────
extern "C" void rmlui_sysdir_show(void) {
    s_in_subpage = false;
    rmlui_wrapper_show_game_document("sysdir");
}

extern "C" void rmlui_sysdir_hide(void) {
    s_in_subpage = false;
    rmlui_wrapper_hide_game_document("sysdir");
}

// Called from Direction_Menu when entering sub-pages
extern "C" void rmlui_sysdir_enter_subpage(void) {
    s_in_subpage = true;
}

// Called from Direction_Menu when exiting sub-pages
extern "C" void rmlui_sysdir_exit_subpage(void) {
    s_in_subpage = false;
}

// ─── Shutdown ────────────────────────────────────────────────────
extern "C" void rmlui_sysdir_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("sysdir");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("sysdir");
        s_model_registered = false;
    }
}

#undef DIRTY_INT
#undef DIRTY_BOOL
