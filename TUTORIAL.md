# Sniper End-to-End Tutorial

This tutorial walks you through recording a trace of a sample application and simulating it using the Sniper native CLI.

## 1. Compile the Sample
First, compile the matrix multiply sample:
```bash
gcc -O2 samples/matmul.c -o samples/matmul
```

## 2. Record and Simulate
Use the native `sim` subcommand to record a trace (using Intel SDE) and simulate it on the default Nehalem-like core model:
```bash
./build/sniper sim -- ./samples/matmul
```

## 3. Analyze the Results
The simulation will generate several files in your current directory (or the directory specified by `--general/output_dir`).

- **`sim.out`:** The primary human-readable summary of the simulation results.
- **`sim.stats.sqlite3`:** A structured database containing all performance metrics.
- **`trace.app0.th0.sift`:** The raw instruction trace (can be deleted to save space after simulation).

### Key Metrics to Look For in `sim.out`:
- **IPC (Instructions Per Cycle):** Overall throughput.
- **Cache Miss Rates:** Look for `L1-D`, `L2`, and `L3` miss rates to understand memory bottlenecks.
- **CPI Stack:** A breakdown of where cycles were spent (e.g., waiting for memory, branch mispredictions).

## 4. Custom Configuration
You can override configuration options directly from the CLI:
```bash
./build/sniper sim -g --perf_model/l2_cache/cache_size=512 -- ./samples/matmul
```
Or by including a custom `.cfg` file:
```bash
./build/sniper sim -c config/gainestown.cfg -- ./samples/matmul
```
