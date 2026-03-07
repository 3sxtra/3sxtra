#ifndef PORT_LEGACY_MATRIX_H
#define PORT_LEGACY_MATRIX_H

#include "structs.h"
#include "types.h"

void njUnitMatrix(MTX* mtx);
void njGetMatrix(MTX* m);
void njSetMatrix(MTX* md, MTX* ms);
void njScale(MTX* mtx, f32 x, f32 y, f32 z);
void njTranslate(MTX* mtx, f32 x, f32 y, f32 z);
void njTranslateZ(f32 z);
void njCalcPoint(MTX* mtx, Vec3* ps, Vec3* pd);
void njCalcPoints(MTX* mtx, Vec3* ps, Vec3* pd, s32 num);

#endif
