/**
 * @file rmlui_leaderboard.h
 * @brief RmlUi Leaderboard screen — shows top players from the lobby server.
 */
#ifndef RMLUI_LEADERBOARD_H
#define RMLUI_LEADERBOARD_H

#ifdef __cplusplus
extern "C" {
#endif

void rmlui_leaderboard_init(void);
void rmlui_leaderboard_update(void);
void rmlui_leaderboard_show(void);
void rmlui_leaderboard_hide(void);
void rmlui_leaderboard_shutdown(void);

/// Fetch a specific page (0-indexed). Triggers async fetch.
void rmlui_leaderboard_fetch_page(int page);

#ifdef __cplusplus
}
#endif

#endif
