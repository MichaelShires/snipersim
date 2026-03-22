# Performance Model Subsystem

This directory contains the performance models that dictate simulation timing.

## Components

- **PerformanceModel:** Base class for all core performance models.
- **Instruction:** Representation of an instruction in the performance model.
- **DynamicInstruction:** Instance of an instruction being executed with runtime information.
- **BranchPredictor:** Models for branch direction and target prediction.
- **QueueModel:** Models for contention and queuing delay in shared resources.

## Performance Model Types

- **OneIPC:** Simple model assuming one instruction per cycle.
- **Interval:** Fast, approximate out-of-order model.
- **ROB (Reorder Buffer):** Detailed cycle-level out-of-order execution model.

## Key Files

- `performance_model.cc/h`: Core performance modeling logic.
- `branch_predictor.cc/h`: Branch prediction infrastructure.
- `dynamic_instruction.cc/h`: Per-instruction execution state.
