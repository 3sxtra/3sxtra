/**
 * @file sys_options.h
 * @brief Game option save/load/defaults/compare API.
 *
 * Split from sys_sub.h — option persistence, change detection,
 * and system-direction page lookup.
 */
#ifndef SYSTEM_OPTIONS_H
#define SYSTEM_OPTIONS_H

#include "structs.h"
#include "types.h"

extern const struct _SAVE_W Game_Default_Data;

void Game_Data_Init(void);
void Setup_IO_ConvDataDefault(s32 id);
void Save_Game_Data(void);
void Copy_Save_w(void);
void Copy_Check_w(void);
void Setup_Default_Game_Option(void);
s32 Check_Change_Contents(void);
void Copy_Key_Disp_Work(void);
s32 Check_Extra_Setting(void);
s16 Check_SysDir_Page(void);

#endif
