#pragma once
/**
 * @file rmlui_copyright.h
 * @brief RmlUi copyright text overlay — replaces Disp_Copyright() SSPutStrPro calls.
 */

#ifdef __cplusplus
extern "C" {
#endif

void rmlui_copyright_init(void);
void rmlui_copyright_update(void);
void rmlui_copyright_shutdown(void);

#ifdef __cplusplus
}
#endif
