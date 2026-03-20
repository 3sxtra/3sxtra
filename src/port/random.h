#ifndef PORT_RANDOM_H
#define PORT_RANDOM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generate cryptographically secure random bytes.
 *
 * Uses platform-specific CSPRNGs (BCryptGenRandom on Windows, arc4random_buf
 * on macOS, getrandom on Linux). Falls back to SDL_rand_bits if native
 * APIs are unavailable or fail.
 *
 * @param buf Buffer to receive random bytes.
 * @param len Number of bytes to generate.
 */
void Sys_RandomBytes(void* buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif // PORT_RANDOM_H
