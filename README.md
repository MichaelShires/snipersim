# The Sniper Multi-Core Simulator

Sniper is a next-generation parallel, high-speed, and accurate x86 simulator. It is designed for multi-core studies, providing high performance and detailed timing models (Interval, ROB).

## 📋 Prerequisites

Sniper requires a modern Linux environment (Ubuntu 22.04+ recommended).

### System Packages (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install build-essential cmake git python3 python3-yaml \
                 libsqlite3-dev libcurl4-openssl-dev zlib1g-dev \
                 pkg-config wget xz-utils
```

### Compiler Requirements
- **G++ 11+** or **Clang 15+**.
- **Intel APX** features specifically require Clang 15 or newer for building micro-benchmarks.

## 🚀 Quick Start (Modern Native Workflow)

### 1. Bootstrap the Environment
```bash
./bootstrap.sh
```
This fetches all dependencies (Intel SDE, Pin, XED), builds the simulator core, and prepares the workspace.

### 2. Verify with the Doctor
```bash
./build/sniper doctor
```
Ensure all required tools (SDE, Clang) are correctly configured.

### 3. Run a Full Simulation
Record and simulate a workload in one command:
```bash
./build/sniper sim -- ./apx_test_bin
```

### 4. Fetch Dependencies Individually
```bash
./build/sniper fetch sde
./build/sniper fetch pin
```

## 🛠 Building from Source
If you prefer building manually via CMake:
```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

## 📚 Further Documentation
- **[DEVELOPER.md](DEVELOPER.md):** Architectural guide and hot-path optimization.
- **[GEMINI.md](GEMINI.md):** Modernization roadmap and feature verification status.
- **`config/`:** Configuration files for core models, cache hierarchies, and uncore settings.

## 🤝 Contributions
Please refer to the Belgian research group at Ghent University for original development history. This modernization fork focuses on **Intel APX** support and native orchestration.

## ⚠️ Troubleshooting

### GLIBCXX ABI Mismatch
Sniper is built with `_GLIBCXX_USE_CXX11_ABI=0` for compatibility with Intel Pin and SDE. If you encounter link errors related to `std::string` or `std::vector` when linking against system libraries, ensure those libraries are compatible with the old ABI or use the provided `bootstrap.sh` which handles the environment.

### Missing SIFT Recorder
If simulation fails with "Unable to resolve tool path", run:
```bash
./build/sniper fetch all
```
This ensures the SDE/Pin SIFT recorder tools are locally available in the project root.


## 🔬 Research Workflows: LLC Miss Dependency Tracking (Fusion Edges)

Sniper includes a native, high-performance capability to track instruction dependencies leading up to Last Level Cache (LLC) misses. This allows you to generate back-slice dependency graphs and find the most commonly executed producer-consumer instruction pairs (fusion edges) prior to long latency loads, enabling research into macro-fusion.

The C++ tracer uses O(1) producer lookup maps, meaning it can guarantee a deep tree search (up to 5 instructions deep within a 4096 instruction rolling window) **without slowing down the simulation**.

### 1. Generating Fusion Edges for a Simpoint
To generate the fusion edge data for a specific run, simply add the `instruction_tracer` configuration flag. The simulator will automatically aggregate the graph in-memory and dump it to a CSV at the end of the run.

```bash
./build/sniper sim -c gainestown -g --instruction_tracer/type=llc -- workloads/my_simpoint
```
This will produce a file named `llc_fusion_edges.csv` in your output directory.

### 2. Merging Weighted Simpoints
If you have run multiple simpoints and need to merge their fusion edge dictionaries based on their representative weights, you can use the native `merge-llc` subcommand. 

For example, if you have two simpoints (`simpoint_A` with a weight of 0.7, and `simpoint_B` with a weight of 0.3):

```bash
./build/sniper merge-llc \
    --weights 0.7 0.3 \
    --inputs simpoint_A/llc_fusion_edges.csv simpoint_B/llc_fusion_edges.csv \
    --output final_weighted_edges.csv
```

This will parse the native CSV files, apply the weights mathematically, merge identical edges, and output a final, globally sorted list of the most valuable fusion candidates across your entire workload.
