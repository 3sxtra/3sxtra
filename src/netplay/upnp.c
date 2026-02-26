/**
 * @file upnp.c
 * @brief UPnP port mapping wrapper using miniupnpc.
 *
 * Provides a simple interface to create/remove UDP port mappings
 * on the local router via UPnP IGD protocol.
 * Compiled only when HAVE_UPNP is defined (miniupnpc available).
 */
#include "upnp.h"

#ifdef HAVE_UPNP

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>

/* miniupnpc API v18+ (v2.2.8) added wan_addr params to UPNP_GetValidIGD */
#ifndef MINIUPNPC_API_VERSION
#define MINIUPNPC_API_VERSION 0
#endif

#define UPNP_DISCOVER_TIMEOUT_MS 2000
#define UPNP_LEASE_DURATION "3600" // 1 hour lease

bool Upnp_AddMapping(UpnpMapping* out, uint16_t internal_port, uint16_t external_port, const char* protocol) {
    if (!out)
        return false;
    memset(out, 0, sizeof(*out));

    int error = 0;
    struct UPNPDev* devlist = upnpDiscover(UPNP_DISCOVER_TIMEOUT_MS, NULL, NULL, 0, 0, 2, &error);

    if (!devlist) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "UPnP: No IGD devices found (error %d)", error);
        return false;
    }

    struct UPNPUrls urls;
    struct IGDdatas data;
    char lan_addr[64] = { 0 };
    char wan_addr[64] = { 0 };

#if MINIUPNPC_API_VERSION >= 18
    int status = UPNP_GetValidIGD(devlist, &urls, &data, lan_addr, sizeof(lan_addr), wan_addr, sizeof(wan_addr));
#else
    int status = UPNP_GetValidIGD(devlist, &urls, &data, lan_addr, sizeof(lan_addr));
#endif

    if (status != 1) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "UPnP: No valid IGD found (status %d)", status);
        freeUPNPDevlist(devlist);
        return false;
    }

    SDL_Log("UPnP: Found IGD, LAN address: %s", lan_addr);

    // Get external IP
    char ext_ip[64] = { 0 };
    int r = UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, ext_ip);
    if (r != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "UPnP: Failed to get external IP (error %d)", r);
        FreeUPNPUrls(&urls);
        freeUPNPDevlist(devlist);
        return false;
    }

    // Try to add port mapping
    char int_port_str[8], ext_port_str[8];
    snprintf(int_port_str, sizeof(int_port_str), "%u", internal_port);
    snprintf(ext_port_str, sizeof(ext_port_str), "%u", external_port);

    r = UPNP_AddPortMapping(urls.controlURL,
                            data.first.servicetype,
                            ext_port_str,
                            int_port_str,
                            lan_addr,
                            "3SX Netplay",
                            protocol,
                            NULL,
                            UPNP_LEASE_DURATION);

    if (r != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "UPnP: AddPortMapping failed: %s (error %d)", strupnperror(r), r);
        FreeUPNPUrls(&urls);
        freeUPNPDevlist(devlist);
        return false;
    }

    SDL_Log("UPnP: Port mapping created %s:%s -> %s:%s (%s)", ext_ip, ext_port_str, lan_addr, int_port_str, protocol);

    SDL_strlcpy(out->external_ip, ext_ip, sizeof(out->external_ip));
    out->external_port = external_port;
    out->internal_port = internal_port;
    out->active = true;

    FreeUPNPUrls(&urls);
    freeUPNPDevlist(devlist);
    return true;
}

void Upnp_RemoveMapping(UpnpMapping* mapping) {
    if (!mapping || !mapping->active)
        return;

    int error = 0;
    struct UPNPDev* devlist = upnpDiscover(UPNP_DISCOVER_TIMEOUT_MS, NULL, NULL, 0, 0, 2, &error);

    if (!devlist)
        return;

    struct UPNPUrls urls;
    struct IGDdatas data;
    char lan_addr[64] = { 0 };
    char wan_addr[64] = { 0 };

#if MINIUPNPC_API_VERSION >= 18
    if (UPNP_GetValidIGD(devlist, &urls, &data, lan_addr, sizeof(lan_addr), wan_addr, sizeof(wan_addr)) == 1) {
#else
    if (UPNP_GetValidIGD(devlist, &urls, &data, lan_addr, sizeof(lan_addr)) == 1) {
#endif
        char ext_port_str[8];
        snprintf(ext_port_str, sizeof(ext_port_str), "%u", mapping->external_port);

        int r = UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, ext_port_str, "UDP", NULL);
        if (r == 0) {
            SDL_Log("UPnP: Port mapping removed for port %u", mapping->external_port);
        } else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "UPnP: Failed to remove port mapping: %s", strupnperror(r));
        }
        FreeUPNPUrls(&urls);
    }

    freeUPNPDevlist(devlist);
    mapping->active = false;
}

bool Upnp_GetExternalIP(char* out_ip, int ip_buf_size) {
    int error = 0;
    struct UPNPDev* devlist = upnpDiscover(UPNP_DISCOVER_TIMEOUT_MS, NULL, NULL, 0, 0, 2, &error);

    if (!devlist)
        return false;

    struct UPNPUrls urls;
    struct IGDdatas data;
    char lan_addr[64] = { 0 };
    char wan_addr[64] = { 0 };

    bool success = false;
#if MINIUPNPC_API_VERSION >= 18
    if (UPNP_GetValidIGD(devlist, &urls, &data, lan_addr, sizeof(lan_addr), wan_addr, sizeof(wan_addr)) == 1) {
#else
    if (UPNP_GetValidIGD(devlist, &urls, &data, lan_addr, sizeof(lan_addr)) == 1) {
#endif
        char ext_ip[64] = { 0 };
        if (UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, ext_ip) == 0) {
            SDL_strlcpy(out_ip, ext_ip, ip_buf_size);
            success = true;
        }
        FreeUPNPUrls(&urls);
    }

    freeUPNPDevlist(devlist);
    return success;
}

#else // !HAVE_UPNP — stubs

bool Upnp_AddMapping(UpnpMapping* out, uint16_t internal_port, uint16_t external_port, const char* protocol) {
    (void)out;
    (void)internal_port;
    (void)external_port;
    (void)protocol;
    return false;
}

void Upnp_RemoveMapping(UpnpMapping* mapping) {
    (void)mapping;
}

bool Upnp_GetExternalIP(char* out_ip, int ip_buf_size) {
    (void)out_ip;
    (void)ip_buf_size;
    return false;
}

#endif // HAVE_UPNP
