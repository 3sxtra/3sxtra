import sys

with open("src/netplay/netplay.c", "r", encoding="utf-8") as f:
    content = f.read()

# 1. replace dynamic_frame_skip_max initialization to 60 globally
content = content.replace("static int dynamic_frame_skip_max = 3;", "static int dynamic_frame_skip_max = 60;")
content = content.replace("dynamic_frame_skip_max = 3;", "dynamic_frame_skip_max = 60;")

# 2. compute_tuning_from_ping
old_tuning = """    if (effective_rtt < 90.0f) {
        *out_delay = 0;
        *out_skip_max = 2;
    } else if (effective_rtt < 150.0f) {
        *out_delay = 1;
        *out_skip_max = 3;
    } else if (effective_rtt < 200.0f) {
        *out_delay = 3;
        *out_skip_max = 4;
    } else if (effective_rtt < 250.0f) {
        *out_delay = 4;
        *out_skip_max = 5;
    } else {
        *out_delay = 5;
        *out_skip_max = 5;
    }"""
new_tuning = """    if (effective_rtt < 90.0f) {
        *out_delay = 0;
        *out_skip_max = 60;
    } else if (effective_rtt < 150.0f) {
        *out_delay = 1;
        *out_skip_max = 60;
    } else if (effective_rtt < 200.0f) {
        *out_delay = 3;
        *out_skip_max = 60;
    } else if (effective_rtt < 250.0f) {
        *out_delay = 4;
        *out_skip_max = 60;
    } else {
        *out_delay = 5;
        *out_skip_max = 60;
    }"""
content = content.replace(old_tuning, new_tuning)

# 3. Game config
old_config = """    config.input_prediction_window = 8; // Absolute max 8 per recommendations

    config.desync_detection = true;"""
new_config = """    config.input_prediction_window = 12; // Reverted

    config.desync_detection = true;"""
content = content.replace(old_config, new_config)

# 4. STUN socket tuning
old_stun = """    if (stun_socket != NULL) {
        // Internet play: reuse the hole-punched STUN socket
        gekko_net_adapter_set(session, SDLNetAdapter_Create(stun_socket));"""
new_stun = """    if (stun_socket != NULL) {
        // Internet play: reuse the hole-punched STUN socket
        NetTuning_SetRecvBuf(stun_socket, 256 * 1024);
        gekko_net_adapter_set(session, SDLNetAdapter_Create(stun_socket));"""
content = content.replace(old_stun, new_stun)

# 5. Spectator config
old_spec = """    config.spectator_delay = 10; // 10 frames (~166ms at 60fps) max per recommendations
    config.input_prediction_window = 8;"""
new_spec = """    config.spectator_delay = 15; // Reverted
    config.input_prediction_window = 12;"""
content = content.replace(old_spec, new_spec)

with open("src/netplay/netplay.c", "w", encoding="utf-8", newline="") as f:
    f.write(content)

print("Done.")
