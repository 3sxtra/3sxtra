#include <stdio.h>
#include <stddef.h>
#include "structs.h"

int main() {
    printf("0x%X cmwk\n", (unsigned int)offsetof(WORK, cmwk));
    printf("0x%X char_table\n", (unsigned int)offsetof(WORK, char_table));
    printf("0x%X se_random_table\n", (unsigned int)offsetof(WORK, se_random_table));
    printf("0x%X step_xy_table\n", (unsigned int)offsetof(WORK, step_xy_table));
    printf("0x%X move_xy_table\n", (unsigned int)offsetof(WORK, move_xy_table));
    printf("0x%X overlap_char_tbl\n", (unsigned int)offsetof(WORK, overlap_char_tbl));
    printf("0x%X olc_ix_table\n", (unsigned int)offsetof(WORK, olc_ix_table));
    printf("0x%X cg_olc\n", (unsigned int)offsetof(WORK, cg_olc));
    printf("0x%X rival_catch_tbl\n", (unsigned int)offsetof(WORK, rival_catch_tbl));
    printf("0x%X curr_rca\n", (unsigned int)offsetof(WORK, curr_rca));
    printf("0x%X set_char_ad\n", (unsigned int)offsetof(WORK, set_char_ad));
    printf("0x%X cg_ix\n", (unsigned int)offsetof(WORK, cg_ix));
    printf("0x%X now_koc\n", (unsigned int)offsetof(WORK, now_koc));

    // Wait, let's also dump from CharState, just in case! 
    // It's part of the main WORK because CharState is just `s16 type; u8 a,b,c,d,e,f;`
    // Wait, the variables inside WORK before `cmwk`
    printf("0x%X cmr0\n", (unsigned int)offsetof(WORK, cmr0));
    printf("0x%X cmmd\n", (unsigned int)offsetof(WORK, cmmd));
    printf("0x%X cmhs\n", (unsigned int)offsetof(WORK, cmhs));

    return 0;
}
