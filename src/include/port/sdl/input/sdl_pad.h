/**
 * @file sdl_pad.h (public forwarding header)
 * @brief Forwards to the canonical sdl_pad.h in port/sdl/.
 *
 * The authoritative header lives at port/sdl/sdl_pad.h.
 * This forwarding header exists so code that includes via the public
 * include/ tree still gets the full declaration set without duplication.
 */
#ifndef SRC_INCLUDE_PORT_SDL_PAD_FORWARD
#define SRC_INCLUDE_PORT_SDL_PAD_FORWARD
#include "../../../../src/port/sdl/input/sdl_pad.h"
#endif
