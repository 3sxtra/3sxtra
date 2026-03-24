#ifndef ARCADE_CHAR_DATA_H
#define ARCADE_CHAR_DATA_H

#include "constants.h"
#include "structs.h"

#include <SDL3/SDL.h>

typedef enum Character {
    CHAR_GILL = 0,
    CHAR_ALEX = 1,
    CHAR_RYU = 2,
    CHAR_YUN = 3,
    CHAR_DUDLEY = 4,
    CHAR_NECRO = 5,
    CHAR_HUGO = 6,
    CHAR_IBUKI = 7,
    CHAR_ELENA = 8,
    CHAR_ORO = 9,
    CHAR_YANG = 10,
    CHAR_KEN = 11,
    CHAR_SEAN = 12,
    CHAR_URIEN = 13,
    CHAR_AKUMA = 14,
    CHAR_CHUN_LI = 15,
    CHAR_MAKOTO = 16,
    CHAR_Q = 17,
    CHAR_TWELVE = 18,
    CHAR_REMY = 19,
    NUM_CHARS = 20
} Character;

void ArcadeCharData_Init();
const CharInitData* ArcadeCharData_Get(Character character);

#endif
