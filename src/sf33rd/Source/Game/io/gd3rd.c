/**
 * @file gd3rd.c
 * @brief AFS file reading and load-request queue management.
 *
 * Handles file open/close/read operations against the AFS archive,
 * manages a queue of load requests for textures, palettes, and sounds,
 * and provides the load-request dispatch table.
 *
 * Part of the io module.
 */

#include "sf33rd/Source/Game/io/gd3rd.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/AcrSDK/MiddleWare/PS2/CapSndEng/emlTSB.h"
#include "sf33rd/AcrSDK/ps2/flps2debug.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/rendering/color3rd.h"
#include "sf33rd/Source/Game/rendering/texgroup.h"
#include "sf33rd/Source/Game/system/ramcnt.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "structs.h"
#include "sf33rd/Source/Game/io/gd_data.h"

typedef void (*LDREQ_Process_Func)(REQ*);

#define LDREQ_PROCESS_COUNT 6
#define LDREQ_QUEUE_SIZE 16
#define LDREQ_RETRY_COUNT 0x40
#define PLAYER_COUNT 2
#define CHAR_TWELVE 0x12
#define METAMOR_BASE_INDEX 0xD4
#define METAMOR_MIRROR_INDEX 0xE6

const u8 lpr_wrdata[3] = { 0x03, 0xC0, 0x3C };
const u8 lpc_seldat[PLAYER_COUNT] = { 10, 11 };
const u8 lpt_seldat[4] = { 3, 4, 5, 0 };

s16 plt_req[PLAYER_COUNT];
u8 ldreq_break;
REQ q_ldreq[LDREQ_QUEUE_SIZE];
u8 ldreq_result[LDREQ_TBL_SIZE];

// forward decls
static s32 Push_LDREQ_Queue(REQ* ldreq);
static void Push_LDREQ_Queue_Metamor();
static void q_ldreq_error(REQ* curr);
static void disp_ldreq_status();
static void Push_LDREQ_Queue_Union(s16 ix);
static s32 Check_LDREQ_Queue_Union(s16 ix);

const LDREQ_Process_Func ldreq_process[LDREQ_PROCESS_COUNT];
s8* ldreq_process_name[];

/** @brief Open an AFS file by request number. */

/** @brief Close an AFS file (no-op on modern platform). */

/** @brief Return the file size for the given AFS file number. */

/** @brief Round a byte size up to the nearest sector boundary. */

/** @brief Cancel a pending file request (stub). */

/** @brief Check whether a file command is still executing. */

/** @brief Issue an asynchronous file-read request. */

/** @brief Check whether an asynchronous file read has completed. */

/** @brief Synchronous file read — request and wait for completion. */

/** @brief Dummy vsync wait (no-op on modern platform). */

/** @brief Load a file by number, allocating a key from any pool. */

/** @brief Load a file by number, returning an allocated key. */

/** @brief Load a file by number using a specific pre-allocated key. */

/** @brief First-time init of the load-request queue. */
void Init_Load_Request_Queue_1st() {
    s16 i;

    for (i = 0; i < LDREQ_QUEUE_SIZE; i++) {
        q_ldreq[i].be = 0;
        q_ldreq[i].type = 0;
    }

    ldreq_break = 0;
}

/** @brief Signal the load-request queue to break (cancel pending loads). */
void Request_LDREQ_Break() {
    ldreq_break = 1;
}

/** @brief Check whether a load-request break has been acknowledged. */
u8 Check_LDREQ_Break() {
    if (ldreq_break) {
        return 1;
    }

    return fsCheckCommandExecuting();
}

/** @brief Enqueue load requests for a player character's assets. */
void Push_LDREQ_Queue_Player(s16 id, s16 ix) {
    REQ ldreq;
    s16 i;
    s16 kara;
    s16 made;

    if (ix < 0 || ix >= LDREQ_IX_SIZE) {
        return;
    }
    if (id < 0 || id >= PLAYER_COUNT) {
        return;
    }

    kara = ldreq_ix[ix][0];
    made = kara + ldreq_ix[ix][1];
    plt_req[id] = ix;

    for (i = kara; i < made; i++) {
        if (i < 0 || i >= LDREQ_TBL_SIZE) {
            break;
        }
        ldreq.type = ldreq_tbl[i].type;
        ldreq.id = id;
        ldreq.ix = ldreq_tbl[i].ix;
        ldreq.frre = ldreq_tbl[i].frre;
        ldreq.key = 0;
        ldreq.group = 0;
        ldreq.result = &ldreq_result[i];

        if (ldreq.type == 2) {
            ldreq.kokey = lpc_seldat[id];
        } else {
            ldreq.kokey = lpt_seldat[id];
        }

        Push_LDREQ_Queue(&ldreq);
    }
}

/** @brief Enqueue load requests for a background stage's assets. */
void Push_LDREQ_Queue_BG(s16 ix) {
    Push_LDREQ_Queue_Union(ix + 20);
    Push_LDREQ_Queue_Metamor();
}

/** @brief Enqueue load requests for union (shared/common) assets. */
static void Push_LDREQ_Queue_Union(s16 ix) {
    REQ ldreq;
    s16 i;
    s16 kara;
    s16 made;

    if (ix < 0 || ix >= LDREQ_IX_SIZE) {
        return;
    }

    kara = ldreq_ix[ix][0];
    made = kara + ldreq_ix[ix][1];

    for (i = kara; i < made; i++) {
        if (i < 0 || i >= LDREQ_TBL_SIZE) {
            break;
        }
        ldreq.type = ldreq_tbl[i].type;
        ldreq.id = 2;
        ldreq.ix = ldreq_tbl[i].ix;
        ldreq.frre = ldreq_tbl[i].frre;
        ldreq.kokey = ldreq_tbl[i].kokey;
        ldreq.key = 0;
        ldreq.group = 0;
        ldreq.result = &ldreq_result[i];
        Push_LDREQ_Queue(&ldreq);
    }
}

/** @brief Enqueue load requests for metamorphosis character data. */
static void Push_LDREQ_Queue_Metamor() {
    switch ((g_state.My_char[0] == CHAR_TWELVE) + (g_state.My_char[1] == CHAR_TWELVE) * 2) {
    case 1:
        Push_LDREQ_Queue_Direct(g_state.My_char[1] + METAMOR_BASE_INDEX, 0);
        break;

    case 2:
        Push_LDREQ_Queue_Direct(g_state.My_char[0] + METAMOR_BASE_INDEX, 1);
        break;

    case 3:
        Push_LDREQ_Queue_Direct(METAMOR_MIRROR_INDEX, 2);
        break;
    }
}

/** @brief Enqueue a direct load request by index and g_state.ID. */
void Push_LDREQ_Queue_Direct(s16 ix, s16 id) {
    REQ ldreq;
    if (ix < 0 || ix >= LDREQ_TBL_SIZE) {
        return;
    }
    ldreq.type = ldreq_tbl[ix].type;
    ldreq.id = id;
    ldreq.ix = ldreq_tbl[ix].ix;
    ldreq.frre = ldreq_tbl[ix].frre;
    ldreq.kokey = ldreq_tbl[ix].kokey;
    ldreq.key = 0;
    ldreq.group = 0;
    ldreq.result = &ldreq_result[ix];
    Push_LDREQ_Queue(&ldreq);
}

/** @brief Push a single load request onto the queue. */
static s32 Push_LDREQ_Queue(REQ* ldreq) {
    s16 i;
    u8 masknum;

    for (i = 0; i < LDREQ_QUEUE_SIZE; i++) {
        if (q_ldreq[i].be == 0) {
            break;
        }
    }

    if (i != LDREQ_QUEUE_SIZE) {
        q_ldreq[i] = ldreq[0];
        q_ldreq[i].be = 2;
        q_ldreq[i].rno = 0;
        q_ldreq[i].retry = LDREQ_RETRY_COUNT;

        switch (ldreq->id) {
        case 0:
            masknum = 3;
            break;

        case 1:
            masknum = 0xC0;
            break;

        default:
            masknum = 0x3C;
            break;
        }

        *q_ldreq[i].result &= ~masknum;
        return 1;
    }

    flLogOut("ファイル読み込み要求バッファがオーバーしました。\n");
    return 0;
}

/** @brief Process pending load requests in FIFO order. */
void Check_LDREQ_Queue() {
    s16 i;

    disp_ldreq_status();

    if (!ldreq_break) {
        if (q_ldreq->be != 0) {
            if (q_ldreq->type < LDREQ_PROCESS_COUNT) {
                ldreq_process[q_ldreq->type](q_ldreq);
            } else {
                q_ldreq_error(q_ldreq);
            }

            if (q_ldreq->be == 0) {
                for (i = 0; i < LDREQ_QUEUE_SIZE - 1; i++) {
                    q_ldreq[i] = q_ldreq[i + 1];
                }

                q_ldreq[i].be = 0;
                q_ldreq[i].type = 0;
            }

            return;
        }
    } else {
        if (q_ldreq->be == 1) {
            fsCansel(q_ldreq);
        }

        Init_Load_Request_Queue_1st();
    }
}

/** @brief Display the current load-request queue status (debug). */
static void disp_ldreq_status() {
    s16 i;

    flPrintColor(0xFFFFFF8F);

    if (Debug_w[DEBUG_LDREQ_QUEUE]) {
        for (i = 0; i < LDREQ_QUEUE_SIZE; i++) {
            flPrintL(2, i + 18, "%1d", q_ldreq[i].be);
            if (q_ldreq[i].type < LDREQ_PROCESS_COUNT) {
                flPrintL(3, i + 18, ldreq_process_name[q_ldreq[i].type]);
            }
        }

        flPrintL(2, i + 18, "%4d", g_state.system_timer);
    }
}

/** @brief Check whether the load-request queue is empty. */
s32 Check_LDREQ_Clear() {
    return q_ldreq->be == 0 && q_ldreq[1].be == 0;
}

/** @brief Check whether a player's load requests have completed. */
s32 Check_LDREQ_Queue_Player(s16 id) {
    s16 i;
    s16 kara;
    s16 made;

    if (id < 0 || id >= PLAYER_COUNT || plt_req[id] < 0 || plt_req[id] >= LDREQ_IX_SIZE) {
        return 0;
    }
    kara = ldreq_ix[plt_req[id]][0];
    made = kara + ldreq_ix[plt_req[id]][1];

    for (i = kara; i < made; i++) {
        if (!(ldreq_result[i] & lpr_wrdata[id])) {
            break;
        }
    }

    if (i != made) {
        return 0;
    }

    return 1;
}

/** @brief Check whether a background's load requests have completed. */
s32 Check_LDREQ_Queue_BG(s16 ix) {
    return Check_LDREQ_Queue_Union(ix + 20);
}

/** @brief Check whether union (shared) load requests have completed. */
static s32 Check_LDREQ_Queue_Union(s16 ix) {
    s16 i;
    s16 kara;
    s16 made;

    if (ix < 0 || ix >= LDREQ_IX_SIZE) {
        return 0;
    }
    kara = ldreq_ix[ix][0];
    made = kara + ldreq_ix[ix][1];

    for (i = kara; i < made; i++) {
        if (!(ldreq_result[i] & lpr_wrdata[2])) {
            break;
        }
    }

    if (i != made) {
        return 0;
    }

    return 1;
}

/** @brief Check whether a direct load request has completed. */
s32 Check_LDREQ_Queue_Direct(s16 ix) {
    if (!(ldreq_result[ix] & lpr_wrdata[2])) {
        return 0;
    }

    return 1;
}

/** @brief Error handler for invalid load-request process types. */
static void q_ldreq_error(REQ* curr) {
    curr->be = 0;
    flLogOut("Q_LDREQ_ERROR : ロード処理の指定に誤りがあります。\n");
}

const LDREQ_Process_Func ldreq_process[LDREQ_PROCESS_COUNT] = { q_ldreq_error,      q_ldreq_texture_group,
                                                                q_ldreq_color_data, q_ldreq_color_data,
                                                                q_ldreq_color_data, q_ldreq_color_data };

s8* ldreq_process_name[] = { "EMP", "TEX", "COL", "SCR", "SND", "KNJ" };

/* ldreq_tbl[] and ldreq_ix[] data tables moved to gd_data.c */
