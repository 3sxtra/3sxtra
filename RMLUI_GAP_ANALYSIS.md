# RmlUI Gap Analysis — Remaining Work

> **Audit Date**: 2026-03-01 (verified)  
> **Scope**: Cross-referencing every native rendering call site against the 69+ implemented RmlUI components and 70 `.rml/.rcss` assets.

---

## Summary

All 27 Phase 3 sub-phases are marked ✅ in [RMLUI_INTEGRATION_PLAN.md](file:///D:/3sxtra/RMLUI_INTEGRATION_PLAN.md). This audit identified **4 remaining gaps** that need RmlUI replacements — not just gating. Two previously flagged items (§3E `Pause_1st_Sub`, §3G `Flash_1P_or_2P`) were verified as already gated and have been removed.

| # | Gap | When Player Sees It | RmlUI Work Needed | Effort |
|---|-----|--------------------|--------------------|--------|
| **1** | [Network Lobby peer list & popups](#1-network-lobby-peer-list--popups) | Every network session — peer names, IPs, ON/OFF, challenge popups all render natively | Extend `rmlui_network_lobby.cpp` with peer array bindings + popup modal RML | Significant |
| **2** | [Char select double-rendering](#2-character-select-double-rendering) | Every match — native `eff38` name plate and `eff42` timer render under RmlUI overlay | Extend `rmlui_char_select.cpp` to fully replace name plate + timer sprites | Medium |
| **3** | [`effect_66` background rectangles](#3-effect_66-background-rectangles) | Many menus — grey translucent rects appear behind RmlUI panels | Add RmlUI `<div>` background panels in each affected `.rml` + suppress native draw | Medium |
| **4** | [`effect_57` Save/Load Direction banner](#4-effect_57-saveload-direction-banner) | Option → System Direction → SAVE or LOAD page — red banner still renders | Gate 2 call sites + existing `sysdir.rml` already covers the content | Trivial |

---

## 1. Network Lobby Peer List & Popups

**Priority**: P0 — most visually broken screen in RmlUI mode.

### When it fires

Mode Menu → Network → any lobby phase with discovered peers or active challenge.

### What renders natively (ungated)

`Network_Lobby()` phases 4–8 in [menu.c](file:///D:/3sxtra/src/sf33rd/Source/Game/menu/menu.c):

| Native Call | Lines | What it draws |
|---|---|---|
| `SSPutStr_Bigger` | 892–899 | "ON"/"OFF" toggle values for LAN/NET auto-connect |
| `SSPutStr_Bigger` | 907–913 | Section headers: "LAN DISCOVERED", "INTERNET PEERS" |
| `SSPutStr_Bigger` | 932–963 | Peer names + IP addresses (entire peer list) |
| `SSPutStr_Bigger` | 989–1019 | Connection status bar text |
| `NetLobby_DrawIncomingPopup()` | 471–542 | `Renderer_Queue2DPrimitive` bg + `SSPutStr2` "INCOMING CHALLENGE!" + name/ping + ACCEPT/DECLINE |
| `NetLobby_DrawOutgoingPopup()` | 559–624 | `Renderer_Queue2DPrimitive` bg + "CONNECTING..." + target name + CANCEL |

Phase 3.16/3.26 gated the 6 `effect_61` menu items but left all dynamic per-frame rendering running.

### RmlUI replacement needed

1. **Extend [rmlui_network_lobby.cpp](file:///D:/3sxtra/src/port/sdl/rmlui_network_lobby.cpp)** with:
   - `lan_peers[]` array binding (name, IP, ping per entry) from `Discovery_GetPeers()`
   - `net_peers[]` array binding
   - `connection_status` string binding
   - `lan_auto_connect` / `net_auto_connect` toggle bindings
   - `incoming_popup_visible`, `outgoing_popup_visible` booleans
   - `popup_challenger_name`, `popup_ping` string bindings

2. **Extend [network_lobby.rml](file:///D:/3sxtra/assets/ui/network_lobby.rml)** with:
   - `<div data-for="peer : lan_peers">` scrollable peer list panel
   - `<div data-for="peer : net_peers">` internet peer panel
   - Popup modal overlay (data-if on visibility booleans)

3. **Gate** all `SSPutStr_Bigger` calls in phases 4–8 + both `NetLobby_Draw*` functions with `if (!use_rmlui || !rmlui_menu_lobby)`.

### Resolved: Blue Background

The blue textured background on the Network Lobby screen is rendered by `effect_57_init(0x4E, 0, 0, 0x45, 0)` — the same Effect 57 that renders header banners, but with **palette `0x45` (blue/cyan)**. This call was originally gated inside the native-only `else` block alongside the grey boxes, text sprites, and description text.

Because `Network_Lobby` case 1 calls `effect_work_init()` (unlike most sub-menus which use `Menu_Suicide` to swap items without destroying effects), the inherited Effect 57 background from Mode_Select is destroyed. The fix was to move the `effect_57_init(0x4E, 0, 0, 0x45, 0)` call **outside** the RmlUI gate so it runs unconditionally. The RmlUI overlay renders transparently on top of the native blue background.

Other native elements (Effect 66 grey boxes, Effect 61 text sprites, Effect 45 description text) remain correctly gated inside the native-only `else` block.

---

## 2. Character Select Double-Rendering

**Priority**: P1 — visible every match.

### When it fires

Mode Menu → Arcade/Versus/Training → Character Select screen.

### What renders natively (ungated)

The char select RmlUI overlay ([rmlui_char_select.cpp](file:///D:/3sxtra/src/port/sdl/rmlui_char_select.cpp) / [char_select.rml](file:///D:/3sxtra/assets/ui/char_select.rml)) replaces only text elements — native sprite effects continue rendering underneath, causing duplicate display.

| Element | Effect | Double-renders with RmlUI? |
|---|---|---|
| Character name plate | `effect_38` (`sel_pl.c:624,668,671,727`) | **Yes** — RmlUI already shows name text |
| Timer digits | `effect_42` (reads `Select_Timer` directly) | **Yes** — RmlUI already shows timer |
| Player indicator "1P"/"2P" | `effect_39` | No RmlUI equivalent yet |
| SA selection icons | `effect_52` | No RmlUI equivalent yet |
| Red separator lines | `effect_69` | No RmlUI equivalent yet |
| Cursor bracket | `effect_K6` | No RmlUI equivalent yet |
| BG panels | `effect_66` ×10 (`sel_pl.c:1804-1828`) | Covered by §3 below |
| Confirm flash | `Renderer_Queue2DPrimitive` | No RmlUI equivalent yet |

### RmlUI replacement needed

**Phase A** (eliminate double-rendering — safest):
- Gate `effect_38_init` calls in `sel_pl.c` — RmlUI already shows the character name
- Gate `effect_42` timer draw path inside `eff42.c` — RmlUI already shows the timer
- These two are pure duplicates, so gating them causes no visual loss

**Phase B** (full sprite replacement — riskier, optional stretch):
- Add RmlUI elements for "1P"/"2P" indicators, SA icons, red lines, cursor bracket
- Gate `effect_39/52/69/K6` — risky because they're in the `Order[]` sprite chain; draw-time suppression inside each `eff*.c` is safer than removing init calls

---

## 3. `effect_66` Background Rectangles

**Priority**: P2 — subtle visual layering, but affects many screens.

### When it fires

SysDir pages, Extra Option pages, training pause, button config, network lobby, char select, and more. Grey translucent rectangles appear behind the RmlUI panels.

### Ungated call sites

| Call Site | Context |
|---|---|
| `menu.c:669-672` | Network lobby peer boxes (LAN/Internet) |
| `menu.c:1208` | Replay load background |
| `menu.c:2054-2058` | Button config dual-panel background |
| `menu_input.c:309-336` | `Setup_Next_Page` direction/sysdir page backgrounds |
| `menu_input.c:351` | Main background for paged menus |
| `menu_input.c:1564-1581` | Training pause menu backgrounds |
| `menu_input.c:1632` | Blocking training background |
| `menu_input.c:2013` | Training option background |
| `sel_pl.c:1804-1828` | Char select background panels (10+ calls) |
| `pause.c:268` | Pause menu background |

### RmlUI replacement needed

Many of these are hardcoded into the `Order[]` sprite chain — gating at init risks breaking the state machine.

**Approach**: Add a **render-time visibility check** inside [eff66.c](file:///D:/3sxtra/src/sf33rd/Source/Game/effect/eff66.c)'s update loop:
- If `use_rmlui` is true and the associated menu's toggle is active, suppress the `sort_push_request` draw call
- Map each effect slot to its menu context (e.g., slots `0x8A`/`0x8B` in lobby → `rmlui_menu_lobby`, training slots → `rmlui_menu_training`, etc.)
- Meanwhile, the existing `.rml` documents already have their own `<div>` backgrounds — the native rects are purely redundant when RmlUI is active

---

## 4. `effect_57` Save/Load Direction Banner

> [!IMPORTANT]
> Effect 57 serves a **dual role**: full-screen colored backgrounds (palette `0x45` = blue, `0x3F` = red) AND narrow header banners. When gating `effect_57_init` calls behind `use_rmlui`, only gate the **banner** instances — the background instance must remain unconditional. See [TEXT_RENDERING_SYSTEMS.md](file:///D:/3sxtra/TEXT_RENDERING_SYSTEMS.md) §3 for details.

**Priority**: P3 — rarely accessed.

### When it fires

Option → System Direction → navigate to SAVE or LOAD page. The animated red banner header renders behind the existing RmlUI System Direction panel.

### What's ungated

`Setup_Replay_Sub()` at `menu.c:2940` calls `effect_57_init()` unconditionally. Two callers don't gate it:

| Caller | File:Line |
|---|---|
| `Save_Direction()` case 0 | `menu_input.c:393` |
| `Load_Direction()` case 0 | `menu_input.c:432` |

All other callers of `Setup_Replay_Sub()` are properly gated (Load_Replay, Setup_Save_Replay_1st).

### RmlUI replacement needed

The existing [sysdir.rml](file:///D:/3sxtra/assets/ui/sysdir.rml) already covers the content for this screen. Only the gate is missing:

```c
// menu_input.c:393 — wrap Setup_Replay_Sub call:
if (!(use_rmlui && rmlui_menu_sysdir))
    Setup_Replay_Sub(1, 0x70, 0xA, 2);

// menu_input.c:432 — same:
if (!(use_rmlui && rmlui_menu_sysdir))
    Setup_Replay_Sub(1, 0x70, 0xA, 2);
```

> **Note**: The `effect_66_init` call inside `Setup_Replay_Sub()` (line 2945) is also ungated — covered by §3 above.

---

## Structural Limitations (Won't Fix)

| Item | Status |
|---|---|
| **Ping graph** (`ImGui::PlotLines` equivalent) | No RmlUI equivalent — would need custom element. Debug-only, low priority. |
| **`SSPutStr_Bigger` gradient fidelity** | RCSS doesn't support `background-clip: text`. Flat colors are acceptable. |
| **VS Screen overlay** | Intentional — tilemap slide-in animation is the iconic visual. RmlUI text sits on top. |
