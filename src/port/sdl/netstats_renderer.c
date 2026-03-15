#include "port/sdl/netstats_renderer.h"
#include "netplay/netplay.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"

#include <SDL3/SDL.h>

void NetstatsRenderer_Render() {
    if (Netplay_GetSessionState() != NETPLAY_SESSION_RUNNING) {
        return;
    }

    NetworkStats stats = { 0 };
    Netplay_GetNetworkStats(&stats);

    char buffer[64];
    SDL_snprintf(buffer, sizeof(buffer), "R:%d P:%d", stats.rollback, stats.ping);

    // Screen is 384 pixels wide. SSPutStrPro expects pixel coordinates.
    // Assuming 6 pixels per character for the proportional font, plus 4px padding.
    int len = SDL_strlen(buffer);
    int x = 384 - (len * 6) - 4;

    // SSPutStrPro(flag, x, y, atr, vtxcol, str)
    // using atr=6 (palette 6) per user instructions
    SSPutStrPro(0, x, 2, 6, 0xFFFFFFFF, (s8*)buffer);
}
