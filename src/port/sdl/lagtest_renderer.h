#ifndef LAGTEST_RENDERER_H
#define LAGTEST_RENDERER_H

/**
 * @brief Render the input lag test OSD overlay.
 *
 * When GPIO lag test mode is active, draws big frame-counter text
 * in the native CPS3 font showing current frame, receive frame,
 * and active frame with lag delta. No-op when GPIO test is disabled.
 */
void LagtestRenderer_Render(void);

#endif /* LAGTEST_RENDERER_H */
