# Functionality documentation

This document reflects the implementation present in the current source tree.

## MemoryPool

### `MemoryPool::MemoryPool(size_t capacity, size_t block_size, Strategy strategy)`
- Source file: `src/memory_pool.cpp`
- Purpose: Reserve a memory region and configure the allocator.
- Input: pool size, fixed block size, and chosen strategy.
- Output: initialized allocator instance.
- Data structure: `std::vector<uint8_t>` backing store plus allocation metadata blocks.
- Error handling: rejects zero-capacity initialization.
- Limitations: single-threaded and non-resizable.

### `MemoryPool::initialize()`
- Source file: `src/memory_pool.cpp`
- Purpose: initialize all metadata and create the initial free block.
- Output: true on success.

### `MemoryPool::allocate(size_t requested_size, uint64_t allocation_id)`
- Source file: `src/memory_pool.cpp`
- Purpose: allocate bytes using the active strategy.
- Working principle: choose a sufficiently large free block, split it if possible, and mark the region as allocated.
- Error handling: returns `nullptr` when there is no sufficient free block.

### `MemoryPool::deallocate(void* ptr)`
- Source file: `src/memory_pool.cpp`
- Purpose: release an allocation and merge adjacent free blocks.
- Error handling: returns `false` if the pointer is invalid or already released.

### `MemoryPool::stats()`
- Source file: `src/memory_pool.cpp`
- Purpose: compute current utilization, fragmentation, and allocation metrics.
- Limitation: metric values are based on block metadata rather than OS-level counters.

### `MemoryPool::to_json()`
- Source file: `src/memory_pool.cpp`
- Purpose: serialize the allocator state to JSON for frontend display.

## FixedSizePool

### `FixedSizePool::allocate()`
- Source file: `src/fixed_size_pool.cpp`
- Purpose: allocate a block of equal size from the pool.
- Behavior: uses fixed-size allocation on a fixed-size pool.

## VariableSizePool

### `VariableSizePool::allocate(size_t requested_size, uint64_t allocation_id)`
- Source file: `src/variable_size_pool.cpp`
- Purpose: request variable-sized allocations using the configured strategy.

## Benchmarking

### `benchmark_pool(...)`
- Source file: `src/benchmark.cpp`
- Purpose: run actual timing experiments with real allocations and deallocations.

## Experiment history and comparison

- `POST /api/pool/initialize` validates and configures a pool capacity (up to
  64 MiB), allocation strategy, and fixed block size when applicable.
- `POST /api/workload/execute` executes a custom operation sequence using
  actual allocator calls. Zero-size allocations fail; fixed-size allocations
  smaller than their block consume the full block, and larger requests fail.
  Allocation operations without an explicit ID receive deterministic
  one-based IDs by allocation order; an ID already active in the same
  workload is rejected rather than shadowing the live allocation.
- The workload editor's CSV import requires `operation,size,allocation_id`,
  accepts quoted/escaped CSV fields, validates every row and allocation
  reference, reports row-specific failures, and limits files to 1 MiB. It
  submits validated operations to the backend as JSON. A downloadable example
  template is available beside the editor.
- `POST /api/workload/random` generates a seeded sequence using a probe
  `MemoryPool`; generated free operations reference only allocations that
  succeeded in that generation pass. The experiment is then executed and
  measured from a fresh pool using the same sequence and seed.
- `POST /api/benchmark` measures allocation/deallocation call latency and
  overall benchmark elapsed time separately. Set `fixed_block_size` to
  configure fixed-size comparisons. Benchmark requests are bounded to 100,000
  allocation samples per strategy. Overall elapsed time includes backend-side
  workload and measurement overhead, but never frontend rendering/animation
  time. Peak utilization is derived from peak allocated bytes; final
  utilization is reported separately. P95/P99 use linear interpolation at
  rank `(n - 1) * p`.
- `GET /api/experiments` returns compact summaries used by the history table.
- `GET /api/experiments/detail?id=...` returns the saved configuration, workload
  distribution, allocation/deallocation metrics, memory and fragmentation
  snapshots, operation log, errors, and final pool state.
- `POST /api/experiments/compare` compares selected saved records and returns
  the measurements plus a same-conditions flag. It checks pool capacity,
  allocation mode, fixed block size, operation sequence, and random seed.
- The frontend compares each measurement to the first selected record. A
  difference is presented as a relative change, not a general claim that one
  allocator strategy is always better.
- Comparison memory and latency values retain the units represented by their
  backend metric fields; allocation success/failure rates are fractions
  converted to percentages in the UI.
- Experiment memory snapshots separate the active workload's requested bytes
  from aligned or fixed-size pool consumption. Peak use is retained across
  later frees, and adjacent free blocks are merged in the canonical block list
  before fragmentation values are reported.
- Experiment history is append-only and stored in a versioned local file.
  `MEMORY_POOL_EXPERIMENTS_PATH` selects the file; the default is
  `data/experiments.bin`. Records survive backend restarts. The single-process
  store rejects corrupt/truncated records instead of silently dropping them.
- `GET /api/experiments/export?id=...&format=...` returns CSV text for
  `full`, `summary`, `operations`, `configuration`, `memory`,
  `fragmentation`, or `benchmarks`. The frontend experiment detail view
  exposes these export choices. `full` is one row per operation with experiment
  configuration, operation timing/result, memory/latency/fragmentation
  metrics, final allocator state, and recorded errors; the other formats are
  narrower tables.

## Metric definitions and units

- Capacity, requested bytes, allocated bytes, free bytes, peak usage, block
  sizes, and offsets are bytes. Requested memory is the sum of active
  allocations' original request sizes; allocated memory is actual pool
  consumption after alignment or fixed-block sizing. Workload total-requested
  memory includes every allocation request, including failed requests.
- Current utilization is `allocated_bytes / capacity_bytes * 100`. Peak
  utilization uses the allocator's recorded peak consumed bytes. Experiment
  average utilization is the arithmetic mean of allocator utilization sampled
  after each workload operation; an empty workload has zero average.
- Allocation/deallocation success and failure rates are their respective
  outcomes divided by the number of requests; zero requests produce zero
  rates. Total latency is the sum of measured call latencies and average
  latency is total divided by requests. Median and percentiles are calculated
  from sorted per-request millisecond samples using linear interpolation at
  zero-based rank `(n - 1) * p`; minimum/maximum are sample extrema. Empty
  samples produce zero-valued summaries.
- The backend measures operation and workload elapsed time with
  `std::chrono::steady_clock`. Workload elapsed time is separate from frontend
  rendering or animation. Successful allocation/deallocation latency samples
  bracket the corresponding allocator call; invalid-free failures measure the
  backend validation path. Benchmark elapsed time includes benchmark-loop and
  measurement overhead and is not an OS-level performance guarantee.
- Internal fragmentation bytes are active pool bytes consumed minus active
  requested bytes, clamped at zero. Its percentage is waste divided by active
  requested bytes, with zero when there are no active requested bytes.
- External fragmentation percentage is
  `(1 - largest_free_block / total_free_bytes) * 100`, clamped at zero; it is
  zero when no free bytes remain. Free-block count, largest size, and average
  free-block size are read from the allocator's canonical block state.
- Contiguous-memory failures, splits, and coalesces are allocator counters.
  Workload allocation-size distribution counts requested sizes for all
  allocation operations, whether they succeeded or failed. Peak active
  allocations is the maximum live allocation count observed during execution.
- Benchmark allocation success/failure rates are fractions in the stored
  record and are rendered as percentages by the UI. Utilization and
  fragmentation metrics in saved experiment and benchmark comparison rows
  are percentage values (0–100); the low-level pool-state API exposes
  utilization as a 0–1 fraction for the live dashboard to convert.
