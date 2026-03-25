#ifndef RMLUI_PLAYER_PROFILE_H
#define RMLUI_PLAYER_PROFILE_H

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the data model for the player profile screen
void rmlui_player_profile_init(void);

// Fetch stats and matches for the current player asynchronously
void rmlui_player_profile_fetch(void);

// Per-frame UI update
void rmlui_player_profile_update(void);

// Show the RmlUi player profile screen
void rmlui_player_profile_show(void);

// Hide the RmlUi player profile screen
void rmlui_player_profile_hide(void);

// Poll player inputs / actions on the profile pane.
// Return 0 if replay ready to play, -1 if cancel requested, 1 to keep polling
int rmlui_player_profile_poll(unsigned short trigger);

// Force cleanup
void rmlui_player_profile_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
