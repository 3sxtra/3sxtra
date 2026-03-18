/**
 * @file appear_registry.c
 * @brief Registry implementation for character entrance animations.
 *
 * Static array of AppearTypeCallbacks pointers, indexed by AppearTypeId.
 * Populated at startup via __attribute__((constructor)) self-registration
 * from the ap_*.c delegation wrappers.
 */

#include "port/appear_registry.h"

#include <assert.h>
#include <stddef.h>

static const AppearTypeCallbacks* g_appear_types[APPEAR_TYPE_COUNT];

void AppearType_Register(AppearTypeId id, const AppearTypeCallbacks* cb) {
    assert(id >= 0 && id < APPEAR_TYPE_COUNT);
    assert(g_appear_types[id] == NULL && "Duplicate AppearType registration");
    g_appear_types[id] = cb;
}

const AppearTypeCallbacks* AppearType_Get(AppearTypeId id) {
    if (id < 0 || id >= APPEAR_TYPE_COUNT) {
        return NULL;
    }
    return g_appear_types[id];
}
