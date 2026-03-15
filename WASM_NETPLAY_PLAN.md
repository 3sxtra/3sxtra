# WASM Browser Build with WebRTC Netplay — Implementation Plan

**Goal**: Run 3SXtra in a browser via Emscripten with full RmlUi menus and rollback netplay over WebRTC DataChannels.

**Reference**: [HeatXD's `webstrike` branch](https://github.com/crowded-street/3sx/compare/main...HeatXD:3sx:webstrike) (upstream 3SX — proves the approach but our fork has diverged significantly)

---

## Phase 0: Function Signature Fixes ✅ DONE

K&R empty-param-list stubs fixed in `effa2`, `game`, `pulpul`, `sound3rd`, and `native_save` (9 files).

---

## Phase 1: WASM Build System & Core Port

Get the game compiling and running in a browser with full RmlUi.

### 1.1 CMakeLists.txt — Emscripten Target

**Current state**: 895 lines, no `EMSCRIPTEN` guards. Links `glad_gl_core` (OpenGL 4.6), `GekkoNet`, `librashader`, `rmlui`+`lua`, `SDL3_net`, `SDL3_image`, `SDL3_mixer`, `curl`, `ZLIB`, plus platform-specific libs (dbghelp/winsock/bcrypt on Win, pipewire on Linux, Syphon on macOS).

**Changes needed** (`if(EMSCRIPTEN)` block):

| Item | Detail |
|---|---|
| OpenGL | Switch glad from GL 4.6 Core → GL ES 3.0 (WebGL 2). Our shaders use `#version 460 core` — need GLSL ES 300 equivalents or transpilation via SDL_shadercross |
| SDL3 ports | Use Emscripten ports: `-sUSE_SDL=3` (or build from source). SDL3's Emscripten backend handles canvas, audio, gamepad |
| ZLIB | `-sUSE_ZLIB=1` (Emscripten port) |
| librashader | **Exclude entirely** — Rust crate, not buildable for WASM. Shader chain features disabled in browser. Guard with `#ifndef __EMSCRIPTEN__` |
| Spout/PipeWire/Syphon | Already platform-guarded — not an issue |
| SPIR-V shaders | Skip `glslc`/`glslangValidator` compilation step; use GLSL ES directly |
| Link flags | `-sWASM=1 -sALLOW_MEMORY_GROWTH=1 -sASYNCIFY` (for blocking SDL calls), `--preload-file assets@/assets` |
| `build-deps.sh` | Guard with `if(NOT EMSCRIPTEN)` — dependencies built separately via `build-deps-wasm.sh` |
| Tracy | Disable (`ENABLE_TRACY` OFF) |
| Tests/CMocka | Disable (`ENABLE_TESTS` OFF) |

### 1.2 OpenGL ES 3.0 / WebGL 2 Compatibility

**Current state**: `sdl_app.c` requests OpenGL 4.6 Core via `SDL_GL_SetAttribute`, loads functions via `glad`. Shaders at `src/shaders/` use `#version 460 core`. The game renderer (`sdl_game_renderer_gl_*.c`) uses GL 4.6 features.

**Changes needed**:
- For WASM: request GL ES 3.0 (`SDL_GL_CONTEXT_PROFILE_ES`, version 3.0)
- Replace `glad` with Emscripten's built-in WebGL2 headers (`#include <GLES3/gl3.h>`)
- Port core shaders (`scene.vert/frag`, `passthru.frag`, `blit.vert`) to `#version 300 es` (or use `SDL_shadercross` if feasible)
- `glBindVertexArray`, `glGenBuffers`, `glTexImage2D` etc. are in WebGL2 — most GL code should port
- **Texture arrays** (`scene_array.frag` uses `sampler2DArray`) — WebGL2 supports `GL_TEXTURE_2D_ARRAY`, should work
- **Compute shaders** (`palette_convert.gpu.comp`) — NOT available in WebGL2. Need CPU fallback or fragment shader equivalent

> [!WARNING]
> The RmlUi GL3 renderer (`RmlUi_Renderer_GL3.cpp`) already has Emscripten support via `RMLUI_PLATFORM_EMSCRIPTEN` — it downgrades to GL ES 3.0 automatically. Our custom glad loader (`RMLUI_GL3_CUSTOM_LOADER=<glad/gl.h>`) needs to be removed for WASM and replaced with Emscripten's native GL headers.

### 1.3 Main Loop Refactor

**Current state** (`main.c`, 749 lines): Standard `while(is_running)` loop with decoupled rendering (accumulator-based). No phase enum. The loop calls `SDLApp_BeginFrame()` → `step_0()` → `SDLApp_EndFrame()` → `SDLApp_PollEvents()` → `step_1()`.

**Changes needed**:
- Emscripten requires `emscripten_set_main_loop()` (or `emscripten_request_animation_frame()`) — blocking `while` loops freeze the browser
- Extract a `main_iteration()` function containing one cycle of the existing loop body
- Gate: `#ifdef __EMSCRIPTEN__` → `emscripten_set_main_loop(main_iteration, 0, 1)` instead of the while loop
- The decoupled rendering mode (accumulator) remains unchanged — just move one iteration into a callback
- Remove `AllocConsole`/`AttachConsole` and `signal()` handlers in WASM path

### 1.4 Resource / AFS Loading

**Current state** (`resources.c`, 168 lines): Uses `SDL_ShowSimpleMessageBox` + `SDL_ShowOpenFolderDialog` to locate ROMs. AFS (`afs.c`, 489 lines): Opens `SF33RD.AFS` via `SDL_IOFromFile`, preloads non-BGM entries to RAM in a background thread, BGM streamed via `SDL_AsyncIO`.

**Changes needed**:
- **Browser resource flow**: Drag-and-drop the `.AFS` file onto the page. JavaScript writes it to Emscripten's MEMFS (in-memory filesystem). A new `Resources_OnAFSDropped()` C function signals availability.
- `Resources_CheckIfPresent()`: On WASM, check if `/afs/SF33RD.AFS` exists in MEMFS
- `Resources_RunResourceCopyingFlow()`: On WASM, no-op (drag-drop handles it)
- `AFS_Init()`: Works as-is once the file is in MEMFS — `SDL_IOFromFile` reads from MEMFS
- `SDL_AsyncIO`: Verify Emscripten support. If not available, fall back to synchronous reads (MEMFS is in-memory so reads are instant anyway)
- New files: `web/pre.js` (drag-drop overlay JavaScript), `web/shell.html` (custom HTML template)

### 1.5 Port Layer Guards

**Actual issues found** (not guessed):

| File | Issue | Fix |
|---|---|---|
| `utils.c:19` | `#include <execinfo.h>` with `backtrace()` — doesn't exist in Emscripten | Add `#elif defined(__EMSCRIPTEN__)` → use `emscripten_get_callstack()` or just `fprintf + abort()` |
| `utils.c:41-46` | `void* buffer[]` + `backtrace()` calls | Guard with `#if !defined(__EMSCRIPTEN__)` |
| `lobby_server.c:51-58` | BSD socket includes (`<arpa/inet.h>`, `<sys/socket.h>`, etc.) for SSE client | Emscripten provides POSIX socket stubs but SSE needs rework — see §2.4 |
| `sdl_app.c:86` | `#include <glad/gl.h>` | On WASM: `#include <GLES3/gl3.h>` instead |
| `sdl_app.c:94` | `#include "shaders/librashader_manager.h"` | Guard with `#ifndef __EMSCRIPTEN__` |
| `sdl_app.c:266-268` | `SDL_GL_CONTEXT_MAJOR_VERSION = 4, MINOR = 6` | WASM: set to ES 3.0 |
| `main.c:89-91` | `#include <windef.h>` / `<ConsoleApi.h>` | Already Win32-guarded ✅ |
| `main.c:69-75` | `signal(SIGINT/SIGTERM)` | WASM: skip (no signals) |
| `stun.c:22-28` | `<winsock2.h>` / `<arpa/inet.h>` for `inet_ntop` | Not needed on WASM (STUN replaced by WebRTC ICE) |

### 1.6 Libraries Not Available on WASM

| Library | Used By | Solution |
|---|---|---|
| **librashader** | Shader chain (CRT filters etc.) | Exclude on WASM. Disable shader menu UI. Core game rendering unaffected |
| **libcurl** | `lobby_server.c` HTTP client | Replace with Emscripten's `emscripten_fetch()` or JS `fetch()` via `EM_ASM` |
| **miniupnpc** | UPnP port forwarding | Not needed (WebRTC handles NAT traversal via ICE) |
| **dbghelp** | Windows stack traces | Already Win32-guarded ✅ |
| **Spout2/PipeWire/Syphon** | Video broadcast | Already platform-guarded ✅ |

### 1.7 New Files

| File | Purpose |
|---|---|
| `web/shell.html` | Custom Emscripten HTML template — black background, canvas, drag-drop zone |
| `web/pre.js` | JavaScript: drag-drop AFS handling, MEMFS write, calls `_Resources_OnAFSDropped()` |
| `build-deps-wasm.sh` | Build SDL3, SDL3_net, SDL3_image, SDL3_mixer, freetype, rmlui for WASM via `emcmake`/`emmake` |

---

## Phase 2: WebRTC Netplay (browser ↔ browser + browser ↔ native)

Replace the UDP `GekkoNetAdapter` with a WebRTC DataChannel adapter.

### 2.1 Architecture

```
┌─────────── Browser or Native ────────────┐     ┌─────────── Browser or Native ────────────┐
│ GekkoNet Session                         │     │ GekkoNet Session                         │
│   ↕ GekkoNetAdapter function pointers    │     │   ↕ GekkoNetAdapter function pointers    │
│ WebRTCAdapter (C)                        │     │ WebRTCAdapter (C)                        │
│   ↕                                      │     │   ↕                                      │
│ Browser: EM_ASM → JS RTCDataChannel      │     │ Browser: EM_ASM → JS RTCDataChannel      │
│ Native:  libdatachannel C API            │     │ Native:  libdatachannel C API            │
└──────────────────────────────────────────┘     └──────────────────────────────────────────┘
               ↕  DataChannel (unreliable, unordered)  ↕
        ┌─────────── Lobby Server ───────────┐
        │  Existing SSE + HTTP infra         │
        │  + New signaling endpoints:        │
        │    POST /signal/offer              │
        │    POST /signal/answer             │
        │    POST /signal/ice                │
        │  SSE delivers signals to peer      │
        └────────────────────────────────────┘
```

### 2.2 Why libdatachannel for Native

We only need unreliable DataChannels to carry GekkoNet packets (~50-200 bytes/frame). **libdatachannel** is the right choice:

- **Small**: ~100KB static lib vs. libwebrtc's 50+ MB (which includes video/audio codecs we'd never use)
- **Clean C API**: `rtcCreatePeerConnection()`, `rtcCreateDataChannel()`, `rtcSetRemoteDescription()` — maps directly to our adapter
- **CMake**: Easy to add as a subdirectory alongside GekkoNet
- **License**: MPL-2.0
- **DataChannel config**: `{ .unordered = true, .maxRetransmits = 0 }` gives us UDP-like semantics

This enables **browser ↔ native** netplay from day one — both sides speak WebRTC.

### 2.3 Browser-Side: `web/webrtc_adapter.js`

| Function | Purpose |
|---|---|
| `webrtc_init(stun_urls)` | Create `RTCPeerConnection` with STUN config |
| `webrtc_create_offer()` → returns SDP | For the initiating peer |
| `webrtc_set_remote_desc(sdp)` | For both peers |
| `webrtc_add_ice(candidate)` | ICE trickle |
| `webrtc_send(data, len)` | `dataChannel.send()` |
| `webrtc_poll()` → queued packets | Read received packets |
| `webrtc_close()` | Cleanup |

DataChannel config: `{ ordered: false, maxRetransmits: 0 }` (UDP-like).

### 2.4 C-Side: `src/netplay/webrtc_adapter.c`

Implements `GekkoNetAdapter`:

```c
#ifdef __EMSCRIPTEN__
  // Uses EM_ASM/EM_JS to call webrtc_adapter.js functions
#else
  // Uses libdatachannel C API (rtcCreatePeerConnection, etc.)
#endif

GekkoNetAdapter* WebRTCAdapter_Create(void);
void WebRTCAdapter_Destroy(void);
```

### 2.5 Integration: `netplay.c` `configure_gekko()`

**Current code** (line 363-374):
```c
if (stun_socket != NULL) {
    gekko_net_adapter_set(session, SDLNetAdapter_Create(stun_socket));
} else {
    gekko_net_adapter_set(session, gekko_default_adapter(local_port));
}
```

**With WebRTC** (when using lobby server / internet play):
```c
#if defined(USE_WEBRTC)
    gekko_net_adapter_set(session, WebRTCAdapter_Create());
#else
    // existing UDP adapter code (kept for LAN play on native)
#endif
```

On WASM, `USE_WEBRTC` is always defined. On native, it's used for internet play via the lobby server (LAN discovery still uses raw UDP via the existing adapter).

### 2.6 Lobby Server: Signaling Endpoints

**Current state**: The lobby server (`lobby_server.c`, 1429 lines) uses `libcurl` for HTTP requests and raw BSD sockets for SSE in a background thread.

**New endpoints needed on the Node.js server**:

| Endpoint | Direction | Purpose |
|---|---|---|
| `POST /signal/offer` | A → server  | Peer A submits SDP offer (JSON body with `player_id`, `target_id`, `sdp`) |
| `POST /signal/answer` | B → server | Peer B submits SDP answer |
| `POST /signal/ice` | Both → server | ICE candidate trickle |

The server relays these to the target peer via the existing SSE channel. New SSE event types: `signal_offer`, `signal_answer`, `signal_ice`.

**Client-side changes** (`lobby_server.c`):
- Add `LobbyServer_SendOffer()`, `LobbyServer_SendAnswer()`, `LobbyServer_SendICE()`
- Parse new SSE event types in `sse_parse_event()`
- On WASM: Replace `libcurl` HTTP with `emscripten_fetch()` (async HTTP via browser's fetch API)
- On WASM: Replace raw-socket SSE thread with `EventSource` API via JavaScript (browser-native SSE, no threads needed)

### 2.7 STUN / NAT Traversal

**Current state**: `stun.c` (390 lines) does manual STUN binding requests via `NET_DatagramSocket` to discover public IP:port, then hole-punches by exchanging UDP packets.

**With WebRTC**: ICE candidates handle STUN/TURN automatically. The browser's `RTCPeerConnection` uses the configured STUN servers (`stun.l.google.com:19302`) internally. On native, `libdatachannel` does the same.

**STUN config**: Use public Google STUN servers initially. Self-hosted TURN can be added later for symmetric NAT traversal.

The existing `stun.c` code stays untouched for native LAN play (where direct UDP is preferred).

---

## Phase 3: Polish & CI

- [ ] GitHub Actions workflow for WASM build
- [ ] Gamepad support verification (SDL3 Emscripten gamepad backend)
- [ ] Audio verification (SDL3 WebAudio backend + ADX decoding)
- [ ] Compute shader fallback (palette conversion — CPU or fragment shader)
- [ ] GitHub Pages deployment
- [ ] Mobile browser touch controls (stretch goal)

---

## Dependency Summary

| Dependency | Native | WASM |
|---|---|---|
| SDL3 | Shared lib | Emscripten port or static build |
| SDL3_net | Shared lib | Static build via `emcmake` |
| SDL3_image | Shared lib | Static build via `emcmake` |
| SDL3_mixer | Shared lib | Static build via `emcmake` |
| GekkoNet | Static lib | Static build via `emcmake` (pure C++) |
| RmlUi | Static lib | Static build via `emcmake` (has Emscripten support) |
| FreeType | Static lib | Static build via `emcmake` |
| Lua 5.4 | Static lib | Static build via `emcmake` |
| ZLIB | System/static | Emscripten port (`-sUSE_ZLIB=1`) |
| glad | GL 4.6 loader | **Not used** — Emscripten provides GL ES 3.0 headers |
| librashader | Static lib (Rust) | **Excluded** — not buildable for WASM |
| libcurl | Shared/static | **Replaced** by `emscripten_fetch()` |
| libdatachannel | Static lib (new) | **Not used** — browser has native WebRTC |
| cJSON | Vendored source | Works as-is ✅ |
| SHA-256 | Portable C | Works as-is ✅ |

---

## Risk Assessment

| Risk | Likelihood | Mitigation |
|---|---|---|
| GL ES 3.0 shader porting | Medium | RPi4 already uses GL 3.3 — many compat paths exist. Start with scene shaders |
| SDL3 Emscripten maturity | Low | SDL3 has official Emscripten support and CI |
| Compute shader (palette_convert) | High | Must implement fragment shader fallback — WebGL2 has no compute |
| `SDL_AsyncIO` on Emscripten | Medium | MEMFS makes all I/O synchronous anyway. Can ifdef to sync reads |
| WebRTC DataChannel reliability | Low | Proven technology, well-documented APIs |
| libdatachannel build on all platforms | Low | CMake-based, CI-tested, works on Windows/Linux/macOS |
| Binary size | Medium | WASM with RmlUi+Lua could be 15-30 MB. Use `-Oz`, `wasm-opt` |
