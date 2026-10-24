# Dashboard Quick Reference

## What the project does

The C++ backend manages a byte pool using First-Fit or Best-Fit placement and fixed-size or variable-size allocation. The React dashboard configures the pool, runs real workloads, measures results, stores experiments, and compares saved records.

## Main sections

| Section | Use |
|---|---|
| Pool configuration | Set capacity, algorithm, and allocation mode |
| Memory visualization | See allocated/free blocks, offsets, and IDs |
| Statistics | Read capacity, usage, utilization, fragmentation, and counters |
| Workload editor | Run JSON/CSV sequences and export results |
| Seeded random workload | Generate reproducible workloads with selectable configuration |
| Stepwise session | Start, step, pause, resume, reset, and inspect progress |
| Benchmark comparison | Measure actual allocation/deallocation timings |
| Experiment history | Inspect, export, compare, or clear saved records |
| Comparison analytics | Plot actual backend metrics for selected experiments |

## Important controls

- **Initialize pool:** `POST /api/pool/initialize`
- **Run workload:** `POST /api/workload/execute`
- **Run random workload:** `POST /api/workload/random`
- **Run benchmark:** `POST /api/benchmark`
- **Start stepwise:** `POST /api/workload/session/start`
- **Step:** `POST /api/workload/session/step`
- **Refresh history:** `GET /api/experiments`
- **Compare:** `POST /api/experiments/compare`
- **Export:** `/api/experiments/export`
- **Clear history:** `DELETE /api/experiments`

## Metric meanings

- **Allocated memory:** reserved bytes currently in use.
- **Requested memory:** payload bytes requested.
- **Utilization:** allocated bytes divided by pool capacity.
- **Internal fragmentation:** reserved bytes beyond requested bytes.
- **External fragmentation:** separated free space; inspect largest free block.
- **P95/P99:** latency at or below which 95%/99% of measured samples fall.
- **Success rate:** successful allocation requests divided by allocation requests.
- **Free block count:** number of separate free regions.

## Recommended presentation flow

1. Initialize a pool.
2. Explain First-Fit/Best-Fit and fixed/variable mode as separate choices.
3. Run a small workload and show splitting/coalescing.
4. Run the same settings with the other strategy.
5. Open history and inspect actual configuration and metrics.
6. Select both experiments and compare actual backend results.
7. Export the comparison CSV.

## Fair comparisons

Keep capacity, allocation mode, fixed block size, workload sequence, random settings, and seed equal. Change only the algorithm when comparing First-Fit with Best-Fit.

## Important limitations

- The original workload endpoint is synchronous.
- Stepwise sessions are process-local and allow one active session.
- Refresh is request-driven; no WebSocket/SSE real-time stream exists.
- Persistence is a versioned append-only local file for one backend process.
- AI-agent orchestration is not implemented.
