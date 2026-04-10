# Comprehensive Architecture Audit & Gap Analysis: 3sxtra vs Modern SOTA

## 1. Executive Summary & Auditing Context

This comprehensive audit evaluates the **3sxtra** modernization project (the CPS3 port) against modern State-of-the-Art (SOTA) engineering principles, heavily informed by the foundational architectural breakthroughs of the **DOOM (1993)** and **Quake 3 Arena (1999)** source codes. 

The current 3sxtra architecture is a hybrid: a legacy 1999 cooperative task scheduler (`cpLoopTask`) wrapped in modern middleware (SDL3, RmlUi, GekkoNet). Moving forward, we must adopt the **"DOOM Equivalent" Mandate**: Optimization isn't just about saving 5% compute; it must unlock capabilities deemed impossible by framework constraints. To achieve this, we must adopt Quake 3's engineering culture: **The Courage to Cut**. If a legacy system (like the entangled `G_No` hierarchy) threatens the purity of our rollback netcode, we must aggressively pivot and rewrite rather than layer it with wrappers.

---

## 2. Reliability & Rigor: Rollback State Management & Determinism

Fighting game netcode lives and dies by its algorithmic efficiency, strict determinism, and execution safety.

### The Gap: Full-State Snapshotting vs. The Unified Event Queue
- **Current Deficit:** `GameState_Save()` and `GameState_Load()` perform full struct serialization. Input reading is decentralized.
- **SOTA Standard:** Quake 3 centralized all inputs into a **Unified Event Queue** (a circular ring buffer with bitmask wrapping). All state histories are derived from this sequence. Furthermore, modern implementations combine this with **Incremental Snapshotting** (delta compression via **dirty flags**). 
- **The Risk:** Brute-forcing a full 22KB memory copy across 5 rollback frames destroys the CPU cache. Furthermore, without a unified input queue, we cannot reliably reproduce non-deterministic "Heisenbugs."
- **Architecture Goal:** 
  1. Implement a unified $O(1)$ Event Queue that drops the oldest event on overflow to guarantee memory stability.
  2. Implement **Event Journaling**: Treat real-time matches as a batch process, allowing the engine to reliably replay any network desync offline.
  3. Formally segment `GameState` arenas and implement bitmask change-tracking for incremental memory snapshots.

### The Gap: Floating-Point Drift & Silent Failures
- **Current Deficit:** The engine relies on pointers being sanitized before rudimentary checksum loops. Uninitialized pointers often lead to silent memory corruption instead of immediate failures.
- **SOTA Standard:** DOOM achieved massive predictability by enforcing **Fixed-Point Math & Bit Shifting**, completely eliminating float divergence. Quake 3 introduced **Defensive Sentinels**—initializing critical boundaries (like FFI function pointers) to `-1` (0xFFFFFFFF) to force an immediate, loud hardware crash on misuse.
- **Architecture Goal:** 
  1. Eliminate floating-point math in the core loop in favor of fixed-point/bit-shifting bounds formats (`[x << 16]`).
  2. Adopt the **Fail Fast** principle: Fill uninitialized rollback arenas and interface pointers with Sentinels to prevent silent desyncs. Replace loop checksums with continuous `xxHash3` block validation.

---

## 3. Philosophy & Elegance: Engine Decoupling & Virtualization

Systems must naturally solve their own problems through strict interface boundaries and data-oriented structures.

### The Gap: Subsystem Entanglement & Cross-Boundary Risk
- **Current Deficit:** The modern `AppMode` wrapper simply hijacks the 1999 static C-arrays (`G_No`) without true isolation. Furthermore, bridging legacy CPS3 logic and SDL wrappers introduces risky stack-calling assumptions.
- **SOTA Standard:** DOOM initialized highly modular subsystems sequentially (`V_`, `M_`, `Z_`) and forced strict `I_` prefix isolation for OS bindings. Quake 3 enforced the **`cdecl` Calling Convention** across boundaries to ensure exact stack cleanup. Instead of complex Entity Component Systems (ECS), 1v1 fighting engines are mathematically purer as pure **Data-Oriented Design (DOD)** arrays wrapped in Finite State Machines (FSMs).
- **Architecture Goal:** 
  1. Enforce strict `cdecl` contracts and explicit `I_` style port boundaries. The CPS3 core must never execute a direct OS/SDL call.
  2. Leverage **Preprocessor-Driven Environments** (`#ifdef`) to orchestrate execution contexts (e.g., headless simulation server) at build-time, completely unified under one codebase.
  3. Detangle `game.c`, deprecating `G_No` jump tables in favor of strict FSM interfaces.

---

## 4. Data-Driven Design & Pre-Computation

SOTA game engines do not calculate at runtime what they can calculate at compile time.

### The Gap: The WAD Philosophy & Run-Time Processing
- **Current Deficit:** Gameplay logic, hitboxes (`cuix`), and frame data (`exec_tm`) remain painfully embedded within C ROM arrays.
- **SOTA Standard:** DOOM relegated 89% of its RAM to data and executed heavily decoupled atomic lumps (WADs). Quake 3 took pre-computation further with the **Area Awareness System (AAS)**, ripping heavy 3D calculations into the build phase.
- **Architecture Goal:** 
  1. Extract `waza_work` and collision maps from C into fast-loading binary flatbuffers. 
  2. **Aggressive Short-Circuiting (Zero Overdraw):** Mirroring DOOM's 1D Clipping Arrays, implement $O(1)$ rollback phase-bypasses. Instantly exit out of audio/rendering logic blocks during frame catch-ups.
  3. Pre-compute look-up spatial grids in the `Init_Task` to guarantee the rollback loop only performs $O(1)$ array lookups.

### The Gap: AI Design (Fuzzy Logic vs Neural Overhead)
- **Current Deficit:** Combat dummies in training mode (`CPU_Rec`) rely on brittle, hardcoded C execution logic.
- **SOTA Standard:** Quake 3 dropped expensive ML in favor of highly parameterized **Fuzzy Logic** systems tuned by automated **Genetic Algorithms** over thousands of simulated bot battles.
- **Architecture Goal:** Convert training dummies and automated CPU opponents to Fuzzy Logic decision matrices. In subsequent iterations, use headless network simulation pipelines to run genetic algorithms overnight, naturally breeding the most balanced difficulty thresholds.

---

## 5. User Impact & Modding: Community Extensibility

The greatest software legacy of id Software wasn't simply open-sourcing code—it was building an architecture that treated the player as a co-developer.

### The Gap: Virtual Machine Sandboxing & Secure Lua Endpoints
- **Current Deficit:** While RmlUi abstracts UI, we have no secure mechanism to let the community script the engine. Standard Lua bindings risk malicious actors writing into netplay memory to automate cheats (auto-blocking).
- **SOTA Standard:** Quake 3 resolved cross-boundary execution via the **Quake Virtual Machine (QVM)**. Mod code was legally sandboxed into bytecode. It could not touch engine memory; it had to issue sequential **numeric Syscall dispatches** to explicitly defined engine endpoints.
- **Architecture Goal:** 
  1. Create a **Hot-Reloadable Mod Directory** for RmlUi assets to allow custom stream overlays and tools.
  2. Implement a strict **Syscall Dispatcher API** for Lua bindings. Community scripts can only request data vertically via explicit endpoints. Read-only limits are hard-enforced.
  3. Convert binary dummy recording strings into human-readable text sequences (`[6] > [2] > [3] + P_FIERCE`). Coupled with the open RmlUi, this API allows the community to author its own training tools, cementing the 3sxtra engine as an unparalleled research ecosystem.
