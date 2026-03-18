# Legacy Architecture Modernization Targets

**Fact-Check Status:** Verified. All structural claims regarding the `struct _TASK` memory layout (in `src/include/structs.h`), its instantiations, and the presence of `_Jmp_Tbl` tables in `screen/` and `system/` files have been rigorously checked against the `src/sf33rd/` codebase and proven exactly accurate.

Following the pattern established by the Menu Backend Migration, there are several components of the game engine that use identical architectural concepts (nested jump tables and integer state arrays) and could benefit from a similar modernization effort.

This document outlines the potential for migrating these legacy monolithic subsystems to data-driven state registries featuring standard lifecycle callbacks (`on_enter`/`on_tick`/`on_exit`) and shared helpers. 

## Engine Components (Sorted by Risk: LOW to HIGH)

### 1. Player/Character Select Hub (`screen/sel_pl.c`)
This is the most obvious candidate. Just like the menu, the character select screen is a massive UI state machine involving cursors, timers, and sequence transitions.

*   **Risk Level:** **LOW**. This is pure UI flow and has absolutely zero impact on in-match fighting mechanics or frame data.
*   **Current State:** It is heavily layered with jump tables like `Sel_PL_Jmp_Tbl`, `PL_Sel_Jmp_Tbl`, `Face_Jmp_Tbl`, `OBJ_Jmp_Tbl`, and `Handicap_Jmp_Tbl` (spanning over 2,000 lines). State is tracked in arbitrary global arrays like `SP_No[]`, `Face_No[]`, and `SO_No[]`.
*   **Modernization:** You could build a `CharSelectPhase` registry. The selection sequence (Select Character -> Select Super Art -> Select Handicap/Color) could be modeled as distinct states with `on_enter` (setup cursor), `on_tick` (handle directional input and selections), and `on_exit` (lock in choice, load assets). You could even reuse the new cursor and fade helpers created for the menu migration.

### 2. ✅ Transient Flow Screens (`screen/win.c`, `screen/continue.c`, `screen/gameover.c`, `system/saver.c`)
These linear presentation screens have been **migrated to the `MenuScreen` registry** (March 2026).

*   **Risk Level:** **LOW**. Post-match/pre-match UI presentations, strictly outside the scope of fighting mechanics.
*   **Status:** **COMPLETED.** All four screens now use `MenuScreen` callbacks (`on_enter`/`on_tick`/`on_exit`) registered via `__attribute__((constructor))` self-registration. The legacy jump tables (`Win_Jmp_Tbl`, `Lose_Jmp_Tbl`, `Continue_Jmp_Tbl`, `GameOver_Jmp_Tbl`) have been removed, with the original `.c` files retained as thin wrappers calling `MenuScreen_Goto` → `MenuScreen_Tick` → `MenuScreen_ExitToLegacy`.
*   **New Files:** `src/port/screens/ms_continue.c`, `ms_win.c`, `ms_gameover.c`, `ms_saver.c`
*   **New Enum Values:** `MENU_SCREEN_CONTINUE`, `MENU_SCREEN_WIN`, `MENU_SCREEN_LOSER`, `MENU_SCREEN_GAMEOVER`, `MENU_SCREEN_SAVER`
*   **Note:** The linker flags in `CMakeLists.txt` use `-Wl,--start-group`/`--end-group` which is GCC/ld-specific; will need adjustment if targeting MSVC.

### 3. Pre-Game Flow Screens and Demos (`screen/entry.c`, `screen/ranking.c`, `demo/demo02.c`)
Operating alongside the transient post-game screens, the coin-entry, leaderboard rankings, and attract mode demos use identical architecture.

*   **Risk Level:** **LOW**. Strictly out-of-match UI/flow systems.
*   **Current State:** Driven by `Main_Jmp_Tbl` arrays mapped to state indices (e.g., `E_No[]`, `D_No[]`).
*   **Modernization:** These screens represent the "idle loop" of the arcade cabinet. Converting them to data-driven flow screens would harmonize the entire out-of-match experience (Entry -> Demo -> Ranking -> Entry) into a single, cohesive `MenuScreen`-style loop.

### 4. Stage Background Animations (`stage/bg*.c`)
The game contains over twenty distinct background animation files (e.g., `bg000.c`, `bg010.c`, `bns_bg.c`), all of which reinvent the wheel for layer scrolling and intro panning.

*   **Risk Level:** **LOW**. Background visuals have zero impact on hitbox collision, hurtboxes, or character frame data. Safe visual-only refactoring.
*   **Current State:** Each stage uses a bespoke `bg_jmp` array (e.g., `bg0002_jmp[3] = { bg0001_init00, bg0000_demo, bg_base_move_common }`). Similar to `TASK`, the state of the background is tracked via `bgw_ptr->r_no_0` and nested `switch/case` fallthroughs.
*   **Modernization:** These dozens of bespoke implementations could be replaced by a single, generic `StageBackground` data structure that defines keyframes, scroll velocities, and parallax depths, entirely removing the need for 20+ duplicated state machines.

### 5. Sound Effect Dispatch (`sound/se_data.c`)
While not a continuous state machine, the Sound system utilizes a massive 1024-entry global function pointer array (`sound_effect_request[]`) to map generic audio request IDs to specific internal routines (`Se_Myself`, `Se_Shock`, `Se_Let`).

*   **Risk Level:** **LOW**. Purely audio dispatch logic. Does not affect game behavior loops unless an effect explicitly ties into an animation delay.
*   **Current State:** The relationship between a sound ID and its actual audio file / playback priority is completely obfuscated behind thousands of repetitive function point casts.
*   **Modernization:** Converting this massive array into a defined structural registry (e.g., `SoundEvent { u16 adx_id; u8 priority; bool is_stereo; }`) would make adding new UI sounds or fixing audio bugs data-driven, rather than requiring code recompilation to change an opaque index.

### 6. The Pause System (`system/pause.c` & `system/reset.c`)
*   **Risk Level:** **MEDIUM**. While it pauses the game flow, it directly interacts with inputs and step-forward logic. Minor risk of introducing input-drop bugs across the pause/unpause boundary.
*   **Current State:** Driven by `Main_Jmp_Tbl` and `Flash_Jmp_Tbl`, with state tracked via `r_no[]` arrays, handling the overlay visuals and frame-stepping logic.
*   **Modernization:** The pause system interacts closely with the In-Game menus. Giving it a defined lifecycle (`on_enter` to snapshot the screen, `on_tick` to handle inputs/menus, `on_exit` to resume updates) would integrate nicely with the newly migrated pause menus (`MENU_SCREEN_PAUSE_MENU`). 

### 7. Character Entrance Animations (`animation/appear.c`)
The game handles walk-ons, car arrivals, and boss intros using the `appear_player()` dispatcher.

*   **Risk Level:** **MEDIUM**. These are pre-match animations. While conceptually simple, errors here can affect the starting frame/timing of Round 1 if the transition from "intro" to "fight state" is bungled.
*   **Current State:** It relies entirely on a 42-entry `appear_jmp_tbl` mapped to `Appear_00000` through `Appear_41000`. It depends on deeply nested state tracking via `wk->wu.routine_no[3]` and `routine_no[4]`.
*   **Modernization:** Each of these 42 routines could be represented by a standard `CinematicSequence` or `AnimationFlow` registry (with `on_enter`/`on_tick`/`on_exit`), which would drastically simplify adding new intro sequences for custom characters.

### 8. The `TASK` System (`src/include/structs.h` & `system/work_sys.h`)
The `TASK` system is the fundamental scheduling unit for non-gameplay state machines. It operates as a cooperative multitasking array where different engine subsystems (like Save/Load, Pause, and Reset) claim a slot and assign a function pointer.

*   **Risk Level:** **MEDIUM**. While largely outside core fighting mechanics, it handles global state transitions and intercepts systems like Reset and Pause logic.
*   **Current State:** Defined as `extern struct _TASK task[11]`, tasks track their state using `r_no[4]` and an unstructured `free[4]` array that acts as arbitrary scratch space, requiring tribal knowledge to know what `free[1]` means in any given context. It has no formal `init` or `shutdown` phases.
*   **Modernization:** The generic `TASK` container has outgrown its usefulness. Instead of passing `struct _TASK*` everywhere, specific core systems should be migrated to their own dedicated, type-safe managers featuring standard lifecycle callbacks (similar to what was done for the Menu Backend).

### 9. The Top-Level Game State Machine (`game.c`)
This is the highest-level supervisor for the game engine, controlling the flow between demos, intros, fighting, and ending sequences.

*   **Risk Level:** **HIGH**. It controls the transition into and out of gameplay. Errors here can break netplay rollback synchronization, cause state desyncs, or corrupt memory upon entering a match.
*   **Current State:** It relies entirely on the `G_No[]` array to navigate wildly nested dispatch tables: `Main_Jmp_Tbl`, `Game_Jmp_Tbl`, `Game00_Jmp_Tbl`, `Game12_Jmp_Tbl`, etc. 
*   **Modernization:** You could replace the `G_No[]` array assignments with explicit, named transitions (`GameState_GotoPhase(PHASE_FIGHT_INTRO)`). Providing standard `on_enter_state` and `on_exit_state` callbacks would make resource loading and memory cleanup (like character sprite loads and background unloads) much safer and less prone to leaks. *(Note: as highlighted in the menu document, `G_No[]` is synchronized during netplay rollback, so this would require careful compatibility mapping)*.

### 10. CPU AI and Player State Logic (`com/com_pl.c`)
While modifying gameplay logic implies higher risk (due to networking and rollback sync), the CPU AI and player state assignment systems use the exact same legacy jump-table structure as the menus.

*   **Risk Level:** **HIGH**. **DO NOT TOUCH IF PRESERVING MECHANICS.** Any change here has a 100% chance of altering character behavior, breaking combo timing, throwing off AI logic, and causing rollback desyncs during netplay.
*   **Current State:** AI states and hit reactions use arrays like `Com_Jmp_Tbl`, `Damage_Jmp_Tbl`, `Float_Jmp_Tbl`, and `Flip_Jmp_Tbl`, with state indexes stored in cryptic `CP_No[]` magic-number arrays.
*   **Modernization:** Modernizing the AI state logic into named, formal state handlers (`AI_STATE_FLOAT`, `AI_STATE_DAMAGE`) with bound context structs would make Mamba RL bot development, data extraction, and general AI modifications significantly safer and more legible.

### 11. Visual Effects System (`effect/eff*.c`)
The game spawns hundreds of transient visual effects (dust, sparks, hit-sparks, projectiles) using independent worker scripts.

*   **Risk Level:** **HIGH**. **DO NOT TOUCH IF PRESERVING MECHANICS.** Important gameplay elements, especially active projectiles (fireballs) and hit-sparks which govern frame-freeze timings, are deeply intertwined with the effect dispatch system. Rebuilding this alters logic drastically.
*   **Current State:** Each effect script (e.g., `effm8.c`, `eff06.c`) acts as an isolated mini-state machine driven entirely by `ewk->wu.routine_no[0]` and `routine_no[1]` mapped against gigantic `switch` statements.
*   **Modernization:** Creating an `EffectLifecycle` registry would allow the engine to standardize memory allocation, sorting, and cleanup for all particles without every single effect needing a bespoke `switch` fallthrough to destroy itself.

### Summary of Benefits
If you apply the "Menu Migration" blueprint to these systems, you will get the exact same benefits:
*   **Massive Code Reduction:** Stripping out redundant `case 0: FadeOut; case 2: FadeIn;` boilerplate across dozens of files.
*   **Named States vs Numbers:** Replacing things like `SP_No[1] = 2;` with `goto_phase(CHAR_SELECT_SUPER_ART)`.
*   **Safer Resource Management:** `on_exit` callbacks ensure that specific visual effects or allocated buffers don't accidentally leak into the next state, a very common issue with the legacy `switch/case` fallthroughs.

---

## Conclusion & Evaluation

Based on the [DeckCook Protocol](../knowledge/deckcook_document_standards/artifacts/design_philosophy/three_judges_protocol.md), here is an evaluation of this infrastructure modernization roadmap from three distinct engineering perspectives.

### 🇷🇺 JUDGE DMITRI PETROV – Reliability & Rigor
> "Good enough is the enemy of correct. Replacing opaque index arrays with formal lifecycles is mandatory for survival."

*   **Technical Soundness:** The current reliance on untyped scratch arrays (`free[4]`) is a disaster waiting to happen. Moving to typed context structs and standard `on_enter`/`on_exit` callbacks directly mitigates the risk of memory/resource leaks across state transitions.
*   **Resilience:** Creating discrete flow objects that guarantee teardown makes error recovery far safer than the current fallthrough approach. 
*   **Verdict:** **9/10**. Solid for demanding real-world use. This is a critical infrastructure refactoring that correctly prioritizes state safety. 

### 🇨🇳 COACH WANG JINPING – Philosophy & Elegance
> "Precision is beauty. When the structures try to be everything, they become nothing."

*   **Clarity:** The legacy CPS3 jump-table structures are the antithesis of clarity. 
*   **Discipline:** I commend the discipline of breaking down monolithic codebases into named, deterministic flow states. Magic numbers like `task[TASK_MENU].r_no[1] = 16` obscure the intent of the programmer.
*   **Verdict:** **8/10**. Championship quality. Organizing flow control into distinct registries brings much-needed order to decades-old chaos.

### 🇺🇸 DR. ISABELLA ROSSI – User Impact & Real-World Value
> "Solutions should be empowering. Maintainers cannot add value if they are terrified of the underlying architecture."

*   **User-Centricity (Developer Experience):** The primary user here is the engine maintainer. Right now, onboarding a new graphics programmer requires an apprenticeship in tribal knowledge.
*   **Impact & Scalability:** By reducing the boilerplate and relying on shared helpers, this modernization exponentially speeds up iteration. 
*   **Verdict:** **9/10**. Strong impact. It directly improves developer velocity, which ultimately equates to faster, higher-quality feature delivery for the end user.
