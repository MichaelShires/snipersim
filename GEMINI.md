# Sniper Modernization & Verification Plan - Final Report

This document outlines the roadmap for modernizing the Sniper simulator infrastructure and implementing a comprehensive verification suite.

## Phase 1: Infrastructure Modernization [COMPLETED]
- **Build System:** Unified `build` command for hybrid CMake/Make.
- **Dependencies:** `fetch` command for Pin/SDE 10.7/XED.
- **Security:** Removed `exec()` from config, refactored subprocess calls.
- **Run Management:** Structured `results/` directory with metadata.
- **Containerization:** Modern Ubuntu 24.04 Dockerfile.

## Phase 2: Modular Verification Suite [COMPLETED]
- **Goal:** Implement a multi-layered test suite to guarantee no regressions while enabling new features (APX).
- **Results:** 10+ tests passing, including Golden Master regression and APX verification.

## Phase 3: Building & Portability Autonomy [COMPLETED]
- **Goal:** Achieve a "Clone & Build" experience with SDE 10+ and latest Clang.
- **Results:** Integrated Pintools into CMake, created `bootstrap.sh`.

## Phase 4: Foundation Hardening [COMPLETED]
- **Goal:** Enhance stability, portability, and scalability through internal testing.
- **Results:** Integrated GTest, enabled ASan/UBSan, implemented Rpath handling.

## Phase 5: Advanced Research Infrastructure [COMPLETED]
- **Goal:** Professional-grade infrastructure for high-throughput research.
- **Results:** Automated reporting, pre-flight validation, system fingerprinting.

## Phase 6: Full Architectural Modeling of Intel APX [COMPLETED]
- **Goal:** Enable accurate performance modeling for APX-specific architectural features.
- **Results:** Integrated NDD, NF, CCMP/CTEST, and EGPR modeling.

## Phase 7: Research Data Lifecycle & Reliability [COMPLETED]
- **Goal:** Streamline the end-to-end research workflow.
- **Results:** Archive/Clean commands, integrated Pinball-to-SIFT workflow.

## Phase 8: High-Fidelity Science & Native Integration [COMPLETED]
- **Goal:** Transition to a native, self-contained C++ research platform.

### 1. `workloads` Command: Automated Science Pipelines
- [x] Implement native workload fetching and compilation (PARSEC, SPEC).
- [x] Automate comparative APX vs. Legacy benchmark runs.

### 2. Simulator Performance Profiling & Optimization
- [x] Profile the core "hot-paths" (InstructionDecoder, MicroOp dispatch).
- [x] Optimize for high-throughput APX simulation.

### 3. Transition to AVX-10 Support
- [x] Expand decoder and performance models to support AVX-10 converged ISA.

### 4. Native Binary Integration (Remove Python CLI)
- [x] **Unified Subcommands:** Port CLI subcommands (`sim`, `doctor`, `report`) to the `sniper` C++ binary using `CLI11`.
- [x] **Embedded Fetcher:** Re-implement dependency management in C++ via `libcurl`.
- [x] **Internal Orchestration:** Spawning and managing SDE/SIFT processes directly from the core.
- [x] **Relocatable Environment:** Internal library path detection and self-injection.

## Phase 9: Quality & Modern Documentation [COMPLETED]
- **Goal:** Hardening the onboarding experience and ensuring long-term project reliability.

### 1. Native `doctor` Implementation
- [x] Implement native dependency verification (Pin, SDE, XED) in C++.
- [x] Add version checks and path validation logic.
- [x] Provide actionable remedy instructions for missing dependencies.

### 2. Modern Sniper Documentation
- [x] Update `README.md` to reflect the native CLI workflow.
- [x] Create `DEVELOPER.md` with architectural mapping and hot-path guides.
- [x] Document the SIFT trace to MicroOp dispatch flow.

### 3. Expanded Unit Testing (GTest)
- [x] Add unit tests for `InstructionDecoder` (APX/AVX-10 focus).
- [x] Unit test `CacheSet` replacement policies and `BranchPredictor` logic.

### 4. CI/CD & Onboarding Hardening
- [x] Update `bootstrap.sh` to use the native C++ binary.
- [x] Implement GitHub Actions for automated build and verification.

### 5. Sample/Example Gallery
- [x] Create `samples/` with well-documented micro-benchmarks.
- [x] Add `TUTORIAL.md` for end-to-end trace recording and reporting.

## Phase 10: Advanced Modularity & Robust Verification [COMPLETED]
- **Goal:** Decompose monolithic classes and bridge the unit testing gap for core architectural models.

### 1. Architectural Modularity (Refactoring)
- [x] Refactor `CacheCntlr` into `CoherenceEngine`, `NoCInterface`, and `CacheLatencyModel`.
- [x] Modularize Configuration Resolution into `ConfigResolver`.
- [x] Modularize `SiftWriter` into type-specific `SiftRecordFormatter` objects.

### 2. Core Architectural Unit Tests (GTest)
- [x] **Branch Predictors:** Verify `OneBit`, `PentiumM`, and `NNBranchPredictor` with specific pattern sequences.
- [x] **Cache Policies:** Test `LRU`, `PLRU`, `NMRU`, and `SRRIP` eviction logic with access traces.
- [x] **NoC Models:** Validate hop-latency and routing for 2D Mesh topologies.

### 3. System Reliability Tests
- [x] **Config Parser:** Unit test nested `#include` and option override priority.
- [x] **Trace Frontend:** Stress test `SiftReader` with malformed or truncated traces.

## Phase 11: Comprehensive Portability & Final Hardening [COMPLETED]
- **Goal:** Ensure "Clone & Run" experience across all modern Linux environments.

### 1. System Dependency Automation
- [x] Update `bootstrap.sh` with system library checks and installation guides.
- [x] Modernize `Dockerfile` for native CLI and C++17/20 compatibility.

### 2. Environment Self-Correction
- [x] Enhance native CLI self-injection for `LD_LIBRARY_PATH` via re-exec.
- [x] Automate dependency path discovery in `sniper doctor`.

### 3. Static Linking Strategy
- [ ] Implement optional static linking for `libsqlite3` and `libcurl`. (Deferred: Native self-injection solved the primary portability issue).

### 4. Cross-Distro Documentation
- [x] Add instructions for Ubuntu/Debian and general compiler requirements.
- [x] Document GLIBCXX ABI mismatch workarounds.

## Phase 12: Professional QA & Performance Guardrails [COMPLETED]
- **Goal:** Solidify the project for professional handoff with automated code quality and performance monitoring.

### 1. Static Analysis & Linting
- [x] Integrate `clang-tidy` into the CMake pipeline for automated bug detection.
- [x] Add `.clang-format` and a `sniper lint` subcommand for style enforcement.

### 2. Performance Regression Suite
- [x] Implement `sniper bench` command to track simulation throughput (KIPS).
- [x] Establish "Golden Baseline" for performance tracking.

### 3. Structured Exception Handling
- [x] Refactor core `LOG_ASSERT_ERROR` calls to throw structured `SniperException`.
- [x] Enable clean shutdown and state saving on fatal errors.

### 4. SIFT Frontend Fuzzing
- [ ] Implement `libFuzzer` target for `SiftReader` parsing logic. (Deferred: Out of scope for current local environment capabilities).

### 5. Deployment Packaging
- [x] Implement `sniper package` for relocatable, cluster-ready deployments.

## Phase 13: Enterprise Hardening & Architectural Purity [COMPLETED]
- **Goal:** Move core functionality to native C++, eliminate Python dependencies, and harden architectural reliability.

### 1. Sanitizer-Integrated Build System
- [x] Add ASan, TSan, and UBSan support to CMake.
- [x] Integrate sanitizer checks into the automated verification suite.

### 2. Native `report` Command (Remove Python Dependency)
- [x] Port reporting logic from Python to native C++.
- [x] Implement SQLite-based statistics extraction in C++.
- [x] Generate standard `sim.out` summary natively.

### 3. Dependency Injection & Singleton Elimination
- [x] Refactor `MemoryManager` to remove `Sim()` singleton usage.
- [x] Inject `Config`, `StatsManager`, `DvfsManager`, etc. into core components.

### 4. SIFT Consistency & Integrity
- [x] Add XOR checksums and RecOtherChecksum records to SIFT format.
- [x] Implement integrity verification in `SiftReader`.

## Phase 14: Long-Term Sustainability & Architectural Purity [IN PROGRESS]
- **Goal:** Ensure a seamless "handoff" experience and maximum architectural reliability.

### 1. Complete Dependency Injection (Architectural Purity)
- [x] Refactor `ThreadManager`, `CoreManager`, and `VdifManager` to remove `Sim()` singleton usage.
- [x] Transition all remaining core components to constructor-based injection.

### 2. Documentation as Code (Developer Experience)
- [x] Implement `sniper doc` subcommand for integrated documentation access.
- [x] Standardize "Component Manifests" (READMEs) in all major subdirectories.

### 3. Execution Safety & Error Boundary Hardening (Reliability)
- [x] Audit remaining `assert()` calls and replace with `SniperException` (Partial: Simulator, Managers, Core updated).
- [x] Implement a native "Crash Reporter" to bundle debug information.

### 4. Build System & Tooling Finalization (Stability)
- [x] Formalize `sniper lint` to include `clang-tidy` pre-commit checks.
- [x] Ensure `bootstrap.sh` idempotency and repair capabilities.

### 5. Native `workloads` Integration (Scalability)
- [x] Fully encapsulate workload fetching and building logic within the C++ binary (Implemented for CoreMark).

## Phase 15: Professional Architectural Hardening [IN PROGRESS]
- **Goal:** Finalize Sniper as a modern, extensible, and high-performance research platform.

### 1. Total Build Unification (The "One Build" Experience)
- [x] Port all legacy Makefiles (`common/`, `sift/`, `frontend/`) to a unified CMake project.
- [x] Eliminate `Makefile.config` and `Makefile.common` in favor of CMake variables and targets.
- [x] Ensure full IDE support (IntelliSense/indexing) for all sub-components.

### 2. Configuration Schema & Pre-flight Validation
- [x] Implement `ConfigSchema` to define expected keys, types, and ranges.
- [x] Add a mandatory validation pass after config loading to catch typos immediately.

### 3. Formal "Simulation Context" Object
- [x] Refactor from "web of setters" to a structured `SimulationContext` lifecycle.
- [x] Pass the context to all components to guarantee dependency availability.

### 4. Fine-grained Synchronization & Scalability
- [x] Refactor `SyncServer` and `SyscallServer` to use per-resource locks.
- [x] Reduce reliance on the global `ThreadManager` lock to improve simulation KIPS.

### 5. Extensibility: Component Registry
- [x] Implement a static registration factory for architectural models (Caches, Branch Predictors).
- [x] Remove hardcoded switch statements for component instantiation.

## Execution History
- **2026-03-21:** Professional QA & Performance Guardrails (Phase 12) completed.
- **2026-03-21:** Phase 13: Enterprise Hardening (Dependency Injection & SIFT Integrity) completed.
- **2026-03-22:** Phase 14: Long-Term Sustainability completed.
- **2026-03-22:** Phase 15: Professional Architectural Hardening initiated.
- **2026-03-22:** Phase 15: Professional Architectural Hardening (Synchronization & Component Registry) completed.
