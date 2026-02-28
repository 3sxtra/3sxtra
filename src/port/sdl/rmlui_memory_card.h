#pragma once
/**
 * @file rmlui_memory_card.h
 * @brief RmlUi Memory Card (Save/Load) Screen — replaces CPS3 effect_57/61/64/66
 *        objects in Memory_Card() with an HTML/CSS overlay.
 */

#ifdef __cplusplus
extern "C" {
#endif

void rmlui_memory_card_init(void);
void rmlui_memory_card_update(void);
void rmlui_memory_card_show(void);
void rmlui_memory_card_hide(void);
void rmlui_memory_card_shutdown(void);

#ifdef __cplusplus
}
#endif
