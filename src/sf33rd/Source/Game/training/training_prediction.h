/**
 * @file training_prediction.h
 * @brief Forward simulation of physics and hitboxes for the Training AI.
 */

#ifndef TRAINING_PREDICTION_H
#define TRAINING_PREDICTION_H

#include "structs.h"
#include "training_state.h"

typedef struct {
    s16 pos_x;
    s16 pos_y;
    s16 speed_x;
    s16 speed_y;
    s16 accel_x;
    s16 accel_y;

    // Predicted hitboxes
    UNK_1* p_body;
    UNK_6* p_pushbox;
} PredictedPlayerState;

typedef struct {
    PredictedPlayerState p1;
    PredictedPlayerState p2;
    s32 frame_offset; // How many frames ahead this represents
} PredictedGameState;

// Projects the state strictly based on physics/gravity without advancing animations
void predict_physics_state(PredictedGameState* out, s16 frames_ahead);

#endif // TRAINING_PREDICTION_H
