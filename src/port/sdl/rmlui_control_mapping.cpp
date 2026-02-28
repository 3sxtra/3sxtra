/**
 * @file rmlui_control_mapping.cpp
 * @brief RmlUi control mapping overlay — data model + update logic.
 *
 * Reads control_mapping.cpp state via accessor functions and exposes
 * it to the RmlUi "control_mapping" data model.  All device management,
 * input capture, and save/load logic stays in control_mapping.cpp.
 */
#include "port/sdl/rmlui_control_mapping.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

#include <string>
#include <vector>

// ── C externs (accessor functions from control_mapping.cpp) ────

extern "C" {
const char* ControlMapping_GetDeviceName(int player_num);
bool ControlMapping_HasDevice(int player_num);
void ControlMapping_ClaimDevice(int player_num, int device_index);
void ControlMapping_UnclaimDevice(int player_num);
void ControlMapping_StartMapping(int player_num);
void ControlMapping_ResetMappings(int player_num);
int  ControlMapping_GetMappingState(int player_num);
int  ControlMapping_GetMappingActionIndex(int player_num);
int  ControlMapping_GetAvailableDeviceCount();
const char* ControlMapping_GetAvailableDeviceName(int index);
int  ControlMapping_GetAvailableDeviceId(int index);
int  ControlMapping_GetPlayerMappingCount(int player_num);
const char* ControlMapping_GetPlayerMappingAction(int player_num, int index);
const char* ControlMapping_GetPlayerMappingInput(int player_num, int index);

extern const char* game_actions[];
int get_game_actions_count();
}

// ── Structs for data binding ───────────────────────────────────

struct DeviceEntry {
    Rml::String name;
    int         device_id;
};

struct MappingEntry {
    Rml::String action;
    Rml::String input;
};

// ── Module state ───────────────────────────────────────────────

static Rml::DataModelHandle s_model_handle;

static std::vector<DeviceEntry> s_available_devices;
static std::vector<MappingEntry> s_p1_mappings;
static std::vector<MappingEntry> s_p2_mappings;

// Snapshot for dirty checking
static struct {
    bool  p1_has_device, p2_has_device;
    int   p1_state, p2_state;
    int   p1_action_idx, p2_action_idx;
    int   avail_count;
    int   p1_map_count, p2_map_count;
} s_prev;

// ── Helpers ────────────────────────────────────────────────────

static void rebuild_available_devices() {
    s_available_devices.clear();
    int count = ControlMapping_GetAvailableDeviceCount();
    for (int i = 0; i < count; i++) {
        const char* name = ControlMapping_GetAvailableDeviceName(i);
        int id = ControlMapping_GetAvailableDeviceId(i);
        if (name) s_available_devices.push_back({name, id});
    }
}

static void rebuild_mappings(int player_num, std::vector<MappingEntry>& vec) {
    vec.clear();
    int count = ControlMapping_GetPlayerMappingCount(player_num);
    for (int i = 0; i < count; i++) {
        const char* action = ControlMapping_GetPlayerMappingAction(player_num, i);
        const char* input  = ControlMapping_GetPlayerMappingInput(player_num, i);
        vec.push_back({action ? action : "", input ? input : ""});
    }
}

// State enum: 0=Idle, 1=Waiting, 2=WaitingForKeyRelease, 3=Done
static Rml::String state_to_string(int state, int action_idx) {
    switch (state) {
    case 1: {
        int total = get_game_actions_count();
        if (action_idx < total) {
            return Rml::String("Press a button for: ") + game_actions[action_idx];
        }
        return "Waiting for input...";
    }
    case 2: return "Release all inputs...";
    case 3: return "Mapping Complete!";
    default: return "";
    }
}

// ── Init ───────────────────────────────────────────────────────

extern "C" void rmlui_control_mapping_init() {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx) {
        SDL_Log("[RmlUi ControlMapping] No context available");
        return;
    }

    Rml::DataModelConstructor c = ctx->CreateDataModel("control_mapping");
    if (!c) {
        SDL_Log("[RmlUi ControlMapping] Failed to create data model");
        return;
    }

    // Register structs
    if (auto sh = c.RegisterStruct<DeviceEntry>()) {
        sh.RegisterMember("name", &DeviceEntry::name);
        sh.RegisterMember("device_id", &DeviceEntry::device_id);
    }
    c.RegisterArray<std::vector<DeviceEntry>>();

    if (auto sh = c.RegisterStruct<MappingEntry>()) {
        sh.RegisterMember("action", &MappingEntry::action);
        sh.RegisterMember("input", &MappingEntry::input);
    }
    c.RegisterArray<std::vector<MappingEntry>>();

    // Bind arrays
    c.Bind("available_devices", &s_available_devices);
    c.Bind("p1_mappings", &s_p1_mappings);
    c.Bind("p2_mappings", &s_p2_mappings);

    // Player device info
    c.BindFunc("p1_has_device", [](Rml::Variant& v) { v = ControlMapping_HasDevice(1); });
    c.BindFunc("p2_has_device", [](Rml::Variant& v) { v = ControlMapping_HasDevice(2); });

    c.BindFunc("p1_device_name", [](Rml::Variant& v) {
        const char* n = ControlMapping_GetDeviceName(1);
        v = Rml::String(n ? n : "");
    });
    c.BindFunc("p2_device_name", [](Rml::Variant& v) {
        const char* n = ControlMapping_GetDeviceName(2);
        v = Rml::String(n ? n : "");
    });

    // Mapping state prompts
    c.BindFunc("p1_prompt", [](Rml::Variant& v) {
        v = state_to_string(ControlMapping_GetMappingState(1),
                            ControlMapping_GetMappingActionIndex(1));
    });
    c.BindFunc("p2_prompt", [](Rml::Variant& v) {
        v = state_to_string(ControlMapping_GetMappingState(2),
                            ControlMapping_GetMappingActionIndex(2));
    });

    c.BindFunc("p1_is_idle", [](Rml::Variant& v) { v = (ControlMapping_GetMappingState(1) == 0); });
    c.BindFunc("p2_is_idle", [](Rml::Variant& v) { v = (ControlMapping_GetMappingState(2) == 0); });

    // Event callbacks
    c.BindEventCallback("claim_p1",
        [](Rml::DataModelHandle h, Rml::Event&, const Rml::VariantList& args) {
            if (args.empty()) return;
            ControlMapping_ClaimDevice(1, args[0].Get<int>());
            h.DirtyVariable("p1_has_device");
            h.DirtyVariable("p1_device_name");
            h.DirtyVariable("available_devices");
        }
    );

    c.BindEventCallback("claim_p2",
        [](Rml::DataModelHandle h, Rml::Event&, const Rml::VariantList& args) {
            if (args.empty()) return;
            ControlMapping_ClaimDevice(2, args[0].Get<int>());
            h.DirtyVariable("p2_has_device");
            h.DirtyVariable("p2_device_name");
            h.DirtyVariable("available_devices");
        }
    );

    c.BindEventCallback("unclaim_p1",
        [](Rml::DataModelHandle h, Rml::Event&, const Rml::VariantList&) {
            ControlMapping_UnclaimDevice(1);
            h.DirtyVariable("p1_has_device");
            h.DirtyVariable("p1_device_name");
            h.DirtyVariable("available_devices");
        }
    );

    c.BindEventCallback("unclaim_p2",
        [](Rml::DataModelHandle h, Rml::Event&, const Rml::VariantList&) {
            ControlMapping_UnclaimDevice(2);
            h.DirtyVariable("p2_has_device");
            h.DirtyVariable("p2_device_name");
            h.DirtyVariable("available_devices");
        }
    );

    c.BindEventCallback("map_p1",
        [](Rml::DataModelHandle h, Rml::Event&, const Rml::VariantList&) {
            ControlMapping_StartMapping(1);
            h.DirtyVariable("p1_prompt");
            h.DirtyVariable("p1_is_idle");
        }
    );

    c.BindEventCallback("map_p2",
        [](Rml::DataModelHandle h, Rml::Event&, const Rml::VariantList&) {
            ControlMapping_StartMapping(2);
            h.DirtyVariable("p2_prompt");
            h.DirtyVariable("p2_is_idle");
        }
    );

    c.BindEventCallback("reset_p1",
        [](Rml::DataModelHandle h, Rml::Event&, const Rml::VariantList&) {
            ControlMapping_ResetMappings(1);
            h.DirtyVariable("p1_mappings");
            h.DirtyVariable("p1_prompt");
            h.DirtyVariable("p1_is_idle");
        }
    );

    c.BindEventCallback("reset_p2",
        [](Rml::DataModelHandle h, Rml::Event&, const Rml::VariantList&) {
            ControlMapping_ResetMappings(2);
            h.DirtyVariable("p2_mappings");
            h.DirtyVariable("p2_prompt");
            h.DirtyVariable("p2_is_idle");
        }
    );

    s_model_handle = c.GetModelHandle();

    // Initial build
    rebuild_available_devices();
    rebuild_mappings(1, s_p1_mappings);
    rebuild_mappings(2, s_p2_mappings);

    SDL_Log("[RmlUi ControlMapping] Data model registered");
}

// ── Per-frame update ───────────────────────────────────────────

extern "C" void rmlui_control_mapping_update() {
    if (!s_model_handle) return;

    // Ensure document is shown (lazy-loads on first call)
    if (!rmlui_wrapper_is_document_visible("control_mapping")) {
        rmlui_wrapper_show_document("control_mapping");
    }

    bool p1_has = ControlMapping_HasDevice(1);
    bool p2_has = ControlMapping_HasDevice(2);
    int p1_state = ControlMapping_GetMappingState(1);
    int p2_state = ControlMapping_GetMappingState(2);
    int p1_idx = ControlMapping_GetMappingActionIndex(1);
    int p2_idx = ControlMapping_GetMappingActionIndex(2);
    int avail = ControlMapping_GetAvailableDeviceCount();
    int p1_mc = ControlMapping_GetPlayerMappingCount(1);
    int p2_mc = ControlMapping_GetPlayerMappingCount(2);

    if (p1_has != s_prev.p1_has_device) {
        s_prev.p1_has_device = p1_has;
        s_model_handle.DirtyVariable("p1_has_device");
        s_model_handle.DirtyVariable("p1_device_name");
    }

    if (p2_has != s_prev.p2_has_device) {
        s_prev.p2_has_device = p2_has;
        s_model_handle.DirtyVariable("p2_has_device");
        s_model_handle.DirtyVariable("p2_device_name");
    }

    if (p1_state != s_prev.p1_state || p1_idx != s_prev.p1_action_idx) {
        s_prev.p1_state = p1_state;
        s_prev.p1_action_idx = p1_idx;
        s_model_handle.DirtyVariable("p1_prompt");
        s_model_handle.DirtyVariable("p1_is_idle");
    }

    if (p2_state != s_prev.p2_state || p2_idx != s_prev.p2_action_idx) {
        s_prev.p2_state = p2_state;
        s_prev.p2_action_idx = p2_idx;
        s_model_handle.DirtyVariable("p2_prompt");
        s_model_handle.DirtyVariable("p2_is_idle");
    }

    if (avail != s_prev.avail_count) {
        s_prev.avail_count = avail;
        rebuild_available_devices();
        s_model_handle.DirtyVariable("available_devices");
    }

    if (p1_mc != s_prev.p1_map_count) {
        s_prev.p1_map_count = p1_mc;
        rebuild_mappings(1, s_p1_mappings);
        s_model_handle.DirtyVariable("p1_mappings");
    }

    if (p2_mc != s_prev.p2_map_count) {
        s_prev.p2_map_count = p2_mc;
        rebuild_mappings(2, s_p2_mappings);
        s_model_handle.DirtyVariable("p2_mappings");
    }
}

// ── Shutdown ───────────────────────────────────────────────────

extern "C" void rmlui_control_mapping_shutdown() {
    s_model_handle = Rml::DataModelHandle();
    s_available_devices.clear();
    s_p1_mappings.clear();
    s_p2_mappings.clear();
}
