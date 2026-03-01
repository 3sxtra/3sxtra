# 3SXtra

> A modernized, enhanced fork of **Street Fighter III: 3rd Strike** — rebuilt for modern GPUs, rollback netplay, and deep training tools.

The project name is **3SXtra**; the binary is `3sx` (`3sx.exe` on Windows).

> [!NOTE]
> This is an experimental, unofficial fork tailored to the author's needs. macOS and mainline Linux are lightly tested. Training Mode and Combo Trials are work-in-progress. Raspberry Pi 4 / Batocera is the primary Linux target.

## Why 3SXtra?

Most ways to play 3rd Strike online today rely on decade-old emulation wrapped in external tools.
3SXtra takes a different approach: the **game engine itself** is rebuilt from a full decomp, running natively on your hardware with a modern rendering pipeline, built-in rollback netplay, and a native training suite — no emulator, no Lua scripts, no FFmpeg.

- **vs. Fightcade / MAME**: native GPU rendering (OpenGL / Vulkan / Metal / DX12), in-engine rollback netplay, no emulation overhead.
- **vs. Online Edition (OE)**: full training mode with frame data, hitbox display, and input history. Open source. Runs on Raspberry Pi to Apple Silicon.

---

## Quick Start

1. **Get the ROM**: You must provide your own legally obtained `.afs` file. Place it in the `rom/` directory next to the executable.
2. **Launch**: Run `3sx.exe` (Windows) or `./3sx` (Linux / macOS).
3. **Controls**: Press **F1** anywhere to open the main menu and configure your controls. P1 defaults: WASD movement, JKIUOP buttons.
4. **Portable Mode** *(optional)*: Create an empty `config/` folder next to the executable to redirect all saves, replays, and settings there instead of your user profile.

---

## Table of Contents

- [Graphics & Visuals](#graphics--visuals)
- [Menus & Interface](#menus--interface)
- [Audio](#audio)
- [Training Mode](#training-mode)
- [Save System & Replays](#save-system--replays)
- [Netplay](#netplay)
- [Performance](#performance)
- [Platform Support](#platform-support)
- [CLI Options](#cli-options)
- [Common Issues](#common-issues)
- [Developer & Building](#developer--building)

---

## Graphics & Visuals

### Two Modern Rendering Backends

| Backend | API | Notes |
|---|---|---|
| **OpenGL 3.3+** | GLSL | Custom GPU pipeline: texture array batching, PBO async uploads, compute-shader palette conversion |
| **SDL_GPU** | Vulkan / Metal / DX12 | Second backend via SDL3's `SDL_GPU` API |
| **SDL2D** | SDL3 2D API | Lightweight software fallback using `SDL_RenderDebugText` |

Select a backend with `--renderer gl`, `--renderer gpu`, or `--renderer sdl`.

### RetroArch Shader Support (librashader)
- Load any `.slangp` preset at runtime — CRT scanlines, ScaleFX, xBR, and hundreds more.
- Hot-swap shaders from the in-game shader picker (**F2**).

### Arcade Bezels
- 40+ high-quality per-character bezels surround the viewport, just like a real arcade cabinet.
- Bezels swap automatically when characters change and reset to defaults on menus and title screen.

### HD Stage Backgrounds
- Per-stage modded multi-layer parallax backgrounds rendered at full output resolution, composited behind the game sprites.
- Assets live in `assets/stages/stage_XX/` — any stage with assets present uses them automatically.
- Toggle on/off from the Mods menu (**F3**); falls back to original backgrounds when disabled or absent.

### Resolution Scaling
- User-configurable output resolution — play at native monitor resolution or scale up for sharper visuals.

---

## Menus & Interface

Open overlay menus with F-keys at any time — they never interrupt gameplay:

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

The community's external Lua training script has been rebuilt as a **native feature inside the engine** — no external tools, no frame-rate penalty.

| Feature | Details |
|---|---|
| **Frame meter** | Color-coded startup / active / recovery / hitstun / blockstun bar, plus frame-advantage readout |
| **Hitbox display** | Hurtboxes 🔵, attackboxes 🔴, throwboxes 🟡, pushboxes 🟢 — individually toggleable |
| **Input display** | On-screen stick + button history for P1 and P2 with frame durations |
| **Stun meter** | Live stun accumulation readout during combos |
| **Dummy AI** | Block mode, parry (incl. red parry), stun mash, wakeup mash, wakeup reversal *(WIP — partially implemented)* |
| **Combo trials** | Step-by-step built-in challenges per character, tracked natively by the engine *(WIP — early stage)* |

---

## Save System & Replays

The PS2 memory card emulation layer has been fully replaced with a modern native save system:

- **Options & Controls** → `options.ini` (human-readable)
- **System Direction** → `direction.ini`
- **Replays** → compact binary files with metadata sidecars, stored in `replays/`
- Writes are **atomic** — a crash mid-save won't corrupt your data.
- A visual **20-slot replay picker** shows date, characters, and slot status.

All files are written to your user profile folder automatically, or to a `config/` folder next to the executable in **portable mode**.

---

## Netplay

![Network Lobby](docs/images/network_lobby.gif)

Built on GekkoNet GGPO rollback netcode, with significant additions:

| Feature | Details |
|---|---|
| **STUN NAT hole-punching** | Discovers public endpoint via STUN, punches through NAT, hands the pre-punched UDP socket to GekkoNet |
| **UPnP port mapping** | Automatically opens the required UDP port on compatible routers as a fallback |
| **Lobby matchmaking server** | Lightweight Node.js server (zero deps): player registration, searching, HMAC-SHA256 auth |
| **Native in-game lobby UI** | Full lobby screen inside the game — accessible from the main menu, no CLI flags required |
| **Async lobby comms** | All HTTP lobby traffic runs on a background thread; never blocks the game loop |
| **Internet lobby display** | Shows player name / room code instead of raw IP addresses |
| **LAN lobby display** | Shows player LAN IP for local network sessions |
| **Pending invite indicator** | Visual cue shown when a connection request is incoming |
| **Region filtering** | Filter the lobby by region for lower-latency matches |
| **Client ID fingerprinting** | Stable `client_id` in `config.ini` prevents username spoofing |
| **Desync prevention** | Frame 0 state reset, `WORK_Other_CONN` sanitization, 17 expanded rollback fields, pointer-safe checksums |
| **Sync test mode** | Parameterized automated sync-test with Python test runner |

Netplay can be started from the in-game **Network** menu or via the classic CLI: `3sx 1 192.168.1.100`

---

## Performance

![VSync & Turbo Mode](docs/images/vsync_turbo.gif)

Targeted optimizations — all fork-only, none in upstream:

| Optimization | Details |
|---|---|
| **SIMDe vectorization** | Portable SSE2/NEON SIMD for 4-bit palette LUT conversion |
| **Texture array batching** | Packs textures into `GL_TEXTURE_2D_ARRAY` for single-bind batched rendering |
| **Persistent mapped buffers** | Triple-buffered VBOs eliminate per-frame `glBufferSubData` stalls |
| **PBO async texture uploads** | Overlaps CPU conversion with GPU upload |
| **GPU palette compute** | Hardware-accelerated palette lookup via compute shaders — smooth 60fps at high resolutions on modest hardware |
| **Active voice bitmask** | Skips all silent audio channels with bit-scan iteration |
| **RAM asset preload** | All game assets loaded into memory at startup — faster stage transitions, less disk stutter |
| **Hybrid frame limiter** | Smooth frame pacing on Raspberry Pi (compensates for kernel timer jitter) |
| **LTO + PGO** | Link-Time Optimization and Profile-Guided Optimization enabled for release builds |

---

## Platform Support

| Platform | Status |
|---|---|
| **Windows** (x86-64) | ✅ Primary development platform |
| **Raspberry Pi 4 / Batocera** | ✅ Full cross-compilation + Batocera integration |
| **Linux x86-64** | ✅ Tested |
| **Linux ARM64** | ✅ Native ARM64 support |
| **macOS** (Intel + Apple Silicon) | ⚠️ Builds, lightly tested |
| **Flatpak (Linux)** | ⚠️ Packaging defined, not fully tested |

### Portable Mode
Drop a `config/` folder next to the executable and 3SXtra switches to portable mode — all saves, replays, and settings stay there instead of your user profile. Perfect for USB sticks and tournament setups.

### Video Broadcasting
Send your game feed to OBS or any compatible app without screen capture overhead:
- **Windows** — Spout2
- **macOS** — Syphon
- **Linux** — PipeWire *(WIP)*

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

---

## Common Issues

| Problem | Solution |
|---|---|
| **Black screen on launch** | Ensure `rom/` contains the correct `.afs` ROM file next to the executable. |
| **Shaders not loading** | Verify the `shaders/` directory was copied alongside the binary (done automatically by CMake). Try `--renderer gl` if the GPU backend has issues. |
| **Netplay desync** | Both players must run the **exact same build**. Use the sync test mode (`--sync-test` / `--sync-test-client`) to verify. |
| **Controller not detected** | Press **F1** → Input Mapping. The game uses SDL3's gamepad API — most controllers work. Generic joysticks are auto-detected. |
| **No audio** | Check `--volume` is not set to 0. The built-in ADX decoder requires no external libraries. |
| **Poor performance on RPi4** | Use `--renderer gl` (not `gpu`). Ensure LTO is enabled in the build (`-DCMAKE_BUILD_TYPE=Release`). |

---

## Developer & Building

### Minimum Requirements

| Requirement | Version |
|---|---|
| **CMake** | 3.24+ |
| **C compiler** | GCC or Clang with C11 support |
| **C++ compiler** | g++ or clang++ (for ImGui and SDL wrappers) |
| **SDL3** | Latest `main` branch (built automatically by `build-deps.sh`) |
| **SPIR-V compiler** | `glslc` (Vulkan SDK) or `glslangValidator` |

### Compiling from Source
Please refer to the comprehensive [Build Guide](docs/building.md) for instructions on setting up MSYS2 (Windows) or the necessary Linux/macOS dependencies.

### Project Structure
```text
3sx/
├── 3sx.exe               # Main executable
├── rom/                  # (Required) Place the .afs ROM file here
├── config/               # (Optional) Create this for Portable Mode
├── replays/              # Engine-native binary replays are saved here
├── assets/
│   ├── ui/               # Sprites and ImGui fonts
│   ├── stages/           # Modded HD stage backgrounds
│   └── bezels/           # Per-character arcade bezels
├── shaders/              # GLSL and SPIR-V shader files
└── tools/                # Development, netplay, and sync-test utilities
```

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
