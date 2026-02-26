#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
#define CFG_KEY_FULLSCREEN "fullscreen"
#define CFG_KEY_WINDOW_WIDTH "window-width"
#define CFG_KEY_WINDOW_HEIGHT "window-height"
#define CFG_KEY_WINDOW_X "window-x"
#define CFG_KEY_WINDOW_Y "window-y"
#define CFG_KEY_SCALEMODE "scale-mode"
#define CFG_KEY_DRAW_RECT_BORDERS "draw-rect-borders"
#define CFG_KEY_DUMP_TEXTURES "dump-textures"
#define CFG_KEY_SHADER_MODE_LIBRETRO "shader-mode-libretro"
#define CFG_KEY_BEZEL_ENABLED "bezel-enabled"
#define CFG_KEY_SHADER_PATH "shader-path"
#define CFG_KEY_BROADCAST_ENABLED "broadcast-enabled"
#define CFG_KEY_BROADCAST_SOURCE "broadcast-source"
#define CFG_KEY_BROADCAST_SHOW_UI "broadcast-show-ui"
#define CFG_KEY_TRAINING_HITBOXES "training-hitboxes"
#define CFG_KEY_TRAINING_PUSHBOXES "training-pushboxes"
#define CFG_KEY_TRAINING_HURTBOXES "training-hurtboxes"
#define CFG_KEY_TRAINING_ATTACKBOXES "training-attackboxes"
#define CFG_KEY_TRAINING_THROWBOXES "training-throwboxes"
#define CFG_KEY_TRAINING_ADVANTAGE "training-advantage"
#define CFG_KEY_TRAINING_STUN "training-stun"
#define CFG_KEY_TRAINING_INPUTS "training-inputs"
#define CFG_KEY_TRAINING_FRAME_METER "training-frame-meter"
#define CFG_KEY_NETPLAY_AUTO_CONNECT "netplay-auto-connect"
#define CFG_KEY_LOBBY_SERVER_URL "lobby-server-url"
#define CFG_KEY_LOBBY_SERVER_KEY "lobby-server-key"
#define CFG_KEY_LOBBY_CLIENT_ID "lobby-client-id"
#define CFG_KEY_LOBBY_DISPLAY_NAME "lobby-display-name"
#define CFG_KEY_LOBBY_AUTO_CONNECT "lobby-auto-connect"
#define CFG_KEY_LOBBY_AUTO_SEARCH "lobby-auto-search"
#define CFG_KEY_LOBBY_REGION "lobby-region"
#define CFG_KEY_VSYNC "vsync"
#define CFG_KEY_DEBUG_HUD "debug-hud"

/// Initialize config system
void Config_Init();

/// Destroy resources used by config system
void Config_Destroy();

/// Save configuration to disk
void Config_Save();

/// Check if a key exists in the configuration
bool Config_HasKey(const char* key);

/// Get the value associated with the given key as a `bool`
/// @return The value associated with `key` if `key` is among entries and the value's type is `bool`, `false` otherwise
bool Config_GetBool(const char* key);

/// Set the value associated with the given key as a `bool`
void Config_SetBool(const char* key, bool value);

/// Get the value associated with the given key as an `int`
/// @return The value associated with `key` if `key` is among entries and the value's type is `int`, `0` otherwise
int Config_GetInt(const char* key);

/// Set the value associated with the given key as an `int`
void Config_SetInt(const char* key, int value);

/// Get the value associated with the given key as a `string`
/// @return The value associated with `key` if `key` is among entries and the value's type is `string`, `NULL` otherwise
const char* Config_GetString(const char* key);

/// Set the value associated with the given key as a `string`
void Config_SetString(const char* key, const char* value);

#ifdef __cplusplus
}
#endif

#endif
