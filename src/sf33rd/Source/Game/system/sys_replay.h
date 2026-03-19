/**
 * @file sys_replay.h
 * @brief Replay record/playback API.
 *
 * Split from sys_sub.h — replay initialization, frame recording,
 * and playback dispatch.
 */
#ifndef SYS_REPLAY_H
#define SYS_REPLAY_H

#include "types.h"

void Check_Replay(void);
void Check_Replay_Status(s16 PL_id, u8 Status);

#endif
