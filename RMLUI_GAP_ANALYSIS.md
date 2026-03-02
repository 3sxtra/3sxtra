# RmlUI Gap Analysis — Remaining Work

> **Audit Date**: 2026-03-01 (verified)  
> **Scope**: Cross-referencing every native rendering call site against the 69+ implemented RmlUI components and 70 `.rml/.rcss` assets.

---

## Summary

All 27 Phase 3 sub-phases are marked ✅ in [RMLUI_INTEGRATION_PLAN.md](file:///D:/3sxtra/RMLUI_INTEGRATION_PLAN.md). This audit identified **2 remaining gaps** that need RmlUI replacements — not just gating. Four previously flagged items (§3E `Pause_1st_Sub`, §3G `Flash_1P_or_2P`, §4 `effect_57` Save/Load banner, §1 Network Lobby) were verified as already gated and have been removed.

| # | Gap | When Player Sees It | RmlUI Work Needed | Effort |
|---|-----|--------------------|--------------------|--------|
| **1** | [Char select double-rendering](#1-character-select-double-rendering) | Every match — native `eff38` name plate and `eff42` timer render under RmlUI overlay | Extend `rmlui_char_select.cpp` to fully replace name plate + timer sprites | Medium |
| **2** | [`effect_66` background rectangles](#2-effect_66-background-rectangles) | Many menus — grey translucent rects appear behind RmlUI panels | Add RmlUI `<div>` background panels in each affected `.rml` + suppress native draw | Medium |

---

## 1. Character Select Double-Rendering

**Priority**: P0 — visible every match.

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

## 2. `effect_66` Background Rectangles

**Priority**: P1 — subtle visual layering, but affects many screens.

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

> **Note**: The `effect_66_init` call inside `Setup_Replay_Sub()` (line 2945) is also ungated — covered here.

---

## Resolved Gaps

The following items were previously flagged but verified as already gated or not applicable:

| Item | Resolution |
|---|---|
| §3E `Pause_1st_Sub` | Already gated behind `use_rmlui` |
| §3G `Flash_1P_or_2P` | Already gated behind `use_rmlui` |
| §4 `effect_57` Save/Load Direction banner | Both `Save_Direction()` and `Load_Direction()` in `menu_input.c` already wrap `Setup_Replay_Sub()` with `if (!(use_rmlui && rmlui_menu_sysdir))` |
| §1 Network Lobby peer list & popups | All native rendering fully gated: effect init (line 665), banner draw (750), SSPutStr_Bigger toggles/headers/peers/status (894), and all four `NetLobby_Draw*` popup calls (1034/1054/1077/1099) behind `use_rmlui && rmlui_menu_lobby` |

---

## Structural Limitations (Won't Fix)

| Item | Status |
|---|---|
| **Ping graph** (`ImGui::PlotLines` equivalent) | No RmlUI equivalent — would need custom element. Debug-only, low priority. |
| **`SSPutStr_Bigger` gradient fidelity** | RCSS doesn't support `background-clip: text`. Flat colors are acceptable. |
| **VS Screen overlay** | Intentional — tilemap slide-in animation is the iconic visual. RmlUI text sits on top. |
