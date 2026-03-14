/**
 * @file net_detect.h
 * @brief Detect active network interface type (WiFi vs Wired).
 *
 * Uses OS-specific APIs:
 *   - Windows: GetAdaptersAddresses() + IfType == IF_TYPE_IEEE80211
 *   - Linux:   ioctl(SIOCGIWNAME) on active interfaces
 */
#ifndef NETPLAY_NET_DETECT_H
#define NETPLAY_NET_DETECT_H

#ifdef __cplusplus
extern "C" {
#endif

/// Connection type string constants
#define NET_CONN_WIFI "wifi"
#define NET_CONN_WIRED "wired"
#define NET_CONN_UNKNOWN "unknown"

/// Detect the connection type of the active default-route network interface.
/// Returns one of NET_CONN_WIFI, NET_CONN_WIRED, or NET_CONN_UNKNOWN.
/// This is a pointer to a static string — do not free.
const char* NetDetect_GetConnectionType(void);

#ifdef __cplusplus
}
#endif

#endif
