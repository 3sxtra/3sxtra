/**
 * @file workuser_select.h
 * @brief Extern declarations for gameplay globals - Character Selection and Cursors
 */
#ifndef WORKUSER_SELECT_H
#define WORKUSER_SELECT_H

#include "types.h"
#include "game_state.h"
#include "sf33rd/Source/Game/engine/cmd_data.h"
#include "sf33rd/Source/Game/select_timer.h"
#include <stdbool.h>

// MARK: - Serialized
extern u8 Order[148];
extern u8 Order_Timer[148];
extern u8 Order_Dir[148];
extern u8 My_char[2];
extern s8 Super_Arts[2];
extern s8 Cursor_X[2];
extern s8 Cursor_Y[2];
extern s8 Cursor_Y_Pos[2][4];
extern s8 Cursor_Timer[2];
extern s8 Complete_Face;
extern u8 Play_Type;
extern s16 Sel_PL_Complete[2];
extern s8 Select_Start[2];
extern s8 Face_MV_Request;
extern s8 Face_Move;
extern s8 Player_id;
extern s8 Last_Player_id;
extern s8 Player_Number;
extern s8 COM_id;
extern s8 EM_id;
extern s8 Select_Status[2];
extern u8 Country;
extern s8 Face_MV_Time;
extern s8 Player_Color[2];
extern u8 Stock_My_char[2];
extern s8 Stock_Player_Color[2];
extern s8 Last_Super_Arts[2];
extern s8 Last_My_char[2];
extern u8 Used_char[2];
extern u8 Weak_PL;
extern s8 ID_of_Face[3][8];
extern s8 Cursor_Move[2];
extern s8 Auto_Cursor[2];
extern s8 Auto_No[2];
extern s8 Auto_Index[2];
extern s8 Auto_Timer[2];
extern s8 Last_My_char2[2];
extern u8 End_PL;
extern s8 Stock_Com_Arts[2];
extern u8 EM_List[2][2];
extern s8 Sel_EM_Complete[2];
extern u8 EM_History[2][10];
extern u8 EM_Candidate[2][2][10];
extern u8 Q_Country;
extern u8 Final_Play_Type[2];
extern u8 kakushi_ix;
extern u8 kakushi_op;
extern u8 Face_No[2];
extern s8 Stop_Cursor[2];
extern s8 Select_Arts[2];
extern u8 Lamp_No;
extern u8 Lamp_Index;
extern u8 Lamp_Color;
extern s8 Menu_Cursor_X[2];
extern s8 Menu_Cursor_Y[2];
extern s8 Menu_Cursor_Move;
extern ModeType Mode_Type;
extern u8 Play_Mode;
extern s8 Cursor_Limit[2];
extern u8 Synchro_No;
extern s16 Sel_Arts_Complete[2];
extern s16 Move_Super_Arts[2];
extern s16 Battle_Country;
extern s16 Face_Status;
extern s16 Area_Number[2];
extern s16 Separate_Area[2][3];
extern s16 Last_Selected_ID;
extern s16 Shell_Separate_Area[2][3];
extern s16 Com_Color_Shot;
extern s16 Stock_Com_Color[2];
extern s16 Lamp_Timer;
extern s16 Flash_Synchro;
extern s16 Synchro_Level;

#endif // WORKUSER_SELECT_H
