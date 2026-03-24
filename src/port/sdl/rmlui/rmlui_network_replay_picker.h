/**
 * @file rmlui_network_replay_picker.h
 * @brief RmlUi overlay for browsing and downloading online replays.
 *
 * Provides a full-screen picker that fetches replay metadata from the
 * lobby server, displays a paginated list, and downloads selected replays
 * for playback.
 */

#ifndef RMLUI_NETWORK_REPLAY_PICKER_H
#define RMLUI_NETWORK_REPLAY_PICKER_H

#ifdef __cplusplus
extern "C" {
#endif

/// Show the network replay picker overlay.
void rmlui_network_replay_picker_show(void);

/// Hide the network replay picker overlay.
void rmlui_network_replay_picker_hide(void);

/// Per-frame update — drives async fetch, data model dirty tracking.
void rmlui_network_replay_picker_update(void);

/// Poll for user action. Returns:
///   1  = still browsing (no action yet)
///   0  = user selected a replay (download complete, ready to play)
///  -1  = user cancelled
int rmlui_network_replay_picker_poll(void);

/// Navigate to previous page.
void rmlui_network_replay_picker_prev_page(void);

/// Navigate to next page.
void rmlui_network_replay_picker_next_page(void);

#ifdef __cplusplus
}
#endif

#endif /* RMLUI_NETWORK_REPLAY_PICKER_H */
