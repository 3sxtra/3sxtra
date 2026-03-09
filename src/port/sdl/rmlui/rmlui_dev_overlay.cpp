/**
 * @file rmlui_dev_overlay.cpp
 * @brief RmlUi developer overlay — live element position & style inspector.
 *
 * Data model "dev_overlay" registered on the window context (Phase 2).
 * Enumerates all loaded RmlUi documents, walks their DOM trees, and
 * exposes per-element CSS property overrides via data-bound controls.
 *
 * Toggled with F9.
 */
#include "port/sdl/rmlui/rmlui_dev_overlay.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

// ── Globals ───────────────────────────────────────────────────
extern "C" {
bool show_dev_overlay = false;
}

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
// Tier 1 additions
static Rml::String s_position_str = "static";
static Rml::String s_top_str = "", s_right_str = "", s_bottom_str = "", s_left_str = "";
static Rml::String s_bg_color = "";
static float s_border_width = 0;
static Rml::String s_border_color = "";
static float s_border_radius = 0;
static Rml::String s_display_str = "block";
// Tier 2 additions
static Rml::String s_text_align = "left";
static float s_line_height = 1.2f;
static float s_letter_spacing = 0;
static Rml::String s_flex_direction = "row";
static Rml::String s_justify_content = "flex-start";
static Rml::String s_align_items = "stretch";
static float s_gap = 0;
static Rml::String s_overflow = "visible";
// Tier 3: read-only computed info
static Rml::String s_info_tag = "";
static Rml::String s_info_id = "";
static Rml::String s_info_classes = "";
static Rml::String s_info_box = "";
static int s_info_children = 0;
static Rml::String s_info_source = "";

// Guard flag: when true, property setters skip re-applying to the element.
// Set during reset to prevent the dirty→setter cycle from re-creating
// local overrides that were just removed.
static bool s_suppress_apply = false;

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
    if (!id.empty())
        label += "#" + id;
    const Rml::String& cls = el->GetClassNames();
    if (!cls.empty())
        label += "." + cls;
    return label;
}

// Calculate depth of element in the DOM tree
static int get_depth(Rml::Element* el) {
    int d = 0;
    Rml::Element* p = el->GetParentNode();
    while (p) {
        d++;
        p = p->GetParentNode();
    }
    return d;
}

static float read_dp(Rml::Element* el, const Rml::String& prop) {
    // Use GetProperty() (resolved) instead of GetLocalProperty() (local-only)
    // so we read the actual computed value including stylesheet rules.
    const Rml::Property* p = el->GetProperty(prop);
    if (!p)
        return 0;
    return p->Get<float>();
}

static void set_dp(Rml::Element* el, const Rml::String& prop, float val) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.1fdp", val);
    el->SetProperty(prop, Rml::String(buf));
}

static void snapshot_element(Rml::Element* el) {
    if (!el)
        return;
    // Use resolved properties (GetProperty) so sliders show actual values,
    // not just explicitly-set local overrides.
    s_margin_top = read_dp(el, "margin-top");
    s_margin_right = read_dp(el, "margin-right");
    s_margin_bottom = read_dp(el, "margin-bottom");
    s_margin_left = read_dp(el, "margin-left");
    s_padding_top = read_dp(el, "padding-top");
    s_padding_right = read_dp(el, "padding-right");
    s_padding_bottom = read_dp(el, "padding-bottom");
    s_padding_left = read_dp(el, "padding-left");
    const Rml::Property* fs = el->GetProperty("font-size");
    s_font_size = fs ? fs->Get<float>() : 14.0f;
    const Rml::Property* op = el->GetProperty("opacity");
    s_opacity = op ? op->Get<float>() : 1.0f;
    // Read element's own visibility CSS property, not IsVisible() which
    // includes ancestor visibility state.
    const Rml::Property* vis = el->GetProperty("visibility");
    s_elem_visible = !vis || vis->ToString() != "hidden";
    const Rml::Property* w = el->GetLocalProperty("width");
    s_width_str = w ? w->ToString() : "";
    const Rml::Property* h = el->GetLocalProperty("height");
    s_height_str = h ? h->ToString() : "";
    const Rml::Property* col = el->GetProperty("color");
    s_color_hex = col ? col->ToString() : "#ffffff";
    s_elem_path = make_label(el);
    // Tier 1 additions
    const Rml::Property* pos = el->GetProperty("position");
    s_position_str = pos ? pos->ToString() : "static";
    const Rml::Property* pt = el->GetLocalProperty("top");
    s_top_str = pt ? pt->ToString() : "";
    const Rml::Property* pr = el->GetLocalProperty("right");
    s_right_str = pr ? pr->ToString() : "";
    const Rml::Property* pb = el->GetLocalProperty("bottom");
    s_bottom_str = pb ? pb->ToString() : "";
    const Rml::Property* pl = el->GetLocalProperty("left");
    s_left_str = pl ? pl->ToString() : "";
    const Rml::Property* bgc = el->GetProperty("background-color");
    s_bg_color = bgc ? bgc->ToString() : "";
    s_border_width = read_dp(el, "border-top-width");
    const Rml::Property* bc = el->GetProperty("border-top-color");
    s_border_color = bc ? bc->ToString() : "";
    s_border_radius = read_dp(el, "border-top-left-radius");
    const Rml::Property* disp = el->GetProperty("display");
    s_display_str = disp ? disp->ToString() : "block";
    // Tier 2 additions
    const Rml::Property* ta = el->GetProperty("text-align");
    s_text_align = ta ? ta->ToString() : "left";
    s_line_height = read_dp(el, "line-height");
    s_letter_spacing = read_dp(el, "letter-spacing");
    const Rml::Property* fd = el->GetProperty("flex-direction");
    s_flex_direction = fd ? fd->ToString() : "row";
    const Rml::Property* jc = el->GetProperty("justify-content");
    s_justify_content = jc ? jc->ToString() : "flex-start";
    const Rml::Property* ai = el->GetProperty("align-items");
    s_align_items = ai ? ai->ToString() : "stretch";
    s_gap = read_dp(el, "gap");
    const Rml::Property* ov = el->GetProperty("overflow-x");
    s_overflow = ov ? ov->ToString() : "visible";
    // Tier 3: computed info (read-only)
    s_info_tag = el->GetTagName();
    s_info_id = el->GetId();
    s_info_classes = el->GetClassNames();
    auto box = el->GetBox();
    char box_buf[128];
    snprintf(box_buf, sizeof(box_buf), "%.0f × %.0f", box.GetSize().x, box.GetSize().y);
    s_info_box = box_buf;
    s_info_children = el->GetNumChildren();
    // Walk up to find document for source URL
    Rml::Element* doc_el = el;
    while (doc_el && doc_el->GetParentNode())
        doc_el = doc_el->GetParentNode();
    Rml::ElementDocument* src_doc = dynamic_cast<Rml::ElementDocument*>(doc_el);
    s_info_source = src_doc ? extract_basename(src_doc->GetSourceURL()) : "";
}

// ── Document & element enumeration ────────────────────────────

static void refresh_doc_list() {
    s_docs.clear();
    s_doc_ptrs.clear();
    int idx = 0;

    auto add_from = [&](Rml::Context* ctx, const char* ctx_label) {
        if (!ctx)
            return;
        for (int i = 0; i < ctx->GetNumDocuments(); i++) {
            Rml::ElementDocument* doc = ctx->GetDocument(i);
            if (!doc)
                continue;
            const Rml::String& src = doc->GetSourceURL();
            if (src.find("dev_overlay") != Rml::String::npos)
                continue;
            if (src.find("rmlui-debug") != Rml::String::npos)
                continue;

            DocEntry e;
            Rml::String title = doc->GetTitle();
            e.name = title.empty() ? extract_basename(src) : title;
            if (e.name.empty())
                e.name = "(untitled)";
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

// ── Grow-only element list management ────────────────────────
// RmlUi's data-for reconciliation evaluates OLD DOM children before
// removing them.  If the backing array shrinks, stale children
// access out-of-bounds indices ("Data array index out of bounds").
//
// Fix: the backing vector NEVER shrinks while the overlay is open.
// A separate s_real_elem_count tracks how many entries are valid.
// The RML uses data-if="el.index < elem_count" to hide padding.
// Since the array only grows, data-for reconciliation can always
// access any previously-valid index without OOB.

static int s_real_elem_count = 0;
static bool s_needs_rebuild = false;

static void do_rebuild_elem_list() {
    if (s_selected_doc < 0 || s_selected_doc >= (int)s_doc_ptrs.size()) {
        // No valid doc — zero out valid range but keep array size
        for (int i = 0; i < s_real_elem_count; i++) {
            s_elements[i] = {"", 0, i, nullptr};
        }
        s_real_elem_count = 0;
        s_selected_elem = -1;
        s_active_elem = nullptr;
        if (s_model) {
            s_model.DirtyVariable("elem_list");
            s_model.DirtyVariable("elem_count");
        }
        return;
    }

    Rml::ElementDocument* doc = s_doc_ptrs[s_selected_doc];
    if (!doc)
        return;

    Rml::ElementList all_elements;
    doc->QuerySelectorAll(all_elements, "*");

    int base_depth = get_depth(doc);

    // Build new list in a temp vector
    std::vector<ElemEntry> new_elems;
    for (size_t i = 0; i < all_elements.size(); i++) {
        Rml::Element* el = all_elements[i];
        const Rml::String& tag = el->GetTagName();
        if (tag == "#text" || tag == "handle" || tag == "scrollbarvertical" || tag == "scrollbarhorizontal" ||
            tag == "sliderarrowdec" || tag == "sliderarrowinc" || tag == "sliderbar" || tag == "slidertrack")
            continue;

        ElemEntry entry;
        entry.label = make_label(el);
        entry.depth = get_depth(el) - base_depth;
        entry.index = (int)new_elems.size();
        entry.ptr = el;
        new_elems.push_back(entry);
    }

    // Grow backing array if needed (never shrink)
    size_t needed = new_elems.size();
    if (needed > s_elements.size()) {
        s_elements.resize(needed);
    }

    // Overwrite with new data
    for (size_t i = 0; i < needed; i++) {
        s_elements[i] = new_elems[i];
    }
    // Mark excess entries as padding (valid structs, hidden by data-if)
    for (size_t i = needed; i < s_elements.size(); i++) {
        s_elements[i] = {"", 0, (int)i, nullptr};
    }

    s_real_elem_count = (int)needed;
    s_selected_elem = -1;
    s_active_elem = nullptr;

    if (s_model) {
        s_model.DirtyVariable("elem_list");
        s_model.DirtyVariable("elem_count");
    }

    SDL_Log("[DevOverlay] Enumerated %d elements for document '%s'",
            s_real_elem_count,
            s_selected_doc < (int)s_docs.size() ? s_docs[s_selected_doc].name.c_str() : "?");
}

static void dirty_all_props() {
    if (!s_model)
        return;
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
    // Tier 1
    s_model.DirtyVariable("sel_position");
    s_model.DirtyVariable("sel_top");
    s_model.DirtyVariable("sel_right");
    s_model.DirtyVariable("sel_bottom");
    s_model.DirtyVariable("sel_left");
    s_model.DirtyVariable("sel_bg_color");
    s_model.DirtyVariable("sel_border_width");
    s_model.DirtyVariable("sel_border_color");
    s_model.DirtyVariable("sel_border_radius");
    s_model.DirtyVariable("sel_display");
    // Tier 2
    s_model.DirtyVariable("sel_text_align");
    s_model.DirtyVariable("sel_line_height");
    s_model.DirtyVariable("sel_letter_spacing");
    s_model.DirtyVariable("sel_flex_direction");
    s_model.DirtyVariable("sel_justify_content");
    s_model.DirtyVariable("sel_align_items");
    s_model.DirtyVariable("sel_gap");
    s_model.DirtyVariable("sel_overflow");
    // Tier 3 (read-only)
    s_model.DirtyVariable("info_tag");
    s_model.DirtyVariable("info_id");
    s_model.DirtyVariable("info_classes");
    s_model.DirtyVariable("info_box");
    s_model.DirtyVariable("info_children");
    s_model.DirtyVariable("info_source");
}

// Called from event callbacks (runs DURING Context::Update).
// Do NOT modify s_elements here — schedule rebuild for next update.
static void do_select_doc(int idx) {
    SDL_Log("[DevOverlay] Scheduling document %d", idx);
    s_selected_doc = idx;
    s_needs_rebuild = true;
    if (s_model) {
        s_model.DirtyVariable("selected_doc");
    }
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

    c.BindFunc("elem_count", [](Rml::Variant& v) { v = s_real_elem_count; });
    c.BindFunc("has_selection", [](Rml::Variant& v) { v = (s_active_elem != nullptr); });

    // ── Selected element properties ──
    // Each setter applies ONLY its own CSS property (not all of them).

#define BIND_DP(var_name, cpp_var, css_prop)                                                                           \
    c.BindFunc(                                                                                                        \
        var_name,                                                                                                      \
        [](Rml::Variant& v) { v = cpp_var; },                                                                          \
        [](const Rml::Variant& v) {                                                                                    \
            cpp_var = v.Get<float>();                                                                                  \
            if (s_active_elem && !s_suppress_apply)                                                                    \
                set_dp(s_active_elem, css_prop, cpp_var);                                                              \
        })

    BIND_DP("sel_margin_top", s_margin_top, "margin-top");
    BIND_DP("sel_margin_right", s_margin_right, "margin-right");
    BIND_DP("sel_margin_bottom", s_margin_bottom, "margin-bottom");
    BIND_DP("sel_margin_left", s_margin_left, "margin-left");
    BIND_DP("sel_padding_top", s_padding_top, "padding-top");
    BIND_DP("sel_padding_right", s_padding_right, "padding-right");
    BIND_DP("sel_padding_bottom", s_padding_bottom, "padding-bottom");
    BIND_DP("sel_padding_left", s_padding_left, "padding-left");

#undef BIND_DP

    // Font-size uses dp units
    c.BindFunc(
        "sel_font_size",
        [](Rml::Variant& v) { v = s_font_size; },
        [](const Rml::Variant& v) {
            s_font_size = v.Get<float>();
            if (s_active_elem && !s_suppress_apply) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.1fdp", s_font_size);
                s_active_elem->SetProperty("font-size", Rml::String(buf));
            }
        });

    // Opacity is unitless (0–1)
    c.BindFunc(
        "sel_opacity",
        [](Rml::Variant& v) { v = s_opacity; },
        [](const Rml::Variant& v) {
            s_opacity = v.Get<float>();
            if (s_active_elem && !s_suppress_apply) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.2f", s_opacity);
                s_active_elem->SetProperty("opacity", Rml::String(buf));
            }
        });

    c.BindFunc(
        "sel_width",
        [](Rml::Variant& v) { v = s_width_str; },
        [](const Rml::Variant& v) {
            s_width_str = v.Get<Rml::String>();
            if (s_active_elem && !s_suppress_apply && !s_width_str.empty())
                s_active_elem->SetProperty("width", s_width_str);
        });
    c.BindFunc(
        "sel_height",
        [](Rml::Variant& v) { v = s_height_str; },
        [](const Rml::Variant& v) {
            s_height_str = v.Get<Rml::String>();
            if (s_active_elem && !s_suppress_apply && !s_height_str.empty())
                s_active_elem->SetProperty("height", s_height_str);
        });
    c.BindFunc(
        "sel_color",
        [](Rml::Variant& v) { v = s_color_hex; },
        [](const Rml::Variant& v) {
            s_color_hex = v.Get<Rml::String>();
            if (s_active_elem && !s_suppress_apply && !s_color_hex.empty())
                s_active_elem->SetProperty("color", s_color_hex);
        });
    c.BindFunc(
        "sel_visible",
        [](Rml::Variant& v) { v = s_elem_visible; },
        [](const Rml::Variant& v) {
            s_elem_visible = v.Get<bool>();
            if (s_active_elem && !s_suppress_apply)
                s_active_elem->SetProperty("visibility", s_elem_visible ? "visible" : "hidden");
        });
    c.BindFunc("sel_path", [](Rml::Variant& v) { v = s_elem_path; });

    // ── Tier 1 additions ──

#define BIND_STR(var_name, cpp_var, css_prop)                                                                          \
    c.BindFunc(                                                                                                        \
        var_name,                                                                                                      \
        [](Rml::Variant& v) { v = cpp_var; },                                                                          \
        [](const Rml::Variant& v) {                                                                                    \
            cpp_var = v.Get<Rml::String>();                                                                            \
            if (s_active_elem && !s_suppress_apply && !cpp_var.empty())                                                \
                s_active_elem->SetProperty(css_prop, cpp_var);                                                         \
        })

    BIND_STR("sel_position", s_position_str, "position");
    BIND_STR("sel_top", s_top_str, "top");
    BIND_STR("sel_right", s_right_str, "right");
    BIND_STR("sel_bottom", s_bottom_str, "bottom");
    BIND_STR("sel_left", s_left_str, "left");
    BIND_STR("sel_bg_color", s_bg_color, "background-color");
    BIND_STR("sel_border_color", s_border_color, "border-color");
    BIND_STR("sel_display", s_display_str, "display");

#undef BIND_STR

    // Border width — uniform, uses dp
    c.BindFunc(
        "sel_border_width",
        [](Rml::Variant& v) { v = s_border_width; },
        [](const Rml::Variant& v) {
            s_border_width = v.Get<float>();
            if (s_active_elem && !s_suppress_apply) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.1fdp", s_border_width);
                s_active_elem->SetProperty("border-width", Rml::String(buf));
            }
        });

    // Border radius — uniform, uses dp
    c.BindFunc(
        "sel_border_radius",
        [](Rml::Variant& v) { v = s_border_radius; },
        [](const Rml::Variant& v) {
            s_border_radius = v.Get<float>();
            if (s_active_elem && !s_suppress_apply) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.1fdp", s_border_radius);
                s_active_elem->SetProperty("border-radius", Rml::String(buf));
            }
        });

    // ── Tier 2 additions ──

#define BIND_STR2(var_name, cpp_var, css_prop)                                                                         \
    c.BindFunc(                                                                                                        \
        var_name,                                                                                                      \
        [](Rml::Variant& v) { v = cpp_var; },                                                                          \
        [](const Rml::Variant& v) {                                                                                    \
            cpp_var = v.Get<Rml::String>();                                                                            \
            if (s_active_elem && !s_suppress_apply && !cpp_var.empty())                                                \
                s_active_elem->SetProperty(css_prop, cpp_var);                                                         \
        })

    BIND_STR2("sel_text_align", s_text_align, "text-align");
    BIND_STR2("sel_flex_direction", s_flex_direction, "flex-direction");
    BIND_STR2("sel_justify_content", s_justify_content, "justify-content");
    BIND_STR2("sel_align_items", s_align_items, "align-items");
    BIND_STR2("sel_overflow", s_overflow, "overflow");

#undef BIND_STR2

    // Line height — uses dp
    c.BindFunc(
        "sel_line_height",
        [](Rml::Variant& v) { v = s_line_height; },
        [](const Rml::Variant& v) {
            s_line_height = v.Get<float>();
            if (s_active_elem && !s_suppress_apply) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.1fdp", s_line_height);
                s_active_elem->SetProperty("line-height", Rml::String(buf));
            }
        });

    // Letter spacing — uses dp
    c.BindFunc(
        "sel_letter_spacing",
        [](Rml::Variant& v) { v = s_letter_spacing; },
        [](const Rml::Variant& v) {
            s_letter_spacing = v.Get<float>();
            if (s_active_elem && !s_suppress_apply) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.1fdp", s_letter_spacing);
                s_active_elem->SetProperty("letter-spacing", Rml::String(buf));
            }
        });

    // Gap — uses dp
    c.BindFunc(
        "sel_gap",
        [](Rml::Variant& v) { v = s_gap; },
        [](const Rml::Variant& v) {
            s_gap = v.Get<float>();
            if (s_active_elem && !s_suppress_apply) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.1fdp", s_gap);
                s_active_elem->SetProperty("gap", Rml::String(buf));
            }
        });

    // ── Tier 3: read-only computed info ──
    c.BindFunc("info_tag", [](Rml::Variant& v) { v = s_info_tag; });
    c.BindFunc("info_id", [](Rml::Variant& v) { v = s_info_id; });
    c.BindFunc("info_classes", [](Rml::Variant& v) { v = s_info_classes; });
    c.BindFunc("info_box", [](Rml::Variant& v) { v = s_info_box; });
    c.BindFunc("info_children", [](Rml::Variant& v) { v = s_info_children; });
    c.BindFunc("info_source", [](Rml::Variant& v) { v = s_info_source; });

    // ── Event callbacks ──

    c.BindEventCallback("refresh_docs", [](Rml::DataModelHandle h, Rml::Event&, const Rml::VariantList&) {
        refresh_doc_list();
        h.DirtyVariable("doc_list");
        h.DirtyVariable("doc_count");
        SDL_Log("[DevOverlay] Refreshed: %d documents", (int)s_docs.size());
    });

    c.BindEventCallback("select_doc", [](Rml::DataModelHandle h, Rml::Event&, const Rml::VariantList& args) {
        if (args.empty()) {
            SDL_Log("[DevOverlay] select_doc: no args!");
            return;
        }
        int idx = args[0].Get<int>();
        SDL_Log("[DevOverlay] select_doc event: index=%d", idx);
        do_select_doc(idx);
        h.DirtyVariable("selected_doc");
    });

    c.BindEventCallback("select_elem", [](Rml::DataModelHandle h, Rml::Event&, const Rml::VariantList& args) {
        if (args.empty()) {
            SDL_Log("[DevOverlay] select_elem: no args!");
            return;
        }
        int idx = args[0].Get<int>();
        SDL_Log("[DevOverlay] select_elem event: index=%d", idx);
        do_select_elem(idx);
        h.DirtyVariable("selected_elem");
    });

    c.BindEventCallback("reset_elem", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
        if (!s_active_elem)
            return;
        const char* props[] = { "margin-top",
                                "margin-right",
                                "margin-bottom",
                                "margin-left",
                                "padding-top",
                                "padding-right",
                                "padding-bottom",
                                "padding-left",
                                "font-size",
                                "opacity",
                                "width",
                                "height",
                                "color",
                                "visibility",
                                "position",
                                "top",
                                "right",
                                "bottom",
                                "left",
                                "background-color",
                                "border-width",
                                "border-color",
                                "border-radius",
                                "display",
                                "text-align",
                                "line-height",
                                "letter-spacing",
                                "flex-direction",
                                "justify-content",
                                "align-items",
                                "gap",
                                "overflow" };
        for (auto p : props)
            s_active_elem->RemoveProperty(p);
        snapshot_element(s_active_elem);
        s_suppress_apply = true;
        dirty_all_props();
        SDL_Log("[DevOverlay] Reset: %s", s_elem_path.c_str());
    });

    c.BindEventCallback("copy_rcss", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
        if (!s_active_elem)
            return;
        std::ostringstream ss;
        ss << "/* Dev Overlay export for: " << s_elem_path.c_str() << " */\n";
        ss << make_label(s_active_elem).c_str() << " {\n";
        auto emit = [&](const char* prop) {
            const Rml::Property* p = s_active_elem->GetLocalProperty(prop);
            if (p)
                ss << "    " << prop << ": " << p->ToString().c_str() << ";\n";
        };
        emit("position");
        emit("display");
        emit("top");
        emit("right");
        emit("bottom");
        emit("left");
        emit("margin-top");
        emit("margin-right");
        emit("margin-bottom");
        emit("margin-left");
        emit("padding-top");
        emit("padding-right");
        emit("padding-bottom");
        emit("padding-left");
        emit("width");
        emit("height");
        emit("border-width");
        emit("border-color");
        emit("border-radius");
        emit("background-color");
        emit("font-size");
        emit("opacity");
        emit("color");
        emit("visibility");
        emit("text-align");
        emit("line-height");
        emit("letter-spacing");
        emit("flex-direction");
        emit("justify-content");
        emit("align-items");
        emit("gap");
        emit("overflow");
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
    if (!s_model)
        return;

    // Clear the reset guard — by this point, Context::Update() has already
    // processed the dirty flags from the reset, and setters were suppressed.
    s_suppress_apply = false;

    // Deferred element-list rebuild (scheduled by select_doc event callback)
    if (s_needs_rebuild) {
        do_rebuild_elem_list();
        dirty_all_props();
        s_needs_rebuild = false;
    }

    // On first open, refresh and auto-select first visible document.
    if (s_first_open && show_dev_overlay) {
        s_first_open = false;
        refresh_doc_list();
        s_model.DirtyVariable("doc_list");
        s_model.DirtyVariable("doc_count");

        // Auto-select first visible doc → rebuild immediately
        for (int i = 0; i < (int)s_docs.size(); i++) {
            if (s_docs[i].visible) {
                s_selected_doc = i;
                s_model.DirtyVariable("selected_doc");
                do_rebuild_elem_list();
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
