/**
 * @file appear_states.h
 * @brief Named constants for routine_no[] indices used in appear.c.
 *
 * Part of the animation module — Task #38 modernization.
 */

#ifndef APPEAR_STATES_H
#define APPEAR_STATES_H

/* routine_no index aliases for character entrance animations */
#define APPEAR_RNO_COMPLETE   2   /**< routine_no[2]: 1 = appear finished */
#define APPEAR_RNO_PHASE      3   /**< routine_no[3]: sub-state within handler */
#define APPEAR_RNO_TYPE       4   /**< routine_no[4]: appear-type selector */
#define APPEAR_RNO_GOUKI      6   /**< routine_no[6]: gouki teleport sub-state */

#endif /* APPEAR_STATES_H */
