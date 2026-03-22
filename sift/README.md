# SIFT (Sniper Instruction Fetch Trace) Interface

This directory contains the SIFT interface logic for trace-driven simulation.

## Components

- **SiftReader:** Parses `.sift` trace files and provides instruction objects to the simulator.
- **SiftWriter:** Generates `.sift` traces from a frontend (e.g., Pin, SDE).
- **SiftFormatters:** Modular objects for formatting specific SIFT records (instructions, memory, etc.).
- **zfstream:** Gzip-compressed stream support for efficient trace storage.

## Key Files

- `sift_reader.cc/h`: Trace decompression and parsing.
- `sift_writer.cc/h`: Trace generation and serialization.
- `sift_format.h`: Definition of the binary SIFT protocol.
- `sift_formatters.cc/h`: Component-specific record emitters.
