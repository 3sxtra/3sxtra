# 3SXtra

[![Windows](https://github.com/3sxtra/3sxtra/actions/workflows/build_windows.yml/badge.svg)](https://github.com/3sxtra/3sxtra/actions/workflows/build_windows.yml)
[![Linux](https://github.com/3sxtra/3sxtra/actions/workflows/build_linux.yml/badge.svg)](https://github.com/3sxtra/3sxtra/actions/workflows/build_linux.yml)
[![Linux ARM64](https://github.com/3sxtra/3sxtra/actions/workflows/build_linux_arm64.yml/badge.svg)](https://github.com/3sxtra/3sxtra/actions/workflows/build_linux_arm64.yml)
[![macOS](https://github.com/3sxtra/3sxtra/actions/workflows/build_macos.yml/badge.svg)](https://github.com/3sxtra/3sxtra/actions/workflows/build_macos.yml)
[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](LICENSE)


> [!NOTE]
> Experimental, unofficial fork. macOS and mainline Linux are lightly tested. Raspberry Pi 4 / Batocera is the primary Linux target.

Binary: `3sx` (`3sx.exe` on Windows).

---

## Quick Start

1. Place your legally obtained `.afs` file extracted from your ISO of SF3.3 in the `rom/` directory. Alternatively, the game will use the default AFS path from upstream.
2. Run `3sx.exe` (Windows) or `./3sx` (Linux / macOS).
3. *Optional*: Create an empty `config/` folder next to the executable for portable mode.

---

## Rendering

| Backend | API | Notes |
|---|---|---|
| **OpenGL 3.3+** | GLSL | Texture array batching, PBO async uploads, compute-shader palette conversion |
| **SDL_GPU** | Vulkan / Metal / DX12 | Via SDL3's `SDL_GPU` API |
| **SDL2D** | SDL3 2D | Software fallback |

Select with `--renderer gl`, `--renderer gpu`, or `--renderer sdl`.

### Visuals & Mods Menu

Press **F3** to access the in-game **Mods Menu** which centralizes various visual and quality-of-life toggles at runtime:

- **Shaders (librashader)**: Load & hot-swap RetroArch `.slangp` presets via the dedicated shader picker (**F2**), or bypass them entirely.
- **Bezels**: 40+ per-character arcade bezels. Auto-swaps on character change and resets for menus.
- **HD Stage Backgrounds**: Per-stage multi-layer parallax backgrounds rendered at output resolution using the 22-stage override system.
- **Sprite Overrides**: HD sprite replacement support hooked into the rendering pipeline.
- **Audio Mods**: Toggle custom BGM & voice replacement packs.
- **Fast Pre-Game**: Skip or speed up the standard arcade intro screens.

---

## Controls & Hotkeys

| Key | Function |
|---|---|
| **F1** | Main menu (input mapping, options, save/load) |
| **F2** | Shader picker |
| **F3** | Mods menu (HD backgrounds, visual mods) |
| **F4** | Cycle shader mode |
| **F5** | Toggle frame-rate uncap |
| **F6** | Stage config |
| **F7** | Training options |
| **F8** | Cycle scale mode |
| **F9** | Cycle shader preset |
| **F10** | Diagnostics (FPS, netplay stats) |
| **F11** | Toggle fullscreen |
| **F12** | Input-lag test |
| **Alt+Enter** | Toggle fullscreen |
| **` (Grave)** | Screenshot |
| **9** | Debug pause / frame-step |
| **0** | Debug overlay (72 options) |

---

## Audio

FFmpeg **removed**. Built-in ADX decoder, zero external audio dependencies.

Master volume: `--volume 0–100`.

Custom audio mods: drop files in `assets/bgm_mod/` (music) or `assets/voice_mod/` (voices).

---

## Save System & Replays

Native save system replaces PS2 memory card emulation:

- `options.ini` — settings and controls
- `direction.ini` — system direction
- `replays/` — string-based binary replay files with `.meta` sidecars
- Atomic writes (crash-safe)
- Auto-saving for netplay matches with auto-upload to the lobby server
- In-game local and online replay browsers

Files go to your user profile, or `config/` in portable mode.

---

## Netplay

![Network Lobby](docs/images/network_lobby.gif)

Built on GekkoNet GGPO rollback netcode.

| Feature | Details |
|---|---|
| **STUN hole-punching** | Discovers public endpoint, punches through NAT |
| **UPnP fallback** | Auto-opens UDP port on compatible routers |
| **Lobby server** | Node.js, zero deps, HMAC-SHA256 auth, Glicko-2 ratings |
| **In-game lobby** | Native and RmlUi lobby screens, Casual and KOTH queues |
| **Tournaments** | Full bracket support (Single/Double Elim, Swiss, Round Robin) with parallel matches and TO controls |
| **Private Rooms** | Password-protected and hidden rooms with seamless QR code joining |
| **Spectating** | Up to 4 live spectators with network lobby integration |
| **Async comms** | HTTP lobby traffic on background thread |
| **LAN support** | Dedicated LAN lobby with local IP display |
| **Matchmaking** | Filter by region, ping, or Wi-Fi connection quality for lower latency |
| **Client ID** | Stable fingerprint prevents username spoofing |
| **Desync prevention** | Frame 0 reset, 17 expanded rollback fields, pointer-safe checksums |
| **Sync test** | Automated sync-test with Python runner |

### The Network Gateway

The in-game **Network** menu serves as your hub for all online features:

- **Lobby Browser**: Browse available Casual, KOTH, and Tournament rooms. See ping, connection type (wired/Wi-Fi), and region flags before joining.
- **Create Room**: Host public, password-protected, or hidden rooms. Generate shareable QR codes, or switch to Tournament type to automatically manage a bracket (Single/Double Elim, Swiss, Round Robin).
- **Leaderboards**: View the global ranking table utilizing the Glicko-2 rating system. Check player tiers, ranks, grades, and most-played characters.
- **Online Replays**: Browse, download, and seamlessly play back recent matches played on the server directly within the game.

Start from the in-game **Network** menu or via CLI shorthand: `3sx 1 192.168.1.100`

---

## Performance

![VSync & Turbo Mode](docs/images/vsync_turbo.gif)

All fork-only optimizations:

| Optimization | Details |
|---|---|
| **SIMDe vectorization** | SSE2/NEON for palette LUT conversion |
| **Texture array batching** | `GL_TEXTURE_2D_ARRAY` single-bind rendering |
| **Persistent mapped buffers** | Triple-buffered VBOs, no per-frame stalls |
| **PBO async uploads** | Overlaps CPU conversion with GPU upload |
| **GPU palette compute** | Compute shader palette lookup |
| **Active voice bitmask** | Skips silent audio channels |
| **RAM asset preload** | All assets in memory at startup |
| **Hybrid frame limiter** | Smooth pacing on RPi (compensates kernel jitter) |
| **LTO + PGO** | Link-Time and Profile-Guided Optimization |

---

## Platform Support

| Platform | Status |
|---|---|
| **Windows** (x86-64) | Primary dev platform |
| **Raspberry Pi 4 / Batocera** | Full cross-compilation + integration |
| **Linux x86-64** | Tested |
| **Linux ARM64** | Native support |
| **macOS** (Intel + Apple Silicon) | Builds, not actively tested |
| **Flatpak** | Packaging defined, not actively tested |

### Portable Mode
Create `config/` next to the executable. All saves, replays, and settings stay local.

### Video Broadcasting
- **Windows** — Spout2
- **macOS** — Syphon
- **Linux** — PipeWire *(WIP)*

---

## CLI Options

```
Usage: 3sx [options] [player_side remote_ip]

  --renderer <backend>       gl, gpu, sdl, or classic (default: gl)
  --volume 0-100             Master volume (default: 100)
  --scale <factor>           Resolution multiplier (default: 1)
  --port <number>            Netplay UDP port (default: 50000)
  --window-pos <x>,<y>       Window position
  --window-size <w>x<h>      Window size
  --ui <rmlui>               UI toolkit for overlay menus
  --enable-broadcast         Enable Spout/Syphon/PipeWire output
  --shm-suffix <suffix>      Shared-memory name for broadcast
  --font-test                Boot into font debug screen
  --help                     Show help

Netplay shorthand:
  3sx 1 192.168.1.100        Connect as P1
  3sx 2 192.168.1.100        Connect as P2
```

---

## Building

See [Build Guide](docs/building.md) for full instructions.


### Project Structure

```text
3sxtra/
├── src/
│   ├── sf33rd/Source/Game/   # Core game engine (C)
│   └── port/sdl/            # SDL3 port layer (render, input, net, rmlui)
├── assets/                  # Bezels, stages, UI, audio mods
├── shaders/                 # GLSL / SPIR-V
├── tests/                   # CMocka unit tests
├── tools/                   # Build scripts, sync-test, Batocera tooling
├── deploy/                  # Deployment layout
├── docs/                    # Build guide
├── third_party/             # Vendored dependencies
├── .github/workflows/       # CI pipelines
└── CMakeLists.txt
```

---

## Dependencies

**Added:** GLAD, SIMDe, stb_image, librashader, SDL_shadercross, RmlUi, Dear ImGui, CMocka, Tracy, Spout2

**Removed:** FFmpeg — replaced by built-in ADX decoder

SDL3 tracks `main` branch (upstream pins a release tarball).

---

## Licenses

See [`THIRD_PARTY_NOTICES.txt`](THIRD_PARTY_NOTICES.txt) for full texts.

| Library | License |
|---|---|
| [GekkoNet](https://github.com/HeatXD/GekkoNet) | MIT |
| [SDL3](https://github.com/libsdl-org/SDL) | zlib |
| [SDL\_shadercross](https://github.com/libsdl-org/SDL_shadercross) | zlib |
| [RmlUi](https://github.com/mikke89/RmlUi) | MIT |
| [Dear ImGui](https://github.com/ocornut/imgui) | MIT |
| [librashader](https://github.com/SnowflakePowered/librashader) | MPL-2.0 |
| [GLAD](https://github.com/Dav1dde/glad) | MIT |
| [SIMDe](https://github.com/simd-everywhere/simde) | MIT |
| [stb\_image](https://github.com/nothings/stb) | Public Domain / MIT |
| [Spout2](https://github.com/leadedge/Spout2) | BSD 2-Clause |
| [Tracy](https://github.com/wolfpld/tracy) | BSD 3-Clause |
| [CMocka](https://cmocka.org) | Apache 2.0 |
| [zlib](https://zlib.net) | zlib |
| [libcdio](https://github.com/libcdio/libcdio) | GPLv3+ |
