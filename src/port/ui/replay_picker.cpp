/**
 * @file replay_picker.cpp
 * @brief DEPRECATED — Legacy ImGui replay picker stub.
 *
 * All replay picker functionality is now in rmlui_replay_picker.
 * This file provides empty stub implementations to satisfy the linker
 * for any remaining references.
 */

#include "port/ui/replay_picker.h"

extern "C" void ReplayPicker_Open(int mode) {
    (void)mode;
}
extern "C" int ReplayPicker_IsOpen(void) {
    return 0;
}
extern "C" int ReplayPicker_GetSelectedSlot(void) {
    return -1;
}
extern "C" int ReplayPicker_Update(void) {
    return -1;
}
