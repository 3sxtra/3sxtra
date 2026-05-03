/**
 * @file sound_effect_data.h
 * @brief Sound effect dispatch table types and API.
 *
 * Declares the SeHandlerType enum-tag lookup table that maps each SE code
 * (0–1023) to its handler type (Shock, Myself, Let, etc.), and the
 * Se_Dispatch() function that provides a single typed entry point for
 * all sound effect requests.
 *
 * Part of the sound module.
 * Originally from the PS2 game module.
 */

#ifndef SOUND_EFFECT_DATA_H
#define SOUND_EFFECT_DATA_H

#include "structs.h"
#include "types.h"

/**
 * @brief Handler type tags for sound effect dispatch.
 *
 * Each SE code in the range [0, 1023] maps to one of these handler types.
 * The Se_Dispatch() function uses this tag to call the correct handler.
 */
typedef enum SeHandlerType {
    SOUND_EFFECTS_HANDLER_CALL_SE = 0,    /**< Generic SE — plays at caller's screen position */
    SOUND_EFFECTS_HANDLER_SHOCK = 1,      /**< Hit/shock SE — switches to KO variant if dead  */
    SOUND_EFFECTS_HANDLER_MYSELF = 2,     /**< Play SE on caller's own player channel          */
    SOUND_EFFECTS_HANDLER_MYSELF_DIE = 3, /**< Like MYSELF but only if character is alive      */
    SOUND_EFFECTS_HANDLER_LET = 4,        /**< Play SE on the target's channel (hit reaction)  */
    SOUND_EFFECTS_HANDLER_LET_SP = 5,     /**< Like LET with KO hit override codes             */
    SOUND_EFFECTS_HANDLER_TERM = 6,       /**< Termination SE — airborne + alive gate          */
    SOUND_EFFECTS_HANDLER_DUMMY = 7       /**< Dummy — stores code for debug, plays nothing    */
} SeHandlerType;

#define SE_DISPATCH_TABLE_SIZE 1024

/** @brief The handler-type lookup table (indexed by SE code). */
extern const SeHandlerType se_handler_type[SE_DISPATCH_TABLE_SIZE];

/**
 * @brief Dispatch a sound effect request to the correct handler.
 *
 * Replaces the legacy pattern:
 *   sound_effect_request[index](ewk, code);
 * with:
 *   Se_Dispatch(index, code, ewk);
 *
 * @param index Handler-type table index (0–1023). Selects which handler
 *              (Se_Shock, Se_Myself, Se_Let, etc.) to call.
 * @param code  Sound code passed through to the handler. Often equals
 *              index, but may differ (e.g. metamorphosis offset, or
 *              filtered SE code from check_xcopy_filter_se_req()).
 * @param ewk   Pointer to the requesting entity's work struct.
 */
void Se_Dispatch(u16 index, u16 code, State_Other* ewk);

#endif
