#pragma once
/**
 * @file rmlui_ranked_matchmaking.h
 * @brief RmlUi Ranked Matchmaking
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

void rmlui_ranked_matchmaking_init(void);
void rmlui_ranked_matchmaking_update(void);
void rmlui_ranked_matchmaking_show(void);
void rmlui_ranked_matchmaking_hide(void);

bool rmlui_ranked_matchmaking_wants_leave(void);
void rmlui_ranked_matchmaking_consume_leave(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_ranked_matchmaking_init(void) {}
static inline void rmlui_ranked_matchmaking_update(void) {}
static inline void rmlui_ranked_matchmaking_show(void) {}
static inline void rmlui_ranked_matchmaking_hide(void) {}
static inline bool rmlui_ranked_matchmaking_wants_leave(void) { return false; }
static inline void rmlui_ranked_matchmaking_consume_leave(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
