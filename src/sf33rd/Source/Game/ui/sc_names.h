/**
 * @file sc_names.h
 * @brief Public API for player name/face rendering, grade display, and
 *        naming entry on the ranking screen.
 *
 * Part of the ui module. Split from sc_sub.c (task #21).
 */

#ifndef SC_NAMES_H
#define SC_NAMES_H

#include "types.h"

void player_name(void);
void player_face_init(void);
void scfont_sqput_face(u16 x, u16 y, u16 atr, u8 page, u8 cx1, u8 cy1, u8 cx2, u8 cy2, u16 priority);
void player_face(void);
void naming_set(u8 pl, s16 place, u16 atr, u16 chr);

#endif
