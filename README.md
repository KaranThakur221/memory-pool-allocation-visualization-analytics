# Custom Memory Pool Allocator

This project implements a working memory pool allocator in C++ with fixed-size and variable-size allocation modes, first-fit and best-fit selection strategies, statistics, benchmarking, CSV export, plot generation, and a lightweight React frontend.

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
cd frontend && npm ci && npm test && npm run build
```

## Run the application

```bash
./build/memory_pool_server 8080
```

In a second terminal:

```bash
cd frontend
npm run dev -- --host 0.0.0.0
```

Open the Vite URL printed by the frontend. The frontend proxies `/api` to
`http://localhost:8080` by default; set `MEMORY_POOL_API_URL` before starting
Vite to use another local backend URL.

The standalone C++ demonstration remains available with:

```bash
./build/memory_pool_demo
```

## Project goals

- Demonstrate actual memory management primitives in user space.
- Compare fixed-size and variable-size pools.
- Measure allocation and deallocation latency.
- Track fragmentation, utilization, and failures.
- Export experiment data and generate charts from real CSV logs.

## Unit testing and TDD workflow

The project follows a small TDD loop:

1. Define the expected behavior as a failing unit test.
2. Implement the allocator behavior needed to satisfy the test.
3. Run the targeted C++ test suite.
4. Confirm the fix and keep the regression tests intact.

The current regression suite checks:

- pool initialization, fixed/variable modes, first-fit/best-fit selection,
  requested-versus-consumed memory, and failure handling
- allocation/deallocation, reset, invalid/repeated/foreign pointer handling,
  splitting and coalescing, and deterministic allocator invariants
- benchmark timing and utilization, experiment metrics/comparison, CSV shape,
  experiment persistence round-trip, and unique experiment IDs

Run tests with:

```bash
ctest --test-dir build --output-on-failure
cd frontend && npm test
```

## Experiment history and comparison

The dashboard history is populated from backend experiment records. Select an
experiment to inspect its configuration, measured workload and allocator
metrics, fragmentation, individual operation results, and recorded errors.
Choose up to four saved records to compare. Comparison deltas use the first
selected record as their baseline; the API checks pool capacity, allocation
mode, fixed block size, operation sequence, and random seed, and flags
differences rather than treating mismatched runs as a controlled comparison.
The saved-record comparison includes execution and latency percentiles,
allocation/deallocation outcomes, memory utilization, fragmentation, free
blocks, splitting, and coalescing measurements.

Experiments are appended to the versioned local file `data/experiments.bin`
and loaded when the backend starts. Set `MEMORY_POOL_EXPERIMENTS_PATH` to use a
different file. A malformed or truncated history file causes an explicit
startup error; it is not silently replaced. The store is intended for one
backend process and is not a multi-process database.

The API supports pool initialization/configuration, manual allocation and
deallocation, custom workload execution, seeded random workload generation,
benchmark comparisons, experiment history/detail/comparison, and CSV export.
Pool capacity is limited to 64 MiB by the initialization endpoint; random
workloads are limited to 10,000 operations.
Experiment detail supports CSV downloads for complete results, operation
logs, configuration, memory statistics, fragmentation, benchmark results,
and a compact summary. Workload CSV import validates the exact
`operation,size,allocation_id` header and rejects malformed rows and invalid
allocation references before a workload can be run. Download a valid template
from the workload editor; imports larger than 1 MiB are rejected.

## Audit record and known gaps

See [DEVELOPMENT_LOG.md](./DEVELOPMENT_LOG.md) for timestamped, evidence-based
audit progress and [documentation/LIMITATIONS.md](./documentation/LIMITATIONS.md)
for verified limitations. Workload execution currently runs an entire
sequence; step/pause/resume controls and an AI-agent orchestration dashboard
are not implemented. The security and memory-safety test coverage is not a
substitute for a configured AI agent or an external security assessment.
