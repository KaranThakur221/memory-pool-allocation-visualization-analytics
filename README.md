# Custom Memory Pool Allocator

This project implements a working memory pool allocator in C++ with fixed-size and variable-size allocation modes, first-fit and best-fit selection strategies, statistics, benchmarking, CSV export, plot generation, and a lightweight React frontend.

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
cd frontend && npm ci && npm test && npm run build
```

## Run the complete dashboard application

The dashboard consists of two services:

1. `memory_pool_server`: the C++ allocator and HTTP API.
2. Vite: the React development server and `/api` proxy.

The commands below assume Linux/macOS and that the repository root is the
current directory. You can either use the one-command launcher above or run
the services manually in separate terminals.

### Quick start: one command

After the backend has been built and frontend dependencies have been
installed, start both services together from the repository root:

```bash
./run_dashboard.sh
```

This starts the C++ backend on port `8080` and the Vite dashboard on port
`3000`, then prints the dashboard URL. Press `Ctrl+C` once to stop both
services. The launcher also cleans up the remaining service if either one
exits unexpectedly.

Optional environment variables:

```bash
MEMORY_POOL_BACKEND_PORT=8081 \
MEMORY_POOL_FRONTEND_PORT=3001 \
MEMORY_POOL_EXPERIMENTS_PATH=/tmp/memory-pool-experiments.bin \
./run_dashboard.sh
```

If the project has not been prepared yet, run the build and dependency
installation steps below once before using the launcher.

### 1. Install prerequisites

Install:

- C++17 compiler
- CMake
- GNU Make or another CMake build tool
- Node.js and npm

Build the backend and install frontend dependencies:

```bash
cmake -S . -B build
cmake --build build
cd frontend
npm ci
cd ..
```

If `frontend/package-lock.json` is unavailable, use `npm install` instead of
`npm ci`.

### 2. Start the C++ backend

From the repository root, open **Terminal 1**:

```bash
./build/memory_pool_server 8080
```

The API is now available at `http://localhost:8080`. Keep this terminal open.
The default experiment history is stored in `data/experiments.bin`. To use a
separate history file, set the path before starting the backend:

```bash
MEMORY_POOL_EXPERIMENTS_PATH=/tmp/memory-pool-experiments.bin \
  ./build/memory_pool_server 8080
```

The history file should be used by only one backend process at a time.

### 3. Start the React/Vite frontend

Open **Terminal 2** in the repository root:

```bash
cd frontend
npm run dev -- --host 0.0.0.0
```

Open the URL printed by Vite, normally:

```text
http://localhost:3000/
```

The frontend proxies every `/api` request to `http://localhost:8080` by
default. If the backend is running on another port, start Vite with the
matching API URL:

```bash
MEMORY_POOL_API_URL=http://localhost:8081 \
  npm run dev -- --host 0.0.0.0
```

For a different frontend port:

```bash
npm run dev -- --host 0.0.0.0 --port 3001
```

### 4. Verify that all services are running

With both terminals running, check the backend directly:

```bash
curl http://localhost:8080/api/pool/state
```

A successful response contains `"success": true` and the current pool
state. Then open the Vite URL in a browser. The dashboard should show
**Connected to allocator backend** and display the live memory pool.

### 5. Run a production frontend preview

For a production-style local run, build the frontend first:

```bash
cd frontend
npm run build
npm run preview -- --host 0.0.0.0
```

The preview server serves the compiled frontend, but API proxy behavior is
intended for the Vite development server. Keep the backend running and use
the development server when testing the full proxied dashboard locally.

### 6. Stop the application

Press `Ctrl+C` in the backend and frontend terminals. Stop the backend first
or the frontend first; no allocator data is deleted by stopping either
service. Experiments already persisted in `data/experiments.bin` remain
available the next time the backend starts with the same history path.

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
benchmark comparisons, experiment history/detail/comparison, and CSV export.+
The seeded workload editor independently selects First-Fit or Best-Fit and
fixed-size or variable-size allocation. Fixed-size mode requires a block size
at least as large as the configured maximum request. The backend records the
actual algorithm, mode, block size, seed, and workload settings with the
experiment; matching seeds/settings produce the same operation sequence
across compatible allocation configurations.
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
sequence through the original endpoint, and the dashboard also provides
backend-owned step/pause/resume/reset controls for one process-local session.
An AI-agent orchestration dashboard is not implemented. The security and
memory-safety test coverage is not a substitute for a configured AI agent or
an external security assessment.
