/**
 * @file se_data.h
 * @brief Externs for the SE function dispatch table and bonus voice data.
 *
 * Part of the sound module.
 * Originally from the PS2 game module.
 */

#ifndef SE_DATA_H
#define SE_DATA_H

extern void (*sound_effect_request[])();
extern const u16 Bonus_Voice_Data[768];

#endif
