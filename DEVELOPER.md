# Sniper Architectural Guide

This document provides a high-level overview of the Sniper simulator's internal architecture, focusing on the data flow from instruction tracing to performance modeling.

## 1. Trace Frontends (SIFT)
Sniper primarily uses the **SIFT (Sniper Instruction Fetch Tool)** interface. SIFT acts as a bridge between the functional emulator (like Intel SDE or Pin) and the timing model.

- **Recorder:** Runs as a Pintool/SDE tool (`sde_sift_recorder.so`). It captures instruction streams and writes them to `.sift` files.
- **Reader:** The Sniper core reads these files via `SiftReader` and converts them into the simulator's internal instruction format.

## 2. Instruction Decoding & Micro-Ops
The `InstructionDecoder` (found in `common/performance_model/performance_models/micro_op/`) is the heart of the frontend timing model.

1. **XED Integration:** Sniper uses the Intel XED library to decode raw x86 bytes.
2. **Micro-Op (uOp) Generation:** Each x86 instruction is decomposed into one or more `MicroOp` objects (e.g., Load, Store, Execute).
3. **Dependency Tracking:** The decoder identifies register and memory dependencies between uOps to model pipeline stalls accurately.

## 3. Core Models
Sniper supports multiple core models, with the **Interval Model** being the most common for high-speed simulation.

- **Interval Model:** Models performance by "jumping" between miss events (branch mispredictions, cache misses).
- **ROB Model:** A more detailed Out-of-Order model that simulates a Reorder Buffer for higher accuracy at lower speeds.

## 4. Memory Subsystem
The memory model is highly modular and hierarchical:
- **L1/L2/L3 Caches:** Implemented in `common/core/memory_subsystem/cache/`.
- **Coherence:** Supports MSI/MESI protocols.
- **DRAM:** Models memory controllers and DIMM latencies.

## 5. Native CLI Orchestration
The native `sniper` binary (built from `standalone/cli.cc`) manages the entire lifecycle:
1. **`sim` command:** Forks an SDE process to record a trace, then automatically starts the Sniper core to simulate that trace.
2. **Configuration:** Loads nested `.cfg` files, overriding base settings with workload-specific parameters.

## Hot-Paths for Optimization
- `InstructionDecoder::decode()`: The bottleneck for frontend-heavy simulations.
- `Cache::access()`: The most frequently called function in the simulator.
- `MicroOp::dispatch()`: The core of the timing execution loop.
