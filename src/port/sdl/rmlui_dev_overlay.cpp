/**
 * @file rmlui_dev_overlay.cpp
 * @brief RmlUi developer overlay — live element position & style inspector.
 *
 * Data model "dev_overlay" registered on the window context (Phase 2).
 * Enumerates all loaded RmlUi documents, walks their DOM trees, and
 * exposes per-element CSS property overrides via data-bound controls.
 *
 * Toggled with F12.
 */
#include "port/sdl/rmlui_dev_overlay.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

// ── Globals ───────────────────────────────────────────────────
extern "C" bool show_dev_overlay = false;

// ── Internal types ────────────────────────────────────────────

struct DocEntry {
    Rml::String name;
    Rml::String context_label; // "win" or "game"
    bool visible;
    int index;
};

struct ElemEntry {
    Rml::String label;
    int depth;
    int index;
    Rml::Element* ptr;
};

// ── Module state ──────────────────────────────────────────────

static Rml::DataModelHandle s_model;

static std::vector<DocEntry> s_docs;
static std::vector<ElemEntry> s_elements;
static std::vector<Rml::ElementDocument*> s_doc_ptrs;

static int s_selected_doc = -1;
static int s_selected_elem = -1;
static Rml::Element* s_active_elem = nullptr;

// CSS property working values
static float s_margin_top = 0, s_margin_right = 0, s_margin_bottom = 0, s_margin_left = 0;
static float s_padding_top = 0, s_padding_right = 0, s_padding_bottom = 0, s_padding_left = 0;
static float s_font_size = 14;
static float s_opacity = 1.0f;
static Rml::String s_width_str = "";
static Rml::String s_height_str = "";
static Rml::String s_color_hex = "#ffffff";
static Rml::String s_elem_path = "(none)";
static bool s_elem_visible = true;

// ── Helpers ───────────────────────────────────────────────────

static Rml::String extract_basename(const Rml::String& src) {
    auto pos = src.rfind('/');
    auto pos2 = src.rfind('\\');
    if (pos2 != Rml::String::npos && (pos == Rml::String::npos || pos2 > pos))
        pos = pos2;
    Rml::String basename = (pos != Rml::String::npos) ? src.substr(pos + 1) : src;
    auto dot = basename.rfind('.');
    if (dot != Rml::String::npos)
        basename = basename.substr(0, dot);
    return basename;
}

static Rml::String make_label(Rml::Element* el) {
    Rml::String label = el->GetTagName();
    const Rml::String& id = el->GetId();
    if (!id.empty()) label += "#" + id;
    const Rml::String& cls = el->GetClassNames();
    if (!cls.empty()) label += "." + cls;
    return label;
}

// Calculate depth of element in the DOM tree
static int get_depth(Rml::Element* el) {
    int d = 0;
    Rml::Element* p = el->GetParentNode();
    while (p) { d++; p = p->GetParentNode(); }
    return d;
}

static float read_dp(Rml::Element* el, const Rml::String& prop) {
    const Rml::Property* p = el->GetLocalProperty(prop);
    if (!p) return 0;
    return p->Get<float>();
}

static void set_dp(Rml::Element* el, const Rml::String& prop, float val) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.1fdp", val);
    el->SetProperty(prop, Rml::String(buf));
}

static void snapshot_element(Rml::Element* el) {
    if (!el) return;
    s_margin_top = read_dp(el, "margin-top");
    s_margin_right = read_dp(el, "margin-right");
    s_margin_bottom = read_dp(el, "margin-bottom");
    s_margin_left = read_dp(el, "margin-left");
    s_padding_top = read_dp(el, "padding-top");
    s_padding_right = read_dp(el, "padding-right");
    s_padding_bottom = read_dp(el, "padding-bottom");
    s_padding_left = read_dp(el, "padding-left");
    const Rml::Property* fs = el->GetLocalProperty("font-size");
    s_font_size = fs ? fs->Get<float>() : 14.0f;
    const Rml::Property* op = el->GetLocalProperty("opacity");
    s_opacity = op ? op->Get<float>() : 1.0f;
    s_elem_visible = el->IsVisible();
    const Rml::Property* w = el->GetLocalProperty("width");
    s_width_str = w ? w->ToString() : "";
    const Rml::Property* h = el->GetLocalProperty("height");
    s_height_str = h ? h->ToString() : "";
    const Rml::Property* col = el->GetLocalProperty("color");
    s_color_hex = col ? col->ToString() : "#ffffff";
    s_elem_path = make_label(el);
}

static void apply_to_element(Rml::Element* el) {
    if (!el) return;
    set_dp(el, "margin-top", s_margin_top);
    set_dp(el, "margin-right", s_margin_right);
    set_dp(el, "margin-bottom", s_margin_bottom);
    set_dp(el, "margin-left", s_margin_left);
    set_dp(el, "padding-top", s_padding_top);
    set_dp(el, "padding-right", s_padding_right);
    set_dp(el, "padding-bottom", s_padding_bottom);
    set_dp(el, "padding-left", s_padding_left);
    char buf[64];
    snprintf(buf, sizeof(buf), "%.1fdp", s_font_size);
    el->SetProperty("font-size", Rml::String(buf));
    snprintf(buf, sizeof(buf), "%.2f", s_opacity);
    el->SetProperty("opacity", Rml::String(buf));
    if (!s_width_str.empty()) el->SetProperty("width", s_width_str);
    if (!s_height_str.empty()) el->SetProperty("height", s_height_str);
    if (!s_color_hex.empty()) el->SetProperty("color", s_color_hex);
}

// ── Document & element enumeration ────────────────────────────

static void refresh_doc_list() {
    s_docs.clear();
    s_doc_ptrs.clear();
    int idx = 0;

    auto add_from = [&](Rml::Context* ctx, const char* ctx_label) {
        if (!ctx) return;
        for (int i = 0; i < ctx->GetNumDocuments(); i++) {
            Rml::ElementDocument* doc = ctx->GetDocument(i);
            if (!doc) continue;
            const Rml::String& src = doc->GetSourceURL();
            if (src.find("dev_overlay") != Rml::String::npos) continue;
            if (src.find("rmlui-debug") != Rml::String::npos) continue;

            DocEntry e;
            Rml::String title = doc->GetTitle();
            e.name = title.empty() ? extract_basename(src) : title;
            if (e.name.empty()) e.name = "(untitled)";
            e.context_label = ctx_label;
            e.visible = doc->IsVisible();
            e.index = idx++;
            s_docs.push_back(e);
            s_doc_ptrs.push_back(doc);
        }
    };

    add_from(static_cast<Rml::Context*>(rmlui_wrapper_get_context()), "win");
    add_from(static_cast<Rml::Context*>(rmlui_wrapper_get_game_context()), "game");
}

static void do_select_doc(int idx);
static void do_select_elem(int idx);

// Use QuerySelectorAll to enumerate ALL elements in a document
static void refresh_elem_list() {
    s_elements.clear();
    s_selected_elem = -1;
    s_active_elem = nullptr;

    if (s_selected_doc < 0 || s_selected_doc >= (int)s_doc_ptrs.size())
        return;

    Rml::ElementDocument* doc = s_doc_ptrs[s_selected_doc];
    if (!doc) return;

    // Use QuerySelectorAll("*") to get ALL descendant elements
    Rml::ElementList all_elements;
    doc->QuerySelectorAll(all_elements, "*");

    int base_depth = get_depth(doc);

    for (size_t i = 0; i < all_elements.size(); i++) {
        Rml::Element* el = all_elements[i];
        const Rml::String& tag = el->GetTagName();
        // Skip internal RmlUi elements
        if (tag == "#text" || tag == "handle" || tag == "scrollbarvertical" ||
            tag == "scrollbarhorizontal" || tag == "sliderarrowdec" ||
            tag == "sliderarrowinc" || tag == "sliderbar" || tag == "slidertrack")
            continue;

        ElemEntry entry;
        entry.label = make_label(el);
        entry.depth = get_depth(el) - base_depth;
        entry.index = (int)s_elements.size();
        entry.ptr = el;
        s_elements.push_back(entry);
    }

    SDL_Log("[DevOverlay] Enumerated %d elements for document '%s'",
            (int)s_elements.size(),
            s_selected_doc < (int)s_docs.size() ? s_docs[s_selected_doc].name.c_str() : "?");
}

static void dirty_all_props() {
    if (!s_model) return;
    s_model.DirtyVariable("sel_margin_top");
    s_model.DirtyVariable("sel_margin_right");
    s_model.DirtyVariable("sel_margin_bottom");
    s_model.DirtyVariable("sel_margin_left");
    s_model.DirtyVariable("sel_padding_top");
    s_model.DirtyVariable("sel_padding_right");
    s_model.DirtyVariable("sel_padding_bottom");
    s_model.DirtyVariable("sel_padding_left");
    s_model.DirtyVariable("sel_font_size");
    s_model.DirtyVariable("sel_opacity");
    s_model.DirtyVariable("sel_width");
    s_model.DirtyVariable("sel_height");
    s_model.DirtyVariable("sel_color");
    s_model.DirtyVariable("sel_visible");
    s_model.DirtyVariable("sel_path");
    s_model.DirtyVariable("has_selection");
}

static void do_select_doc(int idx) {
    SDL_Log("[DevOverlay] Selecting document %d", idx);
    s_selected_doc = idx;
    refresh_elem_list();
    if (s_model) {
        s_model.DirtyVariable("selected_doc");
        s_model.DirtyVariable("elem_list");
        s_model.DirtyVariable("elem_count");
    }
    dirty_all_props();
}

static void do_select_elem(int idx) {
    SDL_Log("[DevOverlay] Selecting element %d", idx);
    if (idx >= 0 && idx < (int)s_elements.size()) {
        s_selected_elem = idx;
        s_active_elem = s_elements[idx].ptr;
        snapshot_element(s_active_elem);
        SDL_Log("[DevOverlay] Selected: %s", s_elem_path.c_str());
    } else {
        s_selected_elem = -1;
        s_active_elem = nullptr;
    }
    if (s_model) {
        s_model.DirtyVariable("selected_elem");
    }
    dirty_all_props();
}

// ── Init ──────────────────────────────────────────────────────

extern "C" void rmlui_dev_overlay_init() {
    void* raw_ctx = rmlui_wrapper_get_context();
    if (!raw_ctx) {
        SDL_Log("[RmlUi DevOverlay] No context available");
        return;
    }
    Rml::Context* ctx = static_cast<Rml::Context*>(raw_ctx);

    Rml::DataModelConstructor c = ctx->CreateDataModel("dev_overlay");
    if (!c) {
        SDL_Log("[RmlUi DevOverlay] Failed to create data model");
        return;
    }

    // ── Document list ──
    if (auto sh = c.RegisterStruct<DocEntry>()) {
        sh.RegisterMember("name", &DocEntry::name);
        sh.RegisterMember("context_label", &DocEntry::context_label);
        sh.RegisterMember("visible", &DocEntry::visible);
        sh.RegisterMember("index", &DocEntry::index);
    }
    c.RegisterArray<std::vector<DocEntry>>();
    c.Bind("doc_list", &s_docs);

    c.BindFunc(
        "selected_doc",
        [](Rml::Variant& v) { v = s_selected_doc; },
        [](const Rml::Variant& v) {
            int new_val = v.Get<int>();
            if (new_val != s_selected_doc) {
                do_select_doc(new_val);
            }
        });

    c.BindFunc("doc_count", [](Rml::Variant& v) { v = (int)s_docs.size(); });

    // ── Element list ──
    if (auto sh = c.RegisterStruct<ElemEntry>()) {
        sh.RegisterMember("label", &ElemEntry::label);
        sh.RegisterMember("depth", &ElemEntry::depth);
        sh.RegisterMember("index", &ElemEntry::index);
    }
    c.RegisterArray<std::vector<ElemEntry>>();
    c.Bind("elem_list", &s_elements);

    c.BindFunc(
        "selected_elem",
        [](Rml::Variant& v) { v = s_selected_elem; },
        [](const Rml::Variant& v) {
            int new_val = v.Get<int>();
            if (new_val != s_selected_elem) {
                do_select_elem(new_val);
            }
        });

    c.BindFunc("elem_count", [](Rml::Variant& v) { v = (int)s_elements.size(); });
    c.BindFunc("has_selection", [](Rml::Variant& v) { v = (s_active_elem != nullptr); });

    // ── Selected element properties ──

#define BIND_PROP(var_name, cpp_var)                                       \
    c.BindFunc(                                                            \
        var_name,                                                          \
        [](Rml::Variant& v) { v = cpp_var; },                             \
        [](const Rml::Variant& v) {                                        \
            cpp_var = v.Get<float>();                                       \
            if (s_active_elem) apply_to_element(s_active_elem);            \
        })

    BIND_PROP("sel_margin_top", s_margin_top);
    BIND_PROP("sel_margin_right", s_margin_right);
    BIND_PROP("sel_margin_bottom", s_margin_bottom);
    BIND_PROP("sel_margin_left", s_margin_left);
    BIND_PROP("sel_padding_top", s_padding_top);
    BIND_PROP("sel_padding_right", s_padding_right);
    BIND_PROP("sel_padding_bottom", s_padding_bottom);
    BIND_PROP("sel_padding_left", s_padding_left);
    BIND_PROP("sel_font_size", s_font_size);
    BIND_PROP("sel_opacity", s_opacity);

#undef BIND_PROP

    c.BindFunc("sel_width",
        [](Rml::Variant& v) { v = s_width_str; },
        [](const Rml::Variant& v) {
            s_width_str = v.Get<Rml::String>();
            if (s_active_elem && !s_width_str.empty()) s_active_elem->SetProperty("width", s_width_str);
        });
    c.BindFunc("sel_height",
        [](Rml::Variant& v) { v = s_height_str; },
        [](const Rml::Variant& v) {
            s_height_str = v.Get<Rml::String>();
            if (s_active_elem && !s_height_str.empty()) s_active_elem->SetProperty("height", s_height_str);
        });
    c.BindFunc("sel_color",
        [](Rml::Variant& v) { v = s_color_hex; },
        [](const Rml::Variant& v) {
            s_color_hex = v.Get<Rml::String>();
            if (s_active_elem && !s_color_hex.empty()) s_active_elem->SetProperty("color", s_color_hex);
        });
    c.BindFunc("sel_visible",
        [](Rml::Variant& v) { v = s_elem_visible; },
        [](const Rml::Variant& v) {
            s_elem_visible = v.Get<bool>();
            if (s_active_elem) s_active_elem->SetProperty("visibility", s_elem_visible ? "visible" : "hidden");
        });
    c.BindFunc("sel_path", [](Rml::Variant& v) { v = s_elem_path; });

    // ── Event callbacks ──

    c.BindEventCallback("refresh_docs", [](Rml::DataModelHandle h, Rml::Event&, const Rml::VariantList&) {
        refresh_doc_list();
        h.DirtyVariable("doc_list");
        h.DirtyVariable("doc_count");
        SDL_Log("[DevOverlay] Refreshed: %d documents", (int)s_docs.size());
    });

    c.BindEventCallback("select_doc", [](Rml::DataModelHandle h, Rml::Event&, const Rml::VariantList& args) {
        if (args.empty()) { SDL_Log("[DevOverlay] select_doc: no args!"); return; }
        int idx = args[0].Get<int>();
        SDL_Log("[DevOverlay] select_doc event: index=%d", idx);
        do_select_doc(idx);
        h.DirtyVariable("selected_doc");
        h.DirtyVariable("elem_list");
        h.DirtyVariable("elem_count");
    });

    c.BindEventCallback("select_elem", [](Rml::DataModelHandle h, Rml::Event&, const Rml::VariantList& args) {
        if (args.empty()) { SDL_Log("[DevOverlay] select_elem: no args!"); return; }
        int idx = args[0].Get<int>();
        SDL_Log("[DevOverlay] select_elem event: index=%d", idx);
        do_select_elem(idx);
        h.DirtyVariable("selected_elem");
    });

    c.BindEventCallback("reset_elem", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
        if (!s_active_elem) return;
        const char* props[] = {"margin-top","margin-right","margin-bottom","margin-left",
            "padding-top","padding-right","padding-bottom","padding-left",
            "font-size","opacity","width","height","color","visibility"};
        for (auto p : props) s_active_elem->RemoveProperty(p);
        snapshot_element(s_active_elem);
        dirty_all_props();
        SDL_Log("[DevOverlay] Reset: %s", s_elem_path.c_str());
    });

    c.BindEventCallback("copy_rcss", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
        if (!s_active_elem) return;
        std::ostringstream ss;
        ss << "/* Dev Overlay export for: " << s_elem_path.c_str() << " */\n";
        ss << make_label(s_active_elem).c_str() << " {\n";
        auto emit = [&](const char* prop) {
            const Rml::Property* p = s_active_elem->GetLocalProperty(prop);
            if (p) ss << "    " << prop << ": " << p->ToString().c_str() << ";\n";
        };
        emit("margin-top"); emit("margin-right"); emit("margin-bottom"); emit("margin-left");
        emit("padding-top"); emit("padding-right"); emit("padding-bottom"); emit("padding-left");
        emit("font-size"); emit("opacity"); emit("width"); emit("height");
        emit("color"); emit("visibility");
        ss << "}\n";
        SDL_Log("[DevOverlay] ─── RCSS Export ───\n%s", ss.str().c_str());
        SDL_SetClipboardText(ss.str().c_str());
    });

    s_model = c.GetModelHandle();
    refresh_doc_list();
    SDL_Log("[RmlUi DevOverlay] Init complete: %d docs", (int)s_docs.size());
}

// ── Per-frame update ──────────────────────────────────────────

static bool s_first_open = true;

extern "C" void rmlui_dev_overlay_update() {
    if (!s_model) return;

    // On first open, refresh and auto-select first visible document
    if (s_first_open && show_dev_overlay) {
        s_first_open = false;
        refresh_doc_list();
        s_model.DirtyVariable("doc_list");
        s_model.DirtyVariable("doc_count");

        // Auto-select first visible doc
        for (int i = 0; i < (int)s_docs.size(); i++) {
            if (s_docs[i].visible) {
                do_select_doc(i);
                s_model.DirtyVariable("selected_doc");
                s_model.DirtyVariable("elem_list");
                s_model.DirtyVariable("elem_count");
                break;
            }
        }
    }

    // Track close/reopen
    if (!show_dev_overlay) {
        s_first_open = true;
    }

    // Periodic refresh
    static int s_counter = 0;
    if (++s_counter >= 60) { // every ~1s at 60fps
        s_counter = 0;
        refresh_doc_list();
        s_model.DirtyVariable("doc_list");
        s_model.DirtyVariable("doc_count");
    }
}

// ── Shutdown ──────────────────────────────────────────────────

extern "C" void rmlui_dev_overlay_shutdown() {
    s_model = Rml::DataModelHandle();
    s_docs.clear();
    s_doc_ptrs.clear();
    s_elements.clear();
    s_active_elem = nullptr;
    s_selected_doc = -1;
    s_selected_elem = -1;
}
