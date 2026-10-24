# Limitations

## Implemented limitations

### Single-threaded allocator
- What: the allocator is not thread-safe.
- Why: no locking or synchronization primitives are used.
- Effect: concurrent allocations can corrupt internal metadata.
- Impact on experiments: experiments are run sequentially and are not representative of multithreaded workloads.

### No in-place pool resizing
- What: an initialized pool cannot grow or shrink in place.
- Why: the backing storage is a single `std::vector` with fixed capacity.
- Effect: users must explicitly reinitialize the pool to change capacity;
  doing so discards its active allocations.

### User-space allocator only
- What: this project allocates from a reserved buffer inside the process, not from the OS virtual memory manager.
- Why: it is implemented in C++ user space.
- Effect: it does not replace the kernel allocator or page allocator.

### Alignment and block granularity
- What: allocations are aligned to 8-byte boundaries.
- Why: the implementation rounds sizes to 8-byte multiples.
- Effect: small requests incur internal fragmentation.

### Fragmentation metric limitations
- What: external fragmentation is approximated as `1 - (largest_free_block / total_free_memory)`.
- Why: the metric is a simplified proxy for fragmentation.
- Effect: it does not fully capture fragmentation under arbitrary allocation patterns.

### Benchmark limitations
- What: benchmark numbers are sensitive to machine load and compiler optimization.
- Why: timings are taken with `steady_clock` inside the same process.
- Effect: results are suitable for relative comparison, not absolute OS performance claims.

## Theoretical limitations

- This project does not implement page tables, virtual memory management, or kernel-level memory reclamation.
- It does not support concurrent allocation between threads.
- It does not emulate NUMA or heap fragmentation at the OS allocator level.

## Current implementation limitations

- Experiment persistence uses a versioned append-only local file, not SQLite.
  It is intended for one backend process; concurrent writers, transactional
  updates, and explicit `fsync` durability are not provided. A corrupt or
  truncated history file stops startup with an error and requires manual
  recovery from a backup.
- Workload execution is synchronous. Step, pause, and resume endpoints are
  not implemented.
- AI-agent orchestration and an agent execution dashboard are not integrated.
  The frontend does not fabricate agent runs.
- Pool initialization is capped at 64 MiB and generated random workloads at
  10,000 operations.
- Valgrind was unavailable in the audit environment. The C++ regression suite
  passed under AddressSanitizer and UndefinedBehaviorSanitizer; that does not
  replace a Valgrind run or broader fuzz testing.
- Frontend workload CSV import validates the required three-column header,
  quoted fields, row widths, operation types, sizes, and scheduled allocation
  references, and is capped at 1 MiB. Direct raw-CSV submission to the backend
  is not the supported import path; clients should validate and submit the
  resulting JSON workload.
