#ifndef SEL_PL_H
#define SEL_PL_H

#include "types.h"

extern s16 Play_Type_1st;
extern u8 SEL_PL_X;

s16 Select_Player(void);
void Sel_PL_Control_Frame(void);

enum SelPlContState { SEL_PL_CONT_1ST = 0, SEL_PL_CONT_2ND, SEL_PL_CONT_3RD, SEL_PL_CONT_4TH };
enum FaceState { FACE_1ST = 0, FACE_2ND, FACE_3RD, FACE_4TH };
enum ObjState { OBJ_1ST = 0, OBJ_2ND, OBJ_3RD };
enum PlSelState { PL_SEL_1ST = 0, PL_SEL_2ND, PL_SEL_3RD, PL_SEL_4TH, PL_SEL_5TH };
enum SelPlState { SEL_PL_1ST = 0, SEL_PL_2ND, SEL_PL_3RD, SEL_PL_4TH, SEL_PL_5TH, SEL_PL_6TH };
enum HandicapState { HANDICAP_1 = 0, HANDICAP_2, HANDICAP_3, HANDICAP_4 };

#endif
