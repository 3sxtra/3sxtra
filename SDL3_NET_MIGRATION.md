# SDL3_Net Migration Plan

> Migrate raw Winsock/BSD socket code to SDL3_Net for cross-platform simplicity.
> Inspired by HeatXD's [client-matchmaking-poc](https://github.com/crowded-street/3sx/compare/main...HeatXD:3sx:client-matchmaking-poc).

---

## Migration Status (March 15, 2026)

> [!IMPORTANT]
> **Migration is COMPLETE and verified.** All phases applied, builds pass, STUN discovery works at runtime.

### ✅ Completed

| Phase | Status | Notes |
|-------|--------|-------|
| Phase 1: Add SDL3_Net dependency | ✅ Done | `build-deps.sh`, `CMakeLists.txt`, RPi4 scripts, Flatpak manifest |
| Phase 2: GekkoNet adapter module | ✅ Done | `sdl_net_adapter.c/h` from commit 33b9a83 |
| Phase 3: Migrate STUN | ✅ Done | `stun.c/h` — raw sockets → SDL3_Net + IPv4-forced resolution |
| Phase 4: Discovery listener | ✅ Done | `discovery.c` listen socket migrated (per-NIC broadcast stays raw) |
| Phase 5: net_detect | ✅ N/A | Stays on raw OS APIs (by design) |

### 🔧 Issues Found & Fixed After Cherry-Pick

| # | Issue | Fix |
|---|-------|-----|
| 1 | `sdl_netplay_ui.cpp` not updated — broken call sites | Migrated 6 sites: `socket_fd`→`socket`, removed `Stun_SetNonBlocking`, `ntohs`→`SDL_Swap16BE` |
| 2 | `ping_probe.c/h` not updated — raw socket APIs | Full rewrite to `NET_SendDatagram`/`NET_ReceiveDatagram` with per-peer `NET_Address*` caching |
| 3 | `SDLNetAdapter_Destroy()` never called | Added call in `netplay.c` cleanup before `NET_DestroyDatagramSocket` |
| 4 | `Stun_HolePunch()` dropped `*peer_ip` updates | Restored by parsing `NET_GetAddressString()` back to `uint32_t` |
| 5 | `ntohs()` undefined (winsock2.h removed) | Replaced with `SDL_Swap16BE()` in `sdl_netplay_ui.cpp` |
| 6 | STUN returns IPv6 on dual-stack Windows | Added `resolve_hostname_ipv4()` helper + `0.0.0.0` socket bind to force IPv4 |

### 🔍 Audit Remediation (March 15, 2026)

A full netplay stack audit was performed and 14 fixes applied:

| # | Finding | Fix | File(s) |
|---|---------|-----|---------|
| M2 | `NET_GetAddressString` called twice in same expression (static buffer race) | Copy first result to local buffer | `stun.c` |
| M6 | Winsock not initialized before raw broadcast socket | Reordered: SDL3_Net listen socket created first | `discovery.c` |
| H1/H2 | Adapter singleton — single cached peer, no spectator support | Per-peer address cache (8 slots), double-create guard | `sdl_net_adapter.c` |
| H3 | SSE thread race on `s_sse_sock` | Changed to `SDL_AtomicInt` | `lobby_server.c` |
| M1 | `NET_SendDatagram` return unchecked in hole-punch | Added return check + warning log | `stun.c` |
| M4 | SSE ring buffer silent overflow | Overflow detection, advance read idx on overflow | `lobby_server.c` |
| M7 | `handshake_ready_since` static inside switch-case | Moved to file scope, reset on lobby entry | `netplay.c` |
| L1 | `NET_Init()`/`NET_Quit()` not called | Added in `SDLApp_Init()`/`SDLApp_Quit()` | `sdl_app.c` |
| L3 | Double body extraction in `GetPlayerStats` | Removed redundant `strstr` | `lobby_server.c` |
| L5 | `public_port` endianness comment wrong | Corrected to "Host byte order" | `stun.h` |
| L6 | `gekkonet.h` included twice | Consolidated with `#define Game` workaround | `netplay.c` |
| L8 | `strncpy` without guaranteed null-termination | Replaced with `SDL_strlcpy` | `lobby_server.c` |
| M3 | Ping probe token fragile across slot reuse | Generation counter in token (hi 8 bits) | `ping_probe.c` |
| M5 | JSON parser truncates nested objects | `find_object_end()` with brace-depth tracking | `lobby_server.c` |

### ✅ Post-Audit Improvements (March 15, 2026)

| # | Item | Notes |
|---|------|-------|
| 1 | `lobby_server.c` HTTP → libcurl | `http_connect()` + `http_request()` + `UploadReplay()` rewritten with libcurl. SSE thread stays on raw sockets (`sse_raw_connect()`). HMAC signing unchanged. cJSON vendored in `third_party/cJSON/`. |
| 2 | UPnP IGD caching | `upnp_ensure_cached()` caches IGD URLs after first discovery. `RemoveMapping`/`GetExternalIP` skip 2s `upnpDiscover()` round-trip. `Upnp_InvalidateCache()` added. |
| 3 | `lobby_server.c` JSON → cJSON | Hand-rolled JSON helpers replaced with robust cJSON wrappers. All body-builders and response-parsers migrated. |
| 4 | Identity CSPRNG | `SDL_rand_bits()` replaced with platform-native APIs `BCryptGenRandom` (Windows) and `getrandom()` (Linux) for cryptographic grade randomness in player identity generation. |

### ⏳ Remaining

- **Manual testing**: LAN discovery, hole punching, gameplay, spectator, casual rooms
- **IPv6 support**: STUN parser only handles family=1 (IPv4). Full IPv6 requires changing IP representation from `uint32_t` across entire stack. TODO in `stun.c`.
- **`discovery.c` per-NIC broadcast**: Still uses raw `sendto()` with `GetAdaptersAddresses`/`getifaddrs`. Cannot migrate without OS API abstraction.

## Table of Contents

1. [Motivation](#motivation)
2. [Current State — Raw Socket Inventory](#current-state--raw-socket-inventory)
3. [What HeatXD's PoC Does Differently](#what-heatxds-poc-does-differently)
4. [SDL3_Net API Overview](#sdl3_net-api-overview)
5. [Migration Plan](#migration-plan)
   - [Phase 1: Add SDL3_Net Dependency](#phase-1-add-sdl3_net-dependency)
   - [Phase 2: GekkoNet Adapter Module](#phase-2-gekkonet-adapter-module)
   - [Phase 3: Migrate STUN to SDL3_Net](#phase-3-migrate-stun-to-sdl3_net)
   - [Phase 4: Migrate Discovery Listener](#phase-4-migrate-discovery-listener)
   - [Phase 5: net_detect (No Migration)](#phase-5-net_detect-no-migration)
6. [Files Changed Summary](#files-changed-summary)
7. [Build System Changes](#build-system-changes)
8. [Risks & Open Questions](#risks--open-questions)
9. [Verification Plan](#verification-plan)

---

## Motivation

Our netplay stack currently uses **raw BSD/Winsock sockets** everywhere — creating UDP sockets, binding, setting `SO_BROADCAST`, toggling non-blocking mode, calling `sendto()`/`recvfrom()`, and wrapping it all in `#ifdef _WIN32` / `#else` blocks. This works, but:

| Problem | Impact |
|---------|--------|
| ~15 `#ifdef _WIN32` blocks across 3 files | Maintenance burden, subtle platform bugs |
| Manual `WSAStartup`/`WSACleanup` lifecycle | Easy to leak or double-init |
| Separate non-blocking setup per platform | `ioctlsocket` vs `fcntl` — easy to get wrong |
| STUN socket adapter duplicated | `stun_adapter_send/recv/free` in `netplay.c` reimplements what GekkoNet's default adapter does |
| Windows `SIO_UDP_CONNRESET` workaround | Obscure, easy to forget on new sockets |

**SDL3_Net** (`SDL_net`) wraps all of this behind a clean cross-platform API:
- `NET_CreateDatagramSocket(addr, port)` — creates, binds, enables broadcast, sets non-blocking
- `NET_SendDatagram(sock, addr, port, data, len)` — unified send
- `NET_ReceiveDatagram(sock, &dgram)` — unified receive with address info
- `NET_ResolveHostname(host)` — async DNS resolution

We already use SDL3 for everything else (windowing, audio, threads, timers, crypto). Adding SDL3_Net completes the networking story.

---

## Current State — Raw Socket Inventory

### `stun.c` (509 lines)

| Section | Lines | Raw Socket APIs Used |
|---------|-------|---------------------|
| `Stun_Discover()` | 210–334 | `socket()`, `bind()`, `setsockopt(SO_REUSEADDR)`, `sendto()`, `recvfrom()`, `getsockname()`, `getaddrinfo()`, `closesocket()` |
| `Stun_SetNonBlocking()` | 337–363 | `ioctlsocket(FIONBIO)` / `fcntl(O_NONBLOCK)`, Windows `WSAIoctl(SIO_UDP_CONNRESET)` |
| `Stun_HolePunch()` | 365–449 | `sendto()`, `recvfrom()`, `setsockopt(SO_RCVTIMEO)` |
| `Stun_SocketSendTo()` | 460–484 | `inet_pton()`, `sendto()` — GekkoNet adapter helper |
| `Stun_SocketRecvFrom()` | 486–502 | `recvfrom()`, `inet_ntop()` — GekkoNet adapter helper |
| `Stun_SocketClose()` | 504–508 | `closesocket()` |
| Platform headers | 8–31 | `<winsock2.h>`, `<ws2tcpip.h>`, `<sys/socket.h>`, `<arpa/inet.h>`, `<netdb.h>`, etc. |
| **KEEP (no sockets)** | 46–118 | `Stun_EncodeEndpoint`, `Stun_DecodeEndpoint`, `Stun_FormatIP` — pure byte math |
| **KEEP (no sockets)** | 120–208 | `build_binding_request`, `parse_binding_response` — STUN protocol parsing |

**Can migrate**: Everything except the encode/decode/parse functions (which have no socket dependency anyway).

### `discovery.c` (476 lines)

| Section | Lines | Raw Socket APIs Used |
|---------|-------|---------------------|
| Platform headers | 12–37 | `<winsock2.h>`, `<iphlpapi.h>`, `<sys/socket.h>`, `<ifaddrs.h>`, etc. |
| `set_nonblocking()` | 75–83 | `ioctlsocket(FIONBIO)` / `fcntl(O_NONBLOCK)` |
| `Discovery_Init()` | 85–132 | `socket()`, `setsockopt(SO_BROADCAST, SO_REUSEADDR, SO_REUSEPORT)`, `bind()` |
| `Discovery_Shutdown()` | 134–146 | `closesocket()`, `WSACleanup()` |
| Per-NIC broadcast (Windows) | 194–233 | `GetAdaptersAddresses()`, `sendto()` to computed directed broadcast |
| Per-NIC broadcast (Linux) | 234–256 | `getifaddrs()`, `sendto()` to `ifa_broadaddr` |
| Fallback broadcast | 259–270 | `sendto()` to `INADDR_BROADCAST` |
| Beacon receive | 273+ | `recvfrom()` |

**Partial migration**: The per-NIC directed broadcast enumeration (`GetAdaptersAddresses`/`getifaddrs`) uses OS APIs that SDL3_Net doesn't wrap. The listen socket and fallback broadcast can migrate. See [Phase 4](#phase-4-migrate-discovery-listener) for details.

### `net_detect.c` (145 lines)

| Section | Lines | Raw Socket APIs Used |
|---------|-------|---------------------|
| Windows WiFi detection | 14–76 | `GetAdaptersAddresses()`, `IfType == IF_TYPE_IEEE80211` |
| Linux WiFi detection | 82–135 | `getifaddrs()`, `ioctl(SIOCGIWNAME)` |

**Cannot migrate**: These are OS-specific NIC introspection APIs (`GetAdaptersAddresses`, `ioctl`) that SDL3_Net doesn't abstract. This file stays on raw sockets.

### `netplay.c` — GekkoNet STUN Adapter (lines 325–392)

| Section | Lines | Purpose |
|---------|-------|---------|
| `stun_adapter_send()` | 325–347 | Calls `Stun_SocketSendTo()` on the hole-punched socket fd |
| `stun_adapter_receive()` | 349–386 | Calls `Stun_SocketRecvFrom()`, allocates `GekkoNetResult` structs |
| `stun_adapter_free()` | 388–390 | `SDL_free()` wrapper |
| `stun_adapter` struct | 392 | Static `GekkoNetAdapter` instance |

**Fully replaceable** with a `sdl_net_adapter` module (Phase 2).

---

## What HeatXD's PoC Does Differently

HeatXD's `client-matchmaking-poc` branch introduces a clean pattern we can adopt:

### 1. `sdl_net_adapter.c` — Reusable GekkoNet ↔ SDL3_Net Adapter

```c
// Key pattern: one adapter per NET_DatagramSocket
GekkoNetAdapter* SDLNetAdapter_Create(NET_DatagramSocket* sock);
void SDLNetAdapter_Destroy(void);
```

**`send_data`**: Parses `"ip:port"` from `GekkoNetAddress`, resolves once via `NET_ResolveHostname()`, caches the `NET_Address*`, sends via `NET_SendDatagram()`. Lazy resolution means zero per-packet overhead after the first send.

**`receive_data`**: Drains all pending datagrams via `NET_ReceiveDatagram()` into a pool of `GekkoNetResult` structs (max 128 per call). Formats sender as `"ip:port"` string for GekkoNet.

**`free_data`**: Just `SDL_free()`.

### 2. Socket Reuse Pattern

The matchmaking socket becomes the gameplay socket:
```c
NET_DatagramSocket* mm_sock = Matchmaking_GetSocket();
if (mm_sock != NULL) {
    active_sock = mm_sock;  // Reuse for GekkoNet
} else {
    p2p_sock = NET_CreateDatagramSocket(NULL, local_port);
    active_sock = p2p_sock;
}
```

This is exactly what we do with `stun_socket_fd` → `stun_adapter`, but cleaner because `NET_DatagramSocket*` is a proper opaque handle instead of a raw `int fd`.

### 3. Clean Matchmaking State Machine

```
IDLE → RESOLVING_DNS → CONNECTING_TCP → AWAITING_ID → SENDING_UDP → AWAITING_MATCH → MATCHED
```

Single `Matchmaking_Run()` function with one `switch` per state. Each state does exactly one thing. Worth studying if we ever refactor our `lobby_async_state` handling.

---

## SDL3_Net API Overview

Key functions we'll use:

| Function | Purpose | Replaces |
|----------|---------|----------|
| `NET_Init()` | Initialize networking | `WSAStartup()` |
| `NET_Quit()` | Shutdown networking | `WSACleanup()` |
| `NET_CreateDatagramSocket(addr, port)` | Create UDP socket (auto: bind, broadcast, non-blocking) | `socket()` + `bind()` + `setsockopt()` + `fcntl/ioctlsocket` |
| `NET_DestroyDatagramSocket(sock)` | Close socket | `closesocket()` |
| `NET_SendDatagram(sock, addr, port, data, len)` | Send UDP packet | `sendto()` |
| `NET_ReceiveDatagram(sock, &dgram)` | Receive UDP packet | `recvfrom()` |
| `NET_DestroyDatagram(dgram)` | Free received datagram | (manual `free` of sockaddr) |
| `NET_ResolveHostname(host)` | Async DNS resolve | `getaddrinfo()` (blocking) |
| `NET_GetAddressStatus(addr)` | Check resolve status | (blocking wait) |
| `NET_GetAddressString(addr)` | Get IP string | `inet_ntop()` |
| `NET_RefAddress(addr)` / `NET_UnrefAddress(addr)` | Reference counting | (manual lifetime) |

**Key properties of `NET_CreateDatagramSocket`:**
- Automatically enables `SO_BROADCAST`
- Automatically sets non-blocking mode
- Handles `SO_REUSEADDR`
- On Windows, applies `SIO_UDP_CONNRESET` workaround internally
- Returns NULL on failure (no raw fd to check)

---

## Migration Plan

### Phase 1: Add SDL3_Net Dependency

**Goal**: Make SDL3_Net available to the build.

#### `build-deps.sh`

Add a new section (following the SDL3_mixer pattern):

```bash
# -----------------------------
# SDL3_net
# -----------------------------

SDL_NET_DIR="$THIRD_PARTY/sdl3_net"
SDL_NET_BUILD="$SDL_NET_DIR/build"

if [ "${SKIP_SDL3_BUILD:-}" = "1" ]; then
    echo "SKIP_SDL3_BUILD=1 — skipping SDL3_net (pre-installed)"
elif [ -d "$SDL_NET_BUILD" ]; then
    echo "SDL3_net already built at $SDL_NET_BUILD"
else
    echo "Building SDL3_net..."
    mkdir -p "$SDL_NET_DIR"
    cd "$SDL_NET_DIR"

    SDL_NET_SRC="$SDL_NET_DIR/SDL_net"

    if [ ! -d "$SDL_NET_SRC" ]; then
        echo "Cloning SDL3_net from git..."
        git clone --depth 1 https://github.com/libsdl-org/SDL_net.git "$SDL_NET_SRC"
    fi

    cd "$SDL_NET_SRC"
    mkdir -p build
    cd build

    case "$OS" in
        Darwin|Linux)
            CMAKE_EXTRA_ARGS=""
            if [ "$OS" = "Darwin" ] && [ "$TARGET_ARCH" = "universal" ]; then
                CMAKE_EXTRA_ARGS="-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
            fi
            cmake .. \
                ${CC:+-DCMAKE_C_COMPILER=$CC} \
                ${CXX:+-DCMAKE_CXX_COMPILER=$CXX} \
                -DCMAKE_INSTALL_PREFIX="$SDL_NET_BUILD" \
                -DSDL3_DIR="$SDL_BUILD/lib/cmake/SDL3" \
                -DCMAKE_PREFIX_PATH="$SDL_BUILD" \
                -DBUILD_SHARED_LIBS=ON \
                -DSDLNET_EXAMPLES=OFF \
                $CMAKE_EXTRA_ARGS
            ;;
        MINGW*|MSYS*|CYGWIN*)
            cmake .. \
                -G "MSYS Makefiles" \
                -DCMAKE_C_COMPILER=gcc \
                -DCMAKE_INSTALL_PREFIX="$SDL_NET_BUILD" \
                -DSDL3_DIR="$SDL_BUILD/lib/cmake/SDL3" \
                -DCMAKE_PREFIX_PATH="$SDL_BUILD" \
                -DBUILD_SHARED_LIBS=ON \
                -DSDLNET_EXAMPLES=OFF
            ;;
    esac

    cmake --build . -j$(nproc)
    cmake --install .
    echo "SDL3_net installed to $SDL_NET_BUILD"

    cd "$ROOT_DIR"
fi
```

#### `CMakeLists.txt`

Add `SDL3_net` alongside SDL3_mixer/SDL3_image:
```cmake
# SDL3_net
set(SDL3_NET_DIR "${THIRD_PARTY_DIR}/sdl3_net/build")
find_package(SDL3_net REQUIRED HINTS "${SDL3_NET_DIR}")
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3_net::SDL3_net)
```

#### Pi4 Cross-Compilation

The Pi4 build uses `third_party_rpi4/` which has pre-built libraries. SDL3_Net needs to be cross-compiled for ARM64 and placed at `third_party_rpi4/sdl3_net/build/`. Add the same build section to the Pi4 build script (adjust `CC`/`CXX` for cross-compiler).

#### GitHub Actions CI

Update `.github/workflows/` build matrix to include SDL3_Net in the dependency installation step.

#### Windows (Visual Studio / CMake preset)

On Windows, SDL3_Net builds alongside SDL3 via CMake. If using vcpkg or manual builds, add `SDL3_net` to the dependency list.

---

### Phase 2: GekkoNet Adapter Module

**Goal**: Create a clean, reusable adapter that wraps `NET_DatagramSocket*` for GekkoNet.

#### New Files

**`src/netplay/sdl_net_adapter.h`**
```c
#ifndef SDL_NET_ADAPTER_H
#define SDL_NET_ADAPTER_H

#include "gekkonet.h"
#include <SDL3_net/SDL_net.h>

/// Create a GekkoNet adapter backed by an existing SDL3_Net datagram socket.
/// The socket must outlive the adapter.
GekkoNetAdapter* SDLNetAdapter_Create(NET_DatagramSocket* sock);

/// Destroy the adapter and release cached DNS entries.
void SDLNetAdapter_Destroy(void);

#endif
```

**`src/netplay/sdl_net_adapter.c`**
```c
#include "sdl_net_adapter.h"
#include <SDL3/SDL.h>

#define MAX_NETWORK_RESULTS 128

static NET_DatagramSocket* adapter_sock = NULL;
static GekkoNetAdapter adapter;
static GekkoNetResult* results[MAX_NETWORK_RESULTS];
static int result_count = 0;

// Cached remote address (for single-peer sessions)
static NET_Address* cached_remote = NULL;
static Uint16 cached_port = 0;

static void send_data(GekkoNetAddress* addr, const char* data, int length) {
    if (!adapter_sock) return;

    if (!cached_remote) {
        char ip[64];
        int port = 0;
        SDL_sscanf((const char*)addr->data, "%63[^:]:%d", ip, &port);
        cached_remote = NET_ResolveHostname(ip);
        cached_port = (Uint16)port;
    }

    switch (NET_GetAddressStatus(cached_remote)) {
    case NET_SUCCESS:
        NET_SendDatagram(adapter_sock, cached_remote, cached_port, data, length);
        break;
    case NET_FAILURE:
        NET_UnrefAddress(cached_remote);
        cached_remote = NULL;
        break;
    case NET_WAITING:
        break; // Still resolving — GekkoNet will retransmit
    }
}

static GekkoNetResult** receive_data(int* length) {
    result_count = 0;
    if (!adapter_sock) {
        *length = 0;
        return results;
    }

    NET_Datagram* dgram = NULL;
    while (result_count < MAX_NETWORK_RESULTS
           && NET_ReceiveDatagram(adapter_sock, &dgram)
           && dgram) {
        const char* ip_str = NET_GetAddressString(dgram->addr);
        char addr_str[64];
        SDL_snprintf(addr_str, sizeof(addr_str), "%s:%d", ip_str, (int)dgram->port);

        GekkoNetResult* res = SDL_malloc(sizeof(GekkoNetResult));
        size_t addr_len = SDL_strlen(addr_str);
        res->addr.data = SDL_malloc(addr_len + 1);
        SDL_strlcpy((char*)res->addr.data, addr_str, addr_len + 1);
        res->addr.size = (unsigned int)addr_len;

        res->data = SDL_malloc(dgram->buflen);
        SDL_memcpy(res->data, dgram->buf, dgram->buflen);
        res->data_len = (unsigned int)dgram->buflen;

        results[result_count++] = res;
        NET_DestroyDatagram(dgram);
        dgram = NULL;
    }

    *length = result_count;
    return results;
}

static void free_data(void* ptr) {
    SDL_free(ptr);
}

GekkoNetAdapter* SDLNetAdapter_Create(NET_DatagramSocket* sock) {
    adapter_sock = sock;
    adapter.send_data = send_data;
    adapter.receive_data = receive_data;
    adapter.free_data = free_data;
    return &adapter;
}

void SDLNetAdapter_Destroy(void) {
    adapter_sock = NULL;
    if (cached_remote) {
        NET_UnrefAddress(cached_remote);
        cached_remote = NULL;
    }
    cached_port = 0;
}
```

#### Changes to `netplay.c`

- **Remove** `stun_adapter_send()`, `stun_adapter_receive()`, `stun_adapter_free()`, `stun_adapter` (~70 lines)
- **Remove** `#include "stun.h"` dependency for socket helpers (only need it for `Stun_Discover`/`Stun_HolePunch`)
- **Change** `stun_socket_fd` (int) → `stun_socket` (`NET_DatagramSocket*`)
- **Change** `configure_gekko()`:
  ```c
  // Before:
  gekko_net_adapter_set(session, &stun_adapter);
  // After:
  gekko_net_adapter_set(session, SDLNetAdapter_Create(stun_socket));
  ```

---

### Phase 3: Migrate STUN to SDL3_Net

**Goal**: Replace raw socket I/O in `stun.c` with SDL3_Net calls while keeping STUN protocol parsing intact.

#### Changes to `stun.h`

```diff
 typedef struct {
     uint32_t public_ip;
     uint16_t public_port;
     uint16_t local_port;
-    int socket_fd;
+    NET_DatagramSocket* socket;  // SDL3_Net datagram socket
 } StunResult;

 // REMOVE these (replaced by sdl_net_adapter):
-int Stun_SocketSendTo(int fd, const char* dest_endpoint, const char* data, int length);
-int Stun_SocketRecvFrom(int fd, char* buf, int buf_size, char* from_endpoint, int endpoint_size);
-void Stun_SocketClose(int fd);
-
-// REMOVE (SDL3_Net sockets are non-blocking by default):
-void Stun_SetNonBlocking(StunResult* result);
```

#### Changes to `stun.c`

**Remove** (30+ lines):
- All `#ifdef _WIN32` / `#include <winsock2.h>` / `#include <sys/socket.h>` / etc.
- `WSAStartup()` / `WSACleanup()` calls
- `set_nonblocking()` implementations
- `Stun_SocketSendTo()`, `Stun_SocketRecvFrom()`, `Stun_SocketClose()` helper functions
- `Stun_SetNonBlocking()` function
- Windows `SIO_UDP_CONNRESET` workaround

**Migrate** `Stun_Discover()`:
```c
// Before:
struct addrinfo hints, *res = NULL;
getaddrinfo("stun.l.google.com", "19302", &hints, &res);
int sock = socket(AF_INET, SOCK_DGRAM, 0);
bind(sock, ...);
sendto(sock, request, 20, 0, res->ai_addr, res->ai_addrlen);
recvfrom(sock, response, sizeof(response), 0, NULL, NULL);

// After:
NET_DatagramSocket* sock = NET_CreateDatagramSocket(NULL, local_port);
NET_Address* stun_addr = NET_ResolveHostname("stun.l.google.com");
// Spin-wait for resolve (we're on a background thread):
while (NET_GetAddressStatus(stun_addr) == NET_WAITING) SDL_Delay(10);
NET_SendDatagram(sock, stun_addr, 19302, request, 20);
NET_Datagram* dgram = NULL;
// Poll with timeout for response:
for (int attempt = 0; attempt < 30 && !dgram; attempt++) {
    NET_ReceiveDatagram(sock, &dgram);
    if (!dgram) SDL_Delay(100);
}
```

**Migrate** `Stun_HolePunch()`:
```c
// Before:
sendto(sock, punch_msg, strlen(punch_msg), 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
recvfrom(sock, recv_buf, sizeof(recv_buf) - 1, 0, (struct sockaddr*)&from_addr, &from_len);

// After:
NET_Address* peer = NET_ResolveHostname(peer_ip_str);
NET_SendDatagram(result->socket, peer, peer_port, punch_msg, strlen(punch_msg));
NET_Datagram* dgram = NULL;
NET_ReceiveDatagram(result->socket, &dgram);
if (dgram && strcmp(dgram->buf, punch_msg) == 0) { ... }
```

**Challenge — Getting local port from `NET_DatagramSocket`:**
The STUN flow needs to know the actual OS-assigned local port (when `local_port == 0`). SDL3_Net doesn't expose `getsockname()`. Options:
1. Always pass an explicit port (avoid port 0)
2. Use the STUN response's port as the truth (which is what we ultimately use anyway)
3. Keep one raw `getsockname()` call wrapped in a small helper

**Recommendation**: Option 2 — the STUN response tells us our mapped port, which is the one that matters.

**KEEP unchanged** (~130 lines):
- `build_binding_request()` — pure byte array construction
- `parse_binding_response()` — pure byte parsing (RFC 5389)
- `Stun_EncodeEndpoint()` / `Stun_DecodeEndpoint()` — pure math
- `Stun_FormatIP()` — pure string formatting

---

### Phase 4: Migrate Discovery Listener

**Goal**: Replace the discovery listen socket and fallback broadcast with SDL3_Net. Keep per-NIC directed broadcast on raw sockets (unavoidable OS API dependency).

#### What Migrates

| Component | Can Migrate? | Why |
|-----------|-------------|-----|
| Listen socket (bind + recv) | ✅ Yes | `NET_CreateDatagramSocket(NULL, DISCOVERY_PORT)` |
| `set_nonblocking()` | ✅ Remove | SDL3_Net sockets are non-blocking by default |
| Fallback broadcast (255.255.255.255) | ✅ Yes | `NET_SendDatagram(sock, broadcast_addr, port, ...)` — SDL3_Net enables `SO_BROADCAST` |
| Per-NIC directed broadcast (Windows) | ❌ No | Requires `GetAdaptersAddresses()` for NIC enumeration |
| Per-NIC directed broadcast (Linux) | ❌ No | Requires `getifaddrs()` for NIC enumeration |
| `WSAStartup/Cleanup` | ✅ Remove | Handled by `NET_Init()`/`NET_Quit()` |

#### Strategy: Hybrid Approach

Keep the per-NIC broadcast enumeration on raw sockets (it's OS-specific NIC introspection, not generic socket I/O). Migrate everything else to SDL3_Net.

```c
// Listen socket — fully migrated:
static NET_DatagramSocket* listen_sock = NULL;
listen_sock = NET_CreateDatagramSocket(NULL, DISCOVERY_PORT);

// Broadcast — keep using raw sendto() for per-NIC directed broadcast
// but the socket itself can be SDL3_Net (it enables SO_BROADCAST):
static NET_DatagramSocket* broadcast_sock = NULL;
broadcast_sock = NET_CreateDatagramSocket(NULL, 0);

// For per-NIC directed broadcast, we still need GetAdaptersAddresses/getifaddrs
// to compute the broadcast address, then send via NET_SendDatagram.
// HOWEVER: NET_SendDatagram takes NET_Address* not sockaddr_in, and we can't
// easily create a NET_Address from a computed broadcast IP without resolving it.
//
// DECISION: Keep the broadcast socket as raw for now. Only migrate the
// listen socket and the beacon parsing/receive path.
```

**Net reduction**: ~30 lines of platform code removed (listen socket + nonblocking setup + WSAStartup).

---

### Phase 5: net_detect (No Migration)

`net_detect.c` uses `GetAdaptersAddresses()` (Windows) and `ioctl(SIOCGIWNAME)` (Linux) to detect WiFi vs Wired connections. These are OS-specific NIC introspection APIs that **no cross-platform networking library abstracts**. This file stays on raw sockets.

---

## Files Changed Summary

| File | Action | Est. Lines Changed |
|------|--------|-------------------|
| `build-deps.sh` | Add SDL3_Net build section | +50 |
| `CMakeLists.txt` | Add SDL3_Net find_package + link | +5 |
| **`src/netplay/sdl_net_adapter.h`** | **NEW** — GekkoNet ↔ SDL3_Net adapter header | +15 |
| **`src/netplay/sdl_net_adapter.c`** | **NEW** — GekkoNet ↔ SDL3_Net adapter impl | +95 |
| `src/netplay/stun.h` | Change `socket_fd` → `socket`, remove helper decls | -10 |
| `src/netplay/stun.c` | Remove raw socket code, use SDL3_Net | -150, +80 |
| `src/netplay/netplay.c` | Remove stun_adapter, use sdl_net_adapter | -70, +5 |
| `src/netplay/discovery.c` | Migrate listen socket to SDL3_Net | -30, +10 |
| `src/netplay/net_detect.c` | No change | 0 |

**Net result**: ~110 new lines (adapter module), ~260 lines removed → **~150 fewer lines** of platform-specific code.

---

## Build System Changes

### `build-deps.sh`

Add SDL3_Net section after SDL3_image (see [Phase 1](#phase-1-add-sdl3_net-dependency) for full script).

### Pi4 cross-build

The Pi4 uses pre-built libraries in `third_party_rpi4/`. SDL3_Net needs to be cross-compiled:
```bash
THIRD_PARTY=third_party_rpi4 CC=aarch64-linux-gnu-gcc CXX=aarch64-linux-gnu-g++ ./build-deps.sh
```

Or manually:
```bash
cd third_party_rpi4/sdl3_net/SDL_net
mkdir build && cd build
cmake .. \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -DCMAKE_INSTALL_PREFIX=../../build \
    -DSDL3_DIR=../../sdl3/build/lib/cmake/SDL3 \
    -DBUILD_SHARED_LIBS=ON \
    -DSDLNET_EXAMPLES=OFF
cmake --build . -j$(nproc)
cmake --install .
```

### GitHub Actions

Add SDL3_Net to the dependency build step in CI workflows. It follows the same pattern as SDL3_mixer.

### Windows (MSVC)

SDL3_Net builds with CMake on Windows. Add it to the CMake configure step:
```
-DSDL3_NET_DIR=third_party/sdl3_net/build
```

### Runtime Distribution

`libSDL3_net.so` / `SDL3_net.dll` must be shipped alongside the binary. Add to CPack/install rules.

---

## Risks & Open Questions

### 1. STUN Local Port Discovery

`Stun_Discover()` currently uses `getsockname()` to get the OS-assigned local port when `local_port == 0`. SDL3_Net doesn't expose this. **Mitigation**: Always pass an explicit port, or use the STUN response's mapped port (which is what we actually share with peers).

### 2. STUN Receive Timeout

The current `Stun_Discover()` sets `SO_RCVTIMEO` for a blocking 3-second wait. SDL3_Net sockets are always non-blocking. **Mitigation**: Poll with `NET_ReceiveDatagram()` in a loop with `SDL_Delay()` — same approach as the hole punch, and we're already on a background thread.

### 3. Hole Punch Address Matching

`Stun_HolePunch()` currently checks `from_addr.sin_addr.s_addr == *peer_ip` to verify the punch came from the right peer. With SDL3_Net, we get `NET_Address*` and `port` from the datagram. **Mitigation**: Compare via `NET_GetAddressString()` — slightly less efficient but sufficient for hole-punch rates (~5 packets/sec).

### 4. Discovery Broadcast Addresses

Per-NIC directed broadcast computation requires OS APIs not in SDL3_Net. **Decision**: Keep broadcast sending on raw sockets in a small helper; only migrate the listen path. This is the pragmatic hybrid approach.

### 5. SDL3_Net Version Stability

SDL3_Net is still in active development (matching SDL3's timeline). Pin to a specific commit or release tag to avoid API breakage.

### 6. Binary Size & Dependency

SDL3_Net adds a shared library (~100KB). Acceptable for what we get in return.

---

## Verification Plan

### Build Verification
- [x] Compile on Windows (MinGW64/MSYS2 via `compile.bat`) — ✅ 1036/1036 targets
- [ ] Compile on Linux x86_64
- [ ] Cross-compile for Raspberry Pi 4 (ARM64) — scripts updated, untested
- [x] Verify `SDL3_net.dll` is packaged in distribution — ✅ confirmed in install output

### Functional Tests

**STUN (Phase 3)**:
- [x] STUN discovery resolves public IP:port — ✅ "Discovered public endpoint (local port 51897)"
- [ ] Room codes still encode/decode correctly (untouched code)
- [ ] Hole punching works for internet matches
- [ ] Cancel during hole punch works

**GekkoNet Adapter (Phase 2)**:
- [ ] LAN match via `gekko_default_adapter` path still works
- [ ] Internet match via `SDLNetAdapter_Create(stun_socket)` works
- [ ] Spectator sessions work

**Discovery (Phase 4)**:
- [ ] LAN beacons are sent on all interfaces
- [ ] Peers appear in lobby within 500ms
- [ ] Stale peers are cleaned up after 15s
- [ ] Multi-adapter machines (WiFi + Ethernet) discover peers on both

**Regression**:
- [ ] Casual lobby match → play → rematch cycle
- [ ] Native lobby invite → accept/decline
- [ ] Cancel outgoing challenge → re-challenge
- [ ] Match reporting works (winner/loser/disconnect)
