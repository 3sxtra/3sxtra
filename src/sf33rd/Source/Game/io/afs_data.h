/**
 * @file afs_data.h
 * @brief Load-request table types and data declarations.
 *
 * Separated from afs_loader.c for readability — the data is pure ROM
 * (const arrays of hex values) with no logic.
 *
 * Part of the io module.
 */

#ifndef AFS_DATA_H
#define AFS_DATA_H

#include "types.h"

typedef struct {
    u8 type;
    u8 ix;
    u8 frre;
    u8 kokey;
} LDREQ_TBL;

#define LDREQ_TBL_SIZE 294
#define LDREQ_IX_SIZE 43

extern const LDREQ_TBL ldreq_tbl[LDREQ_TBL_SIZE];
extern const s16 ldreq_ix[LDREQ_IX_SIZE][2];

#endif /* AFS_DATA_H */
