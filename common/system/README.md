# System Management Subsystem

This directory contains high-level managers that orchestrate the simulator components.

## Major Managers

- **Simulator:** The top-level singleton (transitioning to dependency injection) that owns all other managers.
- **ThreadManager:** Manages simulated threads, their lifecycle, and mapping to cores.
- **CoreManager:** Manages the collection of Core objects.
- **DvfsManager:** Dynamic Voltage and Frequency Scaling controller.
- **HooksManager:** Callback system for extending simulator behavior via Python or C++.
- **MagicServer:** Handles application-to-simulator communication (Magic Instructions).
- **SyscallServer:** Models system calls and provides emulation where necessary.
- **SyncServer:** Models synchronization primitives (mutexes, conditions, barriers).

## Key Files

- `simulator.cc/h`: Main entry point and initialization.
- `thread_manager.cc/h`: Scheduling and thread state tracking.
- `core_manager.cc/h`: Core instance management.
- `magic_server.cc/h`: ROI and frequency control logic.
