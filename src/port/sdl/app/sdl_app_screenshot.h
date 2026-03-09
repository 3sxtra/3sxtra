/**
 * @file sdl_app_screenshot.h
 * @brief Screenshot capture API for the SDL application.
 *
 * Provides a deferred capture mechanism: request a screenshot during input
 * handling, then process the pending capture at end-of-frame when the
 * framebuffer is fully composited.
 */
#ifndef SDL_APP_SCREENSHOT_H
#define SDL_APP_SCREENSHOT_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Request a screenshot capture on the next end-of-frame. */
void SDLAppScreenshot_RequestCapture(void);

/** @brief If a capture was requested, read-back the framebuffer and save it.
 *  Must be called after all rendering is complete (end-of-frame). */
void SDLAppScreenshot_ProcessPending(void);

#ifdef __cplusplus
}
#endif

#endif /* SDL_APP_SCREENSHOT_H */
