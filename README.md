# 3SXtra

> A experimental, unsupported, unofficial fork of 3sx, a decomp of **Street Fighter III: 3rd Strike** for modern platforms — enhanced and tailored to my needs and preferences.

Most things could not be tested on all platforms, in particular MacOS and Linux. 
Linux builds are tested on a Raspberry Pi 4, running Batocera.
Flatpak builds are not yet tested. All workflows are not yet fully tested and may need updating for this repo.

Most things should work, but might need some tweaks here and there.
The training mode and Trials mode are WIP and not fully implemented.

## Quick Start

1. **Get the ISO**: You must provide your own legally obtained `.afs`  . Place it in the `rom` directory next to the executable.
2. **Launch**: Run `3sx.exe` (Windows) or `./3sx` (Linux/macOS).
3. **Controls**: Press **F1** anywhere to open the main menu and configure your controls. P1 defaults layout is WASD for movement and JKIUOP for buttons.
4. **Portable Mode**: (Optional) Create an empty folder named `config/` next to the executable to redirect all saves, replays, and settings there instead of your user profile.

---

## Table of Contents

- [What's New in This Fork](#whats-new-in-this-fork)
- [Graphics & Visuals](#graphics--visuals)
- [Menus & Interface](#menus--interface)
- [Audio](#audio)
- [Training Mode](#training-mode)
- [Save System](#save-system)
- [Netplay](#netplay)
- [Performance](#performance)
- [Platform Support](#platform-support)
- [CLI Options](#cli-options)
- [Developer & Building](#developer--building)

---

## What's New in This Fork

This fork adds a large set of features, performance improvements, and platform support on top of the upstream build.

### Rendering & Graphics

| Feature | Details |
|---|---|
| **OpenGL 3.3+ backend** | Fully custom GPU-accelerated pipeline — replaces upstream's SDL2D renderer entirely |
| **SDL\_GPU backend** | Second backend using SDL3's `SDL_GPU` API (Vulkan / Metal / DirectX 12) |
| **RetroArch shaders** | Load any `.slangp` preset at runtime (CRT, ScaleFX, xBR, …); hot-swap with **F2** |
| **GPU palette compute** | Indexed-color palette lookup offloaded to GPU via compute shaders |
| **Arcade bezels** | 40+ per-character bezels that swap with the characters and reset on menus/title |
| **HD stage backgrounds** | Per-stage modded multi-layer parallax backgrounds rendered behind the game sprites at full output resolution; toggleable via **F3** |
| **Resolution scaling** | User-configurable output resolution; integer scaling and aspect ratio modes |
| **Texture array batching** | Textures packed into `GL_TEXTURE_2D_ARRAY` — far fewer draw calls per frame |

### Menus & Interface

| Feature | Details |
|---|---|
| **ImGui overlay system** | F-key overlay menus accessible any time without interrupting gameplay |
| **Main menu (F1)** | Central hub: input mapping, options, save/load, replay picker |
| **Shader picker (F2)** | Browse and hot-apply RetroArch preset files |
| **Mods menu (F3)** | Toggle HD backgrounds and other visual mods |
| **Stage config (F6)** | Per-stage visual settings |
| **Training options (F7)** | Dummy AI, hitbox overlay, frame data, input display — all configurable in-game |
| **Diagnostics (F10)** | Live FPS counter, netplay metrics, frame timing |
| **Controller icons** | Correct button glyphs for PS3/4/5, Xbox 360/One/Series, Switch Pro, Steam Deck, and keyboard |
| **Toast notifications** | Non-intrusive pop-ups for connection status, save confirms, and other events |
| **Responsive scaling** | All menus, icons, and text scale automatically with window size |
| **Debug overlay (0)** | 72-option debug configuration panel |
| **Debug pause (9)** | Pause execution and frame-step through gameplay |

### Input & Controls

| Feature | Details |
|---|---|
| **Control remapping** | Full button rebinding with persistent storage (`options.ini`) |
| **Generic joystick support** | Any gamepad works; device auto-detected |
| **Keyboard player 1 layout** | WASD movement, JKIUOP buttons — fully remappable |
| **Wait-for-release mapping** | Bindings only activate after the old key is fully released |

### Audio

| Feature | Details |
|---|---|
| **Custom ADX decoder** | FFmpeg fully removed; built-in decoder with zero external library requirements |
| **Master volume control** | Single slider scales BGM and SFX together; settable via `--volume` CLI flag |

### Training Mode

| Feature | Details |
|---|---|
| **Frame meter** | Color-coded startup / active / recovery / hitstun / blockstun bar, plus frame-advantage readout |
| **Hitbox display** | Hurtboxes 🟢, attackboxes 🔴, throwboxes 🟡, pushboxes 🔵 — individually toggleable |
| **Input display** | On-screen stick + button history for P1 and P2 with frame durations |
| **Stun meter** | Live stun accumulation readout during combos |
| **Dummy AI** | Block mode, parry (incl. red parry), stun mash, wakeup mash, wakeup reversal |
| **Combo trials** | Step-by-step built-in challenges per character, tracked natively by the engine _(WIP)_ |

### Save System & Replays

| Feature | Details |
|---|---|
| **Native save system** | Replaces PS2 memory card emulation — saves to regular files (`options.ini`, `direction.ini`) |
| **Atomic writes** | Crash mid-save cannot corrupt existing data |
| **Replay system** | Binary replay files with metadata sidecars stored in `replays/` |
| **20-slot replay picker** | Visual picker showing date, characters, and slot status for saving/loading |
| **Portable mode** | Drop a `config/` folder next to the executable to redirect all saves there |

### Netplay

| Feature | Details |
|---|---|
| **STUN NAT hole-punching** | Client discovers public endpoint via STUN, then hands the pre-punched UDP socket to GekkoNet to preserve the NAT mapping |
| **UPnP port mapping** | Automatically opens the required UDP port on compatible routers |
| **Lobby matchmaking server** | Lightweight Node.js server (zero deps): player registration, searching, HMAC-SHA256 auth |
| **Native in-game lobby UI** | Full lobby screen inside the game — accessible from the main menu, no CLI flags required |
| **Async lobby comms** | All HTTP lobby traffic runs on a background thread; never blocks the game loop |
| **Internet lobby display** | Shows player name / room code instead of raw IP addresses |
| **LAN lobby display** | Shows player LAN IP for local network sessions |
| **Pending invite indicator** | Visual cue shown to the receiving player when a connection request is incoming |
| **Region filtering** | Filter the lobby by region for lower-latency matches |
| **Client ID fingerprinting** | Stable `client_id` in `config.ini` used as lobby identity — prevents username spoofing |
| **Desync prevention** | Frame 0 state reset, `WORK_Other_CONN` sanitization, 17 expanded rollback fields, pointer-safe checksums |
| **Sync test mode** | Parameterized automated sync-test with Python test runner |

### Performance

| Feature | Details |
|---|---|
| **PBO async texture uploads** | Overlaps CPU palette conversion with GPU upload — hides transfer latency |
| **Persistent mapped VBOs** | Triple-buffered vertex buffers eliminate per-frame `glBufferSubData` stalls |
| **SIMDe vectorization** | SSE2 on x86, NEON on ARM for 4-bit palette LUT conversion |
| **Active voice bitmask** | 64-bit bitmask with bit-scan iteration skips all silent audio channels |
| **RAM asset preload** | All game assets loaded into memory at startup — faster stage changes, less disk stutter |
| **Hybrid frame limiter** | Compensates for kernel timer jitter on Raspberry Pi |
| **LTO + PGO** | Link-Time and Profile-Guided Optimization enabled for Release builds |

### Platform

| Feature | Details |
|---|---|
| **Raspberry Pi 4 / Batocera** | Full cross-compilation toolchain and Batocera Buildroot integration |
| **Flatpak (Linux)** | `.desktop` entry, `.metainfo.xml`, proper Linux desktop packaging |
| **ARM64** | Native ARM64 — Apple Silicon Macs and ARM Linux devices |
| **Spout2 (Windows)** | Send game video to OBS or any Spout-compatible app without screen capture |
| **Syphon (macOS)** | Zero-overhead video broadcast on macOS |
| **PipeWire (Linux)** | Zero-overhead video broadcast on Linux _(WIP)_ |

---

## Graphics & Visuals

### Two Modern Rendering Backends
- **OpenGL 3.3+** — fully custom GPU-accelerated pipeline with GLSL shaders, texture array batching, PBO async uploads, and GPU palette conversion via compute shaders.
- **SDL_GPU (Vulkan / Metal / DirectX 12)** — a second backend using SDL3's `SDL_GPU` API, supporting the latest graphics APIs on Windows, macOS, and Linux.

Select the backend with `--renderer gl` or `--renderer gpu` on the command line.

### RetroArch Shader Support (librashader)
- Load any RetroArch `.slangp` preset at runtime — CRT scanlines, ScaleFX, xBR, and hundreds more.
- Hot-swap shaders from the in-game shader picker (**F2**).

### Arcade Bezels
- 40+ high-quality per-character bezels surround the viewport, just like a real arcade cabinet.
- Bezels swap automatically when characters change and reset to defaults on menus and title screen.

### HD Stage Backgrounds
- Per-stage modded multi-layer parallax backgrounds rendered at full output resolution, composited behind the game sprites.
- Assets live in `assets/stages/stage_XX/` — any stage that has assets present will use them automatically.
- Toggle on/off from the Mods menu (**F3**); falls back to the original game backgrounds when disabled or assets are absent.

### Resolution Scaling
- User-configurable output resolution — play at native monitor resolution or scale up for sharper visuals.

---

## Menus & Interface

Open overlay menus with F-keys at any time:

| Key | Menu |
|---|---|
| **F1** | ⚙️ Main Menu — input mapping, options, save/load, and more |
| **F2** | 🎨 Shader Picker — browse and apply RetroArch presets |
| **F3** | 🎭 Mods Menu — toggle HD backgrounds and visual mods |
| **F6** | 🖼️ Stage Config — per-stage visual settings |
| **F7** | 🥋 Training Options — dummy AI, hitboxes, frame data |
| **F10** | 📊 Diagnostics — FPS counter and netplay stats |

### Additional Hotkeys

| Key | Action |
|---|---|
| **F4** | Cycle shader mode (rendering pipeline) |
| **F5** | Toggle frame-rate uncap |
| **F8** | Cycle scale mode (aspect ratio / scaling) |
| **F9** | Cycle shader preset |
| **F11** | Toggle fullscreen |
| **F12** | Input-lag test (Bolt diagnostic) |
| **Alt+Enter** | Toggle fullscreen (alternative) |
| **` (Grave)** | Save screenshot |
| **9** | Debug pause / frame-step |
| **0** | Toggle 72-option debug overlay |

> F1, F2, and F3 are reserved and excluded from keyboard-to-gamepad mapping so they always work as overlay toggles.

**Controller icons** — menus display the correct button icons for PlayStation 3/4/5, Xbox 360/One/Series, Switch Pro, Steam Deck, and keyboard.

**Toast notifications** — non-intrusive pop-ups for connection status, save confirmations, and other events.

**Responsive scaling** — menu text, icons, and titles automatically scale with window size.

---

## Audio

The FFmpeg dependency has been **completely removed**. Audio is decoded by a lightweight built-in ADX decoder with zero external audio library requirements.

A global **master volume** slider scales BGM and SFX together. Set it from the command line with `--volume 0–100`.

---

## Training Mode

The community's external Lua training script has been rebuilt as a **native feature inside the engine**.

- **Frame meter** — real-time startup/active/recovery/hitstun/blockstun color-coded bar; frame advantage readout after every move.
- **Hitbox display** — colored overlays for hurtboxes 🟢, attackboxes 🔴, throwboxes 🟡, and pushboxes 🔵.
- **Stun meter** — live stun accumulation during combos.
- **Input display** — on-screen input history (P1 and P2) with frame durations, toggleable with **F7**.
- **Dummy AI** — block mode, parry mode (including red parry), stun mash, wakeup mash, wakeup reversal. WIP
- **Combo trials** — step-by-step combo challenges per character, tracked natively by the engine. WIP

---

## Save System

The PS2 memory card emulation layer has been fully replaced with a modern native save system:

- **Options & Controls** → `options.ini` (human-readable)
- **System Direction** → `direction.ini`
- **Replays** → compact binary files with metadata sidecars, stored in `replays/`
- Writes are **atomic** — a crash mid-save won't corrupt your data.
- A visual **20-slot replay picker** shows date, characters, and slot status.

All files are written to your user profile folder automatically, or to a `config/` folder next to the executable in **portable mode** (see below).

---

## Netplay

![Network Lobby](docs/images/network_lobby.gif)

Built on the upstream GekkoNet GGPO rollback netcode base, with significant additions:

- **STUN NAT hole-punching** — the client performs a STUN exchange to discover its public endpoint and punch through NAT, then hands the pre-punched UDP socket directly to GekkoNet so the NAT mapping is preserved.
- **UPnP port mapping** — automatically opens the required UDP port on compatible routers as a fallback when STUN alone is insufficient.
- **Lobby matchmaking server** — a lightweight Node.js server (zero dependencies) where players register presence, search for opponents, and exchange STUN room codes for P2P connection. Secured with HMAC-SHA256 request signing. The game includes a native C HTTP client that talks to the lobby asynchronously (non-blocking, background thread).
- **Native in-game lobby UI** — a full lobby screen inside the game, accessible from the main menu without requiring CLI flags. Shows player name / room code for internet play and LAN IP for local play. Pending connection requests are visually indicated to the receiving player.
- **Region-based filtering** — players can filter the lobby by region for lower-latency matches.
- **Client ID fingerprinting** — each client generates a stable unique `client_id` (persisted in `config.ini`) used as `player_id` with the lobby server, preventing username spoofing.
- **ImGui netplay diagnostics** — lobby browser, mini-HUD, and detailed diagnostics window with metrics history.
- **Sync test mode** — parameterized automated sync testing with Python test runner.
- **Desync prevention** — Frame 0 state reset, `WORK_Other_CONN` sanitization (F1549 fix), expanded rollback state (17 new fields), pointer-safe checksums, and per-subsystem checksums for faster triage.
- **Toast notifications** for connection events.

---

## Performance

![VSync & Turbo Mode](docs/images/vsync_turbo.gif)

50+ targeted optimizations — all fork-only, none in upstream:

- **SIMDe vectorization** — portable SSE2/NEON SIMD for 4-bit palette LUT conversion.
- **Texture array batching** — packs textures into `GL_TEXTURE_2D_ARRAY` for single-bind batched rendering.
- **Persistent mapped buffers** — triple-buffered VBOs eliminate per-frame `glBufferSubData` stalls.
- **PBO async texture uploads** — overlaps CPU conversion with GPU upload.
- **GPU palette compute** — hardware-accelerated palette lookup via compute shaders.
- **Active voice bitmask** — skips all silent audio channels with bit-scan iteration.
- **All game assets preloaded into RAM** — faster stage transitions, less disk stutter.
- **Hybrid frame limiter** — smooth frame pacing on Raspberry Pi (compensates for kernel timer jitter).
- **LTO + PGO** — Link-Time Optimization and Profile-Guided Optimization enabled for release builds.

---

## Platform Support

### Portable Mode
Drop a `config/` folder next to the executable and 3SX switches to portable mode — all saves, replays, and settings stay there instead of your user profile. Perfect for USB sticks and tournament setups.

### Video Broadcasting
Send your game feed to OBS or any compatible app without screen capture overhead:
- **Windows** — Spout2
- **macOS** — Syphon
- **Linux** — PipeWire _(WIP)_

### Raspberry Pi 4
Full cross-compilation support for RPi4 with Batocera or standalone Linux.

### Flatpak (Linux)
Install as a proper Linux desktop app with a `.desktop` entry and metadata.

### ARM64
Native ARM64 support — runs on Apple Silicon Macs and ARM Linux devices.

---

## CLI Options

```
--renderer gl|gpu|sdl      Select rendering backend (default: gl)
--volume 0-100             Set master volume percentage (default: 100)
--scale <factor>           Internal resolution multiplier (default: 1)
--window-pos <x>,<y>       Initial window position
--window-size <w>x<h>      Initial window size
--enable-broadcast         Enable Spout/Syphon/PipeWire video broadcast
--shm-suffix <suffix>      Shared-memory name suffix for broadcast
--sync-test                Start netplay sync-test as P1 (localhost)
--sync-test-client         Start netplay sync-test as P2 (localhost)
--help                     Show help message
```

Netplay is started from the in-game **Network** menu or via the classic CLI: `3sx 1 192.168.1.100`

---


## Developer & Building

### Compiling from Source
Please refer to the comprehensive [Build Guide](docs/building.md) for instructions on setting up MSYS2 (Windows) or the necessary Linux/macOS dependencies to compile this repository.

### Project Structure
```text
3sx/
├── 3sx.exe               # Main executable
├── sfiii3n/              # (Required) Place the sfiii3n.zip ROM here
├── config/               # (Optional) Create this for Portable Mode
├── replays/              # Engine-native binary replays are saved here
├── assets/
│   ├── ui/               # Sprites and ImGui fonts
│   ├── stages/           # Modded HD stage backgrounds 
│   └── bezels/           # Per-character arcade bezels
└── tools/                # Development, netplay, and sync-test utilities
```

### Developer Tools

The `tools/` directory contains utilities for debugging and development:

| Tool | Description |
|---|---|
| `lobby-server/` | Node.js lobby/matchmaking server (zero dependencies, HMAC auth) |
| `ui_designer/` | Coordinate editing tool for replicating native menu layouts |
| `lua_trial_parser.py` | Python script to parse trial data for the native trials implementation |
| `compile_tests.bat` | Script to compile and run the CMocka unit tests |
| `1click_windows_v2.bat` | Automated one-click MSYS2 setup and build script for Windows |

Additional root-level scripts: `compile.bat` (MSYS2 compilation), `lint.bat` (static analysis), `run_sync_test.bat` (quick sync test), and `build-deps.sh`.

---

## Code Quality

This fork applies a structured 24-session modernization pass across the entire codebase:

| Metric | Upstream | This Fork |
|---|---|---|
| Doxygen comments | 3 | **4,074** |
| Functions marked `static` | — | **3,400+** |
| Files `clang-format`'d | — | **1,000+** |
| Jump tables with bounds checks | — | **10+** |
| Unit tests | 0 | **30+** |

---

## CI / GitHub Actions

Seven workflow definitions (in `.github/workflows/`), which currently have the `.disabled` extension while they are being finalized for this branch:

| Workflow | Platform |
|---|---|
| `build_windows.yml` | Windows (MSYS2 / MinGW64) |
| `build_linux.yml` | Linux x86_64 |
| `build_linux_arm64.yml` | Linux ARM64 (cross-compile) |
| `build_macos.yml` | macOS |
| `build_rpi4.yml` | Raspberry Pi 4 (cross-compile) |
| `build_flatpak.yml` | Linux Flatpak |
| `release.yml` | Multi-platform release packaging |

---

## Dependencies

**Added (not in upstream):**
GLAD, SIMDe, stb_image, librashader, SDL_shadercross, Dear ImGui, CMocka, Tracy, Spout2

**Removed:**
FFmpeg (`libavcodec`, `libavformat`, `libavutil`) — replaced by custom ADX decoder

SDL3 tracks latest `main` branch vs upstream's pinned release tarball.

---

## Licenses

Full license texts for all third-party components are in [`THIRD_PARTY_NOTICES.txt`](THIRD_PARTY_NOTICES.txt).

| Library | License |
|---|---|
| [GekkoNet](https://github.com/HeatXD/GekkoNet) | MIT |
| [SDL3](https://github.com/libsdl-org/SDL) | zlib |
| [SDL\_shadercross](https://github.com/libsdl-org/SDL_shadercross) | zlib |
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
