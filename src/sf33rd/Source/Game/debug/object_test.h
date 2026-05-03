/**
 * @file object_test.h
 * @brief Public API for vibration/force-feedback data snapshot.
 *
 * Part of the debug module.
 */

#ifndef _OBJECT_TEST_H_
#define _OBJECT_TEST_H_

#include "structs.h"
#include "types.h"

#define OT_PULREQ_MAX 51
#define OT_PULPARA_MAX 53
#define OT_PULREQ_XX_MAX 32

extern PPWORK_SUB_SUB ot_pulreq_xx[OT_PULREQ_XX_MAX];
extern RumbleRequest ot_pulreq[OT_PULREQ_MAX];
extern RumbleParams ot_pulpara[OT_PULPARA_MAX];

void ot_make_curr_vib_data();

#endif
