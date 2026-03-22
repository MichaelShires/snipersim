# Core Subsystem

This directory contains the architectural models for individual processor cores in Sniper.

## Components

- **Core:** The main core class that manages the pipeline and interacts with other subsystems.
- **TopologyInfo:** Manages the mapping of logical core IDs to physical topology (packages, cores, hardware contexts).
- **BbvCount:** Basic Block Vector tracking for phase analysis and sampling.
- **CheetahManager:** Interface to the Cheetah cache simulator for rapid cache analysis.

## Key Files

- `core.cc/h`: Monolithic class representing a processor core.
- `topology_info.cc/h`: Topology discovery and management.
- `bbv_count.cc/h`: Instruction distribution and phase detection.
