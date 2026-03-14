/**
 * @file net_detect.c
 * @brief Detect active network interface type (WiFi vs Wired).
 *
 * Windows: GetAdaptersAddresses() — checks IfType on the adapter with a
 *          default gateway (i.e. the one actually routing traffic).
 * Linux:   ioctl(SIOCGIWNAME) — probes each interface with getifaddrs();
 *          if the ioctl succeeds, the interface supports wireless extensions.
 */
#include "net_detect.h"

#include <string.h>

#ifdef _WIN32

/* ======== Windows: GetAdaptersAddresses ======== */
#include <winsock2.h>
#include <iphlpapi.h>
#include <ipifcons.h>
#ifdef _MSC_VER
#pragma comment(lib, "iphlpapi.lib")
#endif

#ifndef IF_TYPE_IEEE80211
#define IF_TYPE_IEEE80211 71
#endif

const char* NetDetect_GetConnectionType(void) {
    ULONG buf_size = 15000;
    IP_ADAPTER_ADDRESSES* addrs = NULL;
    ULONG ret;
    const char* result = NET_CONN_UNKNOWN;

    /* Allocate and call GetAdaptersAddresses with retry */
    for (int attempt = 0; attempt < 3; attempt++) {
        addrs = (IP_ADAPTER_ADDRESSES*)malloc(buf_size);
        if (!addrs)
            return NET_CONN_UNKNOWN;

        ret = GetAdaptersAddresses(AF_UNSPEC,
                                   GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                                       GAA_FLAG_SKIP_DNS_SERVER,
                                   NULL,
                                   addrs,
                                   &buf_size);
        if (ret == ERROR_BUFFER_OVERFLOW) {
            free(addrs);
            addrs = NULL;
            continue;
        }
        break;
    }

    if (ret != NO_ERROR || !addrs) {
        free(addrs);
        return NET_CONN_UNKNOWN;
    }

    /* Find the first adapter with a default gateway and OperStatus UP */
    for (IP_ADAPTER_ADDRESSES* a = addrs; a; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp)
            continue;
        if (!a->FirstGatewayAddress)
            continue;

        if (a->IfType == IF_TYPE_IEEE80211) {
            result = NET_CONN_WIFI;
        } else if (a->IfType == MIB_IF_TYPE_ETHERNET) {
            result = NET_CONN_WIRED;
        }
        break; /* Use the first active adapter with a gateway */
    }

    free(addrs);
    return result;
}

#else /* !_WIN32 */

/* ======== POSIX: interface type detection ======== */

#ifdef __linux__
/* Linux: ioctl(SIOCGIWNAME) probes each interface for wireless extensions. */
#include <ifaddrs.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

/* linux/wireless.h defines SIOCGIWNAME; if unavailable, define it manually.
 * Value 0x8B01 is stable across all Linux kernels since 2.0. */
#ifndef SIOCGIWNAME
#define SIOCGIWNAME 0x8B01
#endif

const char* NetDetect_GetConnectionType(void) {
    struct ifaddrs* ifa_list = NULL;
    if (getifaddrs(&ifa_list) != 0)
        return NET_CONN_UNKNOWN;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        freeifaddrs(ifa_list);
        return NET_CONN_UNKNOWN;
    }

    const char* result = NET_CONN_UNKNOWN;

    for (struct ifaddrs* ifa = ifa_list; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr)
            continue;
        if (ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if (!(ifa->ifa_flags & IFF_UP) || !(ifa->ifa_flags & IFF_RUNNING))
            continue;
        if (ifa->ifa_flags & IFF_LOOPBACK)
            continue;

        /* Try wireless ioctl */
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ - 1);

        if (ioctl(sock, SIOCGIWNAME, &ifr) == 0) {
            result = NET_CONN_WIFI;
        } else {
            result = NET_CONN_WIRED;
        }
        break; /* Use the first active non-loopback interface */
    }

    close(sock);
    freeifaddrs(ifa_list);
    return result;
}

#else
/* macOS / other POSIX: no wireless ioctl available, report unknown. */
const char* NetDetect_GetConnectionType(void) {
    return NET_CONN_UNKNOWN;
}
#endif

#endif /* _WIN32 */
