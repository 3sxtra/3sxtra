#ifndef SDL_APP_INPUT_H
#define SDL_APP_INPUT_H

#include <SDL3/SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Process an SDL event for application-level inputs (menus, shortcuts, etc.)
 * @param event The SDL event to process
 * @return true if the event was handled and should not propagate further
 */
bool SDLAppInput_HandleEvent(SDL_Event* event);

#ifdef __cplusplus
}
#endif

#endif // SDL_APP_INPUT_H
