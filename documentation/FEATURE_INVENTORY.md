# Feature inventory

| Feature | Module | Status | Dependencies | Testing status |
|---|---|---|---|---|
| Fixed-size pool | `src/fixed_size_pool.cpp` | Implemented | `MemoryPool` | Tested |
| Variable-size pool | `src/variable_size_pool.cpp` | Implemented | `MemoryPool` | Tested |
| First-fit allocation | `src/first_fit.cpp` | Implemented | `MemoryPool` | Tested |
| Best-fit allocation | `src/best_fit.cpp` | Implemented | `MemoryPool` | Tested |
| Pool metadata | `src/memory_pool.cpp` | Implemented | None | Tested |
| Fragmentation metrics | `src/memory_pool.cpp` | Implemented | `MemoryPool` | Tested |
| Benchmarking | `src/benchmark.cpp` | Implemented | `MemoryPool` | Tested |
| Experiment metrics and latency percentiles | `src/experiment_metrics.cpp`, API | Implemented | Actual allocator execution | Unit and API-tested |
| Experiment detail/history | `frontend/src/App.jsx`, API | Implemented | Persisted records | Browser smoke-tested |
| Side-by-side comparison | `frontend/src/experimentDashboard.js`, API | Implemented | Saved experiment records | Unit and browser-tested |
| CSV export / analysis | `analysis/*.py`, experiment export API | Implemented | Python libs / local backend | Export formats and CSV shape live-tested |
| React dashboard | `frontend/src/*.jsx` | Implemented | React/Vite | Tests/build passed; browser exercised with live backend |
| Persistent experiment history | `src/experiment_store.cpp`, API | Implemented | Local filesystem | Round-trip, API, and backend restart verified |
| Configurable pool initialization | `POST /api/pool/initialize`, dashboard | Implemented | `MemoryPool` | Valid/invalid config and fixed-block state live-tested |
| Seeded random workloads | `POST /api/workload/random`, dashboard | Implemented with independent algorithm/mode selection | `MemoryPool` | UI payload/configuration tests added; C++ build/live API verification pending |
| Step/pause/resume workload controls | API and dashboard | Implemented (single active process-local session) | `src/api_server.cpp`, `frontend/src/App.jsx` | Runtime verified; CTest 1/1; frontend 16/16 |
| AI-agent orchestration/dashboard | UI and backend | Not implemented | Agent runtime unavailable | Not tested |
| Sanitizer validation | CMake instrumented build | Implemented for tests | ASan/UBSan | CTest passed with both sanitizers; Valgrind unavailable |
| Workload CSV validation/import | `frontend/src/workloadCsv.js`, editor | Implemented in supported UI import path | Browser File API | 7 focused parser tests; 1 MiB cap |
| Direct raw-CSV API request parsing | `src/api_server.cpp` | Partial | C++ lightweight parser | Not the supported browser import path; no direct raw-CSV endpoint contract |
