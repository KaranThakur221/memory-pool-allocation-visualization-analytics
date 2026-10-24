# MASTER PROJECT PROMPT
## Custom Memory Pool Allocator with Interactive React Dashboard, C++ Backend, AI Testing Agents, and TDD

### 1. PROJECT OVERVIEW

Build a complete, functional, interactive software project titled:

**Custom Memory Pool Allocator with Interactive Memory Visualization**

The project must demonstrate how a custom memory pool allocator manages memory internally, how different allocation algorithms behave, how memory is reused, and how fragmentation develops during allocation and deallocation.

The application must consist of:

- A C++ backend implementing the actual memory pool allocator.
- A React frontend providing an interactive memory visualization dashboard.
- A communication layer connecting the frontend to the C++ backend.
- An interactive workload simulator that accepts user-defined allocation and deallocation requests.
- Real-time memory visualization based on actual backend execution.
- Benchmarking, statistics, experiment logging, and CSV export.
- Automated AI agents responsible for TDD, development, testing, integration, memory safety, security testing, and regression testing.

**Important:** This is a software development project. Focus exclusively on building the application, its functionality, architecture, testing, and documentation. Do not generate an unrelated academic report, research paper, presentation, viva questions, or other academic deliverables.

---

# 2. PRIMARY OBJECTIVE

Develop a working memory allocator simulator that allows users to:

1. Create a memory pool with a configurable total capacity.
2. Select different memory allocation strategies.
3. Allocate memory using fixed-size and variable-size allocation.
4. Enter custom allocation and deallocation requests.
5. Execute workloads step by step or automatically.
6. Observe how the C++ allocator manages memory.
7. Visualize allocated, free, reserved, and fragmented memory blocks.
8. Understand memory reuse, splitting, coalescing, and fragmentation.
9. Compare the performance and behavior of allocation algorithms.
10. Export experiment results and memory statistics.
11. Run automated tests through specialized AI agents.
12. Verify that the frontend displays actual backend results rather than fabricated or independently simulated values.

The final application must be usable, reliable, visually polished, and technically correct.

---

# 3. TECHNOLOGY STACK

Use the following technologies.

### Backend

- C++
- Modern C++17 or C++20
- CMake
- Standard C++ libraries
- Appropriate data structures for memory management
- JSON serialization for communication
- Google Test or an equivalent C++ testing framework
- AddressSanitizer
- UndefinedBehaviorSanitizer
- Valgrind when supported by the development environment

### Frontend

- React
- TypeScript
- Vite
- HTML5
- CSS3
- Recharts or another suitable visualization library
- Lucide React icons
- Responsive dashboard layout

### Backend communication

Use a suitable local communication architecture.

Preferred architecture:

- C++ allocator engine
- C++ backend service using HTTP and JSON
- REST API for frontend communication
- React frontend communicating with the C++ backend

A lightweight C++ HTTP framework may be used if appropriate.

The backend must remain responsible for actual memory allocation decisions.

Do not implement a separate JavaScript allocator that merely imitates the C++ backend.

### Development tools

- Git
- CMake
- npm
- Node.js
- C++ compiler
- Automated testing tools
- AI coding agents

Use appropriate tools available in the development environment.

---

# 4. PROJECT ARCHITECTURE

Organize the application into separate, maintainable components.

Recommended structure:

project-root/
│
├── backend/
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── MemoryPool.hpp
│   │   ├── MemoryBlock.hpp
│   │   ├── Allocator.hpp
│   │   ├── FixedSizeAllocator.hpp
│   │   ├── VariableSizeAllocator.hpp
│   │   ├── FreeList.hpp
│   │   ├── AllocationStrategy.hpp
│   │   ├── MemoryStatistics.hpp
│   │   └── WorkloadSimulator.hpp
│   │
│   ├── src/
│   │   ├── main.cpp
│   │   ├── MemoryPool.cpp
│   │   ├── FixedSizeAllocator.cpp
│   │   ├── VariableSizeAllocator.cpp
│   │   ├── FreeList.cpp
│   │   ├── MemoryStatistics.cpp
│   │   ├── WorkloadSimulator.cpp
│   │   └── ApiServer.cpp
│   │
│   ├── tests/
│   │   ├── MemoryPoolTests.cpp
│   │   ├── AllocationTests.cpp
│   │   ├── DeallocationTests.cpp
│   │   ├── FragmentationTests.cpp
│   │   ├── FreeListTests.cpp
│   │   ├── CoalescingTests.cpp
│   │   ├── IntegrationTests.cpp
│   │   ├── SecurityTests.cpp
│   │   └── RegressionTests.cpp
│   │
│   └── benchmarks/
│       └── BenchmarkRunner.cpp
│
├── frontend/
│   ├── src/
│   │   ├── components/
│   │   │   ├── MemoryVisualization/
│   │   │   ├── AllocationControls/
│   │   │   ├── WorkloadEditor/
│   │   │   ├── StatisticsPanel/
│   │   │   ├── BenchmarkPanel/
│   │   │   ├── ExperimentHistory/
│   │   │   └── AgentTestingPanel/
│   │   │
│   │   ├── pages/
│   │   │   ├── Dashboard.tsx
│   │   │   ├── AllocatorSimulator.tsx
│   │   │   ├── Benchmarking.tsx
│   │   │   ├── Experiments.tsx
│   │   │   └── TestingDashboard.tsx
│   │   │
│   │   ├── services/
│   │   │   └── allocatorApi.ts
│   │   │
│   │   ├── hooks/
│   │   ├── types/
│   │   ├── utils/
│   │   ├── App.tsx
│   │   └── main.tsx
│   │
│   ├── package.json
│   └── vite.config.ts
│
├── workloads/
│   └── sample-workloads/
│
├── experiments/
│
├── documentation/
│
├── scripts/
│
├── README.md
└── .gitignore

You may modify this structure when necessary, but preserve separation between the allocator engine, backend API, frontend, tests, and experiment data.

Do not place the entire project inside one large source file.

---

# 5. C++ MEMORY POOL ALLOCATOR

Implement a genuine memory pool allocator in C++.

The allocator must manage a predefined region of memory and perform allocation and deallocation within that region.

The implementation must not rely on the operating system to independently allocate every user-requested block.

The allocator must maintain its own memory pool and allocation metadata.

## 5.1 Memory pool initialization

Allow users to configure:

- Total pool capacity.
- Pool initialization.
- Allocation strategy.
- Allocation mode.
- Fixed block size when using fixed-size allocation.
- Alignment requirements.
- Optional allocation limits.

The pool must be initialized with a valid memory region.

Display the initial state of the memory pool before any allocations.

The initial pool should appear as one large free region or as the appropriate collection of free blocks for the selected allocator.

Validate all configuration values.

Reject invalid pool sizes, invalid block sizes, unsupported alignment values, and configurations that exceed implementation limits.

---

# 6. MEMORY ALLOCATION STRATEGIES

Implement the following allocation strategies.

## 6.1 First-Fit Allocation

Implement First-Fit allocation.

The allocator must:

1. Traverse available free blocks in the defined order.
2. Find the first free block large enough for the request.
3. Allocate the requested memory.
4. Split the remaining free block when splitting is supported and beneficial.
5. Update allocation metadata.
6. Return the allocation identifier and actual allocation result.

The frontend must display the actual block selected by the C++ backend.

## 6.2 Best-Fit Allocation

Implement Best-Fit allocation.

The allocator must:

1. Inspect available free blocks.
2. Identify the smallest suitable free block.
3. Allocate the requested size.
4. Split the remaining block when appropriate.
5. Update the free list and metadata.
6. Return the allocation result.

Show how Best-Fit selects a different block from First-Fit when the workload makes the distinction observable.

## 6.3 Fixed-Size Allocation

Implement fixed-size memory allocation.

Allow the user to configure:

- Total pool size.
- Block size.
- Number of available blocks.
- Allocation requests.

The allocator must manage fixed-size blocks and track their allocation status.

Implement appropriate handling of requests that:

- Match the configured block size.
- Require more than one block.
- Are smaller than the block size.
- Exceed the supported allocation size.
- Exceed the remaining available capacity.

Clearly explain and display internal fragmentation when a request uses a fixed block larger than the requested size.

If multi-block allocations are supported, ensure that they are handled consistently and safely.

Do not silently assume that every fixed-size request consumes exactly its requested number of bytes.

## 6.4 Variable-Size Allocation

Implement variable-size allocation.

Users must be able to enter different allocation sizes.

The allocator must manage free regions dynamically.

Support:

- First-Fit.
- Best-Fit.
- Allocation metadata.
- Free block management.
- Block splitting.
- Deallocation.
- Memory reuse.
- Coalescing.
- Fragmentation tracking.

The frontend must display the actual allocation size and the requested size.

Where alignment or allocator metadata introduces overhead, expose the distinction between requested size and consumed pool space.

---

# 7. FREE LIST IMPLEMENTATION

Implement a free-list data structure for managing available memory.

The free list must track:

- Block starting offset.
- Block size.
- Free or allocated status.
- Block relationships.
- Allocation identifier where applicable.

Implement the appropriate free-list operations:

1. Initialize the free list.
2. Find a suitable free block.
3. Remove a block from the free list.
4. Split a block.
5. Insert a freed block.
6. Merge adjacent free blocks.
7. Update metadata.
8. Detect invalid free-list states.

Choose and document an appropriate free-list organization.

The implementation may use linked lists, ordered structures, or another suitable design.

If First-Fit and Best-Fit require different search behavior, implement that behavior explicitly.

Do not claim that an algorithm is First-Fit or Best-Fit if the implementation does not follow its actual selection rules.

---

# 8. MEMORY ALLOCATION AND DEALLOCATION

Implement allocation and deallocation as actual backend operations.

Each successful allocation must produce:

- Unique allocation identifier.
- Requested size.
- Actual allocated size.
- Starting offset.
- Ending offset.
- Allocation strategy.
- Allocation status.
- Allocation sequence number.

Each deallocation must identify a valid active allocation.

The backend must reject:

- Unknown allocation identifiers.
- Double frees.
- Invalid offsets.
- Invalid allocation sizes.
- Deallocation of already-freed blocks.
- Operations that would corrupt allocator metadata.

When an allocation is freed:

1. Mark the block as free.
2. Update allocation metadata.
3. Add the block to the free list.
4. Detect adjacent free blocks.
5. Coalesce adjacent free blocks when supported.
6. Update fragmentation statistics.
7. Return the updated memory state.

Ensure that memory is actually reusable after deallocation.

The next allocation must be able to reuse a freed block when the selected strategy and workload allow it.

---

# 9. BLOCK SPLITTING AND COALESCING

Implement block splitting.

When a free block is larger than the requested allocation:

- Allocate the required region.
- Preserve the remaining free region.
- Update the free list.
- Update block offsets.
- Ensure that no regions overlap.

Handle small leftover regions correctly.

Do not create unusable fragments smaller than the allocator's minimum valid free-block size.

Implement coalescing.

When adjacent blocks become free:

- Detect their adjacency.
- Merge them into a larger free block.
- Update the free list.
- Remove obsolete metadata.
- Ensure the resulting block has the correct size and boundaries.

The visualization must clearly show when blocks are split and when adjacent free blocks are merged.

---

# 10. MEMORY FRAGMENTATION

The application must demonstrate both internal and external fragmentation.

## 10.1 Internal fragmentation

Track internal fragmentation where applicable.

For fixed-size allocation:

Internal Fragmentation = Allocated Capacity - Requested Capacity

Display:

- Requested memory.
- Allocated memory.
- Wasted memory.
- Number of allocated blocks.
- Average internal fragmentation.

For variable-size allocation, report internal fragmentation only when the implementation produces it through block sizing, alignment, or other actual overhead.

Do not invent internal fragmentation values.

## 10.2 External fragmentation

Track free memory that is distributed across separate regions.

Display:

- Total free memory.
- Largest free block.
- Number of free blocks.
- Average free block size.
- External fragmentation indicator.
- Allocation failures caused by insufficient contiguous space.

Use a documented external-fragmentation metric.

For example, when total free memory is nonzero:

External Fragmentation Ratio = 1 - Largest Free Block / Total Free Memory

Clearly state that this is the application's chosen fragmentation indicator and not a universal measurement standard.

Handle the zero-free-memory case without division by zero.

Show examples where:

- Total free memory is sufficient.
- No single free block is large enough.
- Allocation fails because of external fragmentation.
- Coalescing restores a sufficiently large free region.

---

# 11. INTERACTIVE REACT DASHBOARD

Build a modern React dashboard that communicates with the C++ backend.

The dashboard must be visually polished, responsive, intuitive, and easy to use.

Use a professional software-development dashboard design.

Avoid unnecessary decorative elements, excessive gradients, cluttered layouts, and oversized cards.

The interface should prioritize actual allocator operations and memory visualization.

## 11.1 Dashboard sections

Include the following sections.

### A. Overview

Display:

- Total pool capacity.
- Used memory.
- Free memory.
- Number of active allocations.
- Number of free blocks.
- Number of allocation failures.
- Current allocation strategy.
- Current allocator mode.
- Fragmentation indicators.

### B. Memory Visualization

Display a graphical representation of the memory pool.

Each memory block must be positioned according to its actual backend offset.

The visualization must show:

- Free blocks.
- Allocated blocks.
- Block sizes.
- Allocation identifiers.
- Memory offsets.
- Allocation order.
- Fragmented regions.
- Fixed-size blocks when applicable.

Use a consistent visual representation for each block state.

Provide a legend.

Users must be able to inspect individual blocks.

When a user selects a block, display its details.

Use actual backend data to determine the block's location, size, and status.

### C. Allocation Controls

Provide controls for:

- Pool size.
- Allocation strategy.
- Allocation mode.
- Fixed block size.
- Allocation size.
- Alignment where supported.
- Allocation identifier when selecting an existing allocation for deallocation.

Include buttons for:

- Initialize Pool.
- Allocate Memory.
- Free Selected Allocation.
- Run Workload.
- Step.
- Pause.
- Resume.
- Reset.

### D. Statistics

Display live statistics for:

- Total memory.
- Used memory.
- Free memory.
- Requested memory.
- Allocated capacity.
- Internal fragmentation.
- External fragmentation.
- Active allocation count.
- Free block count.
- Allocation successes.
- Allocation failures.
- Deallocation count.
- Memory utilization.

### E. Workload Editor

Provide an interactive workload editor.

Users must be able to define custom allocation and deallocation sequences.

### F. Benchmarking

Provide a dedicated benchmark section for comparing allocator behavior.

### G. Experiment History

Provide a section for reviewing completed experiments and exported results.

### H. Testing Dashboard

Provide a testing interface displaying automated testing results and agent activity.

---

# 12. REAL-TIME MEMORY VISUALIZATION

The memory visualization must be driven by actual C++ allocator state.

The React frontend must never independently decide:

- Which free block is selected.
- Whether an allocation succeeds.
- Whether a block is split.
- Whether adjacent blocks are merged.
- Whether memory is reusable.
- Whether fragmentation causes allocation failure.

Those decisions belong exclusively to the C++ allocator.

The frontend should receive a structured response from the backend and render it.

Example backend memory state:

{
  "poolSize": 1024,
  "usedMemory": 300,
  "freeMemory": 724,
  "blocks": [
    {
      "id": "A1",
      "offset": 0,
      "size": 100,
      "requestedSize": 100,
      "status": "allocated"
    },
    {
      "id": "F1",
      "offset": 100,
      "size": 200,
      "status": "free"
    },
    {
      "id": "A2",
      "offset": 300,
      "size": 200,
      "requestedSize": 200,
      "status": "allocated"
    },
    {
      "id": "F2",
      "offset": 500,
      "size": 524,
      "status": "free"
    }
  ]
}

This example is only a schema illustration.

The application must render actual values returned by the allocator.

The frontend must not hardcode this example as the real memory state.

## Visualization requirements

- Display block boundaries accurately.
- Display actual memory offsets.
- Display actual block sizes.
- Scale blocks according to their size.
- Show allocation and deallocation changes.
- Animate changes without changing backend results.
- Highlight the block selected by an allocation operation.
- Highlight freed blocks.
- Show split and coalescing operations.
- Display allocation failures.
- Provide a clear memory-state legend.

Animation must not alter allocator behavior.

If an animation is interrupted, the displayed state must still correspond to the latest backend state.

---

# 13. USER INPUT AND CUSTOM WORKLOADS

The application must allow users to control allocator behavior through the interface.

Do not restrict the application to predefined demonstration sequences.

Users must be able to enter their own workloads.

## 13.1 Manual allocation

Allow users to:

1. Select the allocation strategy.
2. Enter an allocation size.
3. Execute the allocation.
4. View the result.
5. Inspect the selected block.
6. Repeat the operation.

The backend must return the actual allocation result.

## 13.2 Manual deallocation

Allow users to:

1. View active allocations.
2. Select an allocation.
3. Free it.
4. Observe the updated memory pool.

Only valid active allocations may be freed.

## 13.3 Custom workload editor

Create an editor where users can define a sequence of operations.

Supported operation types:

- Allocate.
- Free.
- Pause or step between operations where appropriate.

Each operation must have a defined structure.

Example:

[
  {
    "operation": "allocate",
    "size": 120
  },
  {
    "operation": "allocate",
    "size": 240
  },
  {
    "operation": "allocate",
    "size": 80
  },
  {
    "operation": "free",
    "allocationId": "A1"
  },
  {
    "operation": "allocate",
    "size": 100
  }
]

This is an illustrative workload format.

Actual allocation identifiers must be resolved from backend results.

Do not assume that identifiers are known before allocation.

## 13.4 Workload controls

Allow users to:

- Add operations.
- Edit operations.
- Delete operations.
- Reorder operations.
- Validate the workload.
- Execute the workload.
- Execute one operation at a time.
- Pause execution.
- Resume execution.
- Reset the experiment.

Display the current operation.

Display the completed operation count.

Display the remaining operation count.

Display operation results.

When an operation fails, show the actual backend error or failure reason.

Do not silently skip failed operations.

## 13.5 Random workloads

Allow users to generate random workloads.

Configuration options:

- Number of operations.
- Minimum allocation size.
- Maximum allocation size.
- Allocation probability.
- Deallocation probability.
- Random seed.
- Pool size.
- Allocation strategy.

Random workloads must be reproducible when the same seed and configuration are used.

Ensure that generated free operations refer to existing active allocations.

Do not generate invalid deallocation operations unless the user explicitly selects an invalid-operation testing scenario.

---

# 14. CSV IMPORT AND EXPORT

Implement CSV support.

## 14.1 CSV workload import

Allow users to import a workload from a CSV file.

Support an appropriate format such as:

operation,size,allocation_id

allocate,120,

allocate,240,

free,,A1

Validate the file before execution.

Report:

- Invalid rows.
- Missing fields.
- Unsupported operations.
- Invalid sizes.
- Invalid allocation references.
- Parsing errors.

Do not execute malformed workloads.

Provide a downloadable example CSV template.

## 14.2 CSV experiment export

Allow users to export experiment results.

Include:

- Experiment identifier.
- Timestamp.
- Pool size.
- Allocation mode.
- Allocation strategy.
- Workload configuration.
- Operation sequence.
- Allocation results.
- Allocation failures.
- Requested memory.
- Allocated memory.
- Free memory.
- Internal fragmentation.
- External fragmentation.
- Active allocation count.
- Free block count.
- Benchmark duration.
- Relevant performance metrics.

Use actual experiment results.

Do not create fabricated benchmark data.

Allow users to export:

1. Operation-level results.
2. Final memory state.
3. Summary statistics.
4. Benchmark comparison results.

Use consistent CSV formatting.

---

# 15. BACKEND API

Implement a reliable API connecting React to the C++ allocator.

The API must expose operations such as:

POST /api/pool/initialize

POST /api/allocate

POST /api/free

POST /api/workload/execute

POST /api/workload/step

POST /api/workload/pause

POST /api/workload/resume

POST /api/pool/reset

GET /api/pool/state

GET /api/statistics

POST /api/benchmark

POST /api/experiment/export

These are suggested endpoint names.

Adjust them as necessary.

The backend must provide:

- Input validation.
- JSON request parsing.
- JSON response serialization.
- Meaningful error messages.
- Consistent error handling.
- State synchronization.
- Safe allocator access.
- Appropriate API documentation.

Every allocation-related API must execute actual C++ allocator logic.

## API response requirements

For successful operations, return:

- Operation identifier.
- Allocation identifier when applicable.
- Requested size.
- Allocated size.
- Memory offset.
- Success status.
- Updated memory state.
- Updated statistics.
- Operation result.

For failed operations, return:

- Failure status.
- Error code.
- Meaningful explanation.
- Updated state if the operation changed any state.

Ensure the frontend handles failed requests without losing synchronization with the backend.

---

# 16. BENCHMARKING

Implement a benchmarking system for the allocator.

Users must be able to compare:

- First-Fit.
- Best-Fit.
- Fixed-size allocation.
- Variable-size allocation.

Use the same workload when comparing algorithms wherever the comparison is intended to be controlled.

The benchmark must use actual C++ allocator execution.

## 16.1 Benchmark metrics

Collect:

- Total workload execution time.
- Allocation operation count.
- Deallocation operation count.
- Successful allocation count.
- Failed allocation count.
- Average allocation latency.
- Maximum allocation latency.
- Total requested memory.
- Total allocated capacity.
- Peak memory utilization.
- Final free memory.
- Final largest free block.
- Final free block count.
- Fragmentation metrics.

Measure performance using suitable high-resolution timing facilities.

Do not include frontend animation time in allocator performance measurements.

Separate:

- Allocator execution time.
- API overhead where measurable.
- Visualization or UI time.

## 16.2 Algorithm comparison

Provide a comparison interface.

Allow users to:

1. Select a workload.
2. Choose allocation strategies.
3. Execute the workload using each strategy.
4. Compare the results.

Display comparison charts for:

- Allocation failures.
- Memory utilization.
- Fragmentation.
- Execution time.
- Allocation latency.

Explain that performance depends on workload characteristics and allocator implementation.

Do not automatically declare one algorithm universally superior.

## 16.3 Benchmark correctness

Ensure that:

- Each strategy receives the intended workload.
- Each experiment starts from a clean pool.
- Random workloads use consistent seeds.
- Failed allocations are recorded.
- Results are not fabricated.
- Benchmark results can be reproduced.

---

# 17. EXPERIMENT LOGGING

Implement experiment logging.

Every experiment must record:

- Experiment ID.
- Date and time.
- Pool configuration.
- Allocation strategy.
- Allocation mode.
- Workload.
- Random seed if applicable.
- Operation results.
- Final memory state.
- Statistics.
- Benchmark results.

Allow users to:

- Review experiments.
- Compare experiments.
- Export experiments.
- Delete experiment records when appropriate.
- Reset experiment history.

Clearly distinguish between:

- Current allocator state.
- Saved experiment state.
- Benchmark results.

Do not accidentally reuse results from an earlier experiment.

---

# 18. AI AGENT SYSTEM

Integrate specialized AI agents into the development workflow.

The agents must help build, test, validate, and improve the application.

Agents must operate on the actual project source code and test results.

They must not merely generate textual explanations claiming that the application works.

The AI agent system must support the following roles.

## 18.1 TDD Development Agent

Purpose:

Develop the project using Test-Driven Development.

Follow the Red-Green-Refactor workflow.

### Red

- Identify a required behavior.
- Write a test for the behavior.
- Execute the test.
- Confirm that the test fails for the expected reason.

### Green

- Implement the minimum code needed to satisfy the test.
- Execute the test.
- Confirm that it passes.

### Refactor

- Improve the implementation.
- Remove duplication.
- Improve readability.
- Preserve behavior.
- Run relevant tests again.

The agent must follow this process for core allocator features.

Required TDD targets include:

- Pool initialization.
- First-Fit allocation.
- Best-Fit allocation.
- Fixed-size allocation.
- Variable-size allocation.
- Free-list management.
- Allocation.
- Deallocation.
- Memory reuse.
- Block splitting.
- Coalescing.
- Fragmentation.
- Invalid allocation handling.
- Invalid deallocation handling.
- API integration.

The agent must not claim TDD was followed unless the tests were actually written and executed.

## 18.2 Unit Testing Agent

Purpose:

Test individual allocator components.

Test:

- Memory pool initialization.
- Block metadata.
- Allocation strategy selection.
- Free-list behavior.
- Fixed-size allocation.
- Variable-size allocation.
- Allocation limits.
- Deallocation.
- Splitting.
- Coalescing.
- Fragmentation calculations.
- Invalid inputs.

The agent must:

1. Identify relevant unit tests.
2. Execute them.
3. Collect actual results.
4. Identify failures.
5. Report the failing test.
6. Suggest or implement fixes when authorized.
7. Rerun the tests after changes.

## 18.3 Integration Testing Agent

Purpose:

Verify that the complete application works across the C++ backend, API, and React frontend.

Test:

- Backend startup.
- API initialization.
- Pool creation.
- Allocation requests.
- Deallocation requests.
- Memory-state retrieval.
- Statistics retrieval.
- Workload execution.
- Step execution.
- Pause and resume.
- Reset.
- CSV import.
- CSV export.
- Benchmark execution.

Verify that the frontend displays actual backend responses.

Detect:

- API errors.
- Invalid JSON.
- State synchronization problems.
- Incorrect allocation identifiers.
- Stale frontend state.
- Missing memory updates.
- Backend/frontend contract mismatches.

## 18.4 Memory Safety Testing Agent

Purpose:

Detect memory safety problems in the C++ allocator.

Use available tools such as:

- AddressSanitizer.
- UndefinedBehaviorSanitizer.
- Valgrind.
- Compiler warnings.
- Static analysis tools.

Test for:

- Buffer overflows.
- Out-of-bounds access.
- Use-after-free.
- Double free.
- Invalid pointer operations.
- Memory leaks.
- Integer overflow.
- Invalid alignment.
- Corrupted allocator metadata.
- Free-list corruption.
- Overlapping allocations.
- Invalid block boundaries.

The agent must execute actual tests where tools are available.

If a tool is unavailable, report that it was not executed.

Do not claim that memory safety has been verified solely because the source code appears correct.

## 18.5 Authorized Penetration Testing Agent

Purpose:

Perform security testing against the application's own backend and local development environment.

The agent must focus on:

- Input validation.
- API security.
- Request validation.
- Invalid JSON.
- Oversized requests.
- Unexpected data types.
- Invalid allocation sizes.
- Integer overflow.
- Malformed CSV files.
- Resource exhaustion.
- Repeated invalid requests.
- Unauthorized API access.
- Unsafe file handling.
- Incorrect error handling.
- Potential denial-of-service conditions.
- State corruption.

The agent must test only:

- The project's own local backend.
- The project's own development environment.
- Explicitly authorized test environments.

### Security testing restrictions

The agent must not:

- Attack external websites.
- Scan unrelated systems.
- Access unauthorized services.
- Attempt credential theft.
- Execute destructive operations against production systems.
- Delete user files.
- Modify unrelated system configurations.
- Perform uncontrolled resource-exhaustion tests.
- Execute destructive tests without explicit authorization.

Use bounded, controlled test workloads.

Require explicit user approval before:

- Destructive testing.
- Potentially disruptive testing.
- External environment testing.
- Tests requiring elevated privileges.
- Changes to security-sensitive configuration.

Provide a clear testing scope and report.

## 18.6 Regression Testing Agent

Purpose:

Ensure that new changes do not break previously working features.

After each significant code change:

1. Identify affected components.
2. Run relevant unit tests.
3. Run relevant integration tests.
4. Run the existing regression suite.
5. Compare results.
6. Identify newly introduced failures.
7. Report the results.

The agent must verify previously working functionality after changes to:

- Allocation algorithms.
- Free-list implementation.
- Block metadata.
- API routes.
- Frontend state.
- Visualization.
- Workload execution.
- Benchmarking.
- CSV handling.

Do not mark a regression test as passed unless it was actually executed successfully.

## 18.7 Frontend Testing Agent

Purpose:

Verify the React dashboard.

Test:

- Dashboard rendering.
- Pool initialization.
- Allocation controls.
- Deallocation controls.
- Workload editor.
- Workload execution.
- Step controls.
- Pause and resume.
- Reset.
- Memory visualization.
- Statistics.
- Experiment history.
- CSV import.
- CSV export.
- Benchmark charts.
- Error messages.
- Loading states.

Verify that frontend components are connected to the real backend.

Detect hardcoded demonstration values that are incorrectly displayed as live data.

## 18.8 Documentation and Observation Agent

Purpose:

Generate and maintain project documentation based on the actual implementation and execution results.

Document:

- Project architecture.
- Allocator algorithms.
- Memory pool implementation.
- Free-list design.
- API endpoints.
- Frontend/backend integration.
- Workload configuration.
- Benchmarking methodology.
- Test results.
- Observations.
- Limitations.

The agent must distinguish between:

- Verified observations.
- Expected behavior.
- Hypothetical examples.
- Unexecuted tests.
- Known limitations.

Do not fabricate experimental results.

---

# 19. AI AGENT EXECUTION MODES

Implement appropriate execution modes for AI agents.

## Read-only mode

Agents may:

- Inspect source code.
- Inspect project files.
- Read logs.
- Analyze test results.
- Suggest changes.

They must not modify files or execute potentially destructive operations.

## Testing mode

Agents may:

- Execute approved tests.
- Run bounded workloads.
- Inspect results.
- Generate test reports.

They must not perform unauthorized destructive actions.

## Development mode

Agents may:

- Modify project source code.
- Implement features.
- Write tests.
- Refactor code.
- Fix bugs.

Source modification must follow the project's development permissions.

## Security testing mode

Agents may execute approved security tests against the project's own environment.

Potentially disruptive tests must require explicit approval.

Clearly identify which mode is active.

Maintain logs of agent actions.

---

# 20. AI AGENT TESTING DASHBOARD

Create a dashboard for managing automated testing.

Display:

- Agent name.
- Agent role.
- Current status.
- Assigned task.
- Test category.
- Execution status.
- Passed tests.
- Failed tests.
- Skipped tests.
- Errors.
- Duration.
- Last execution.
- Relevant logs.

Provide controls for:

- Starting approved test suites.
- Running unit tests.
- Running integration tests.
- Running regression tests.
- Running memory safety tests.
- Running authorized security tests.
- Viewing results.
- Reviewing logs.

Do not create fake agent activity.

The dashboard must reflect actual agent execution and actual test results.

If AI agents are unavailable or not configured, the application must clearly indicate that the agent functionality is unavailable.

Do not display simulated agent success as real execution.

---

# 21. TESTING REQUIREMENTS

The application must include comprehensive automated testing.

## 21.1 Allocator unit tests

Test:

- Initial pool state.
- Full allocation.
- Partial allocation.
- Allocation failure.
- Exact-fit allocation.
- First-Fit behavior.
- Best-Fit behavior.
- Fixed-size allocation.
- Variable-size allocation.
- Memory reuse.
- Invalid sizes.
- Pool boundaries.
- Deallocation.
- Double free.
- Invalid allocation identifiers.
- Block splitting.
- Coalescing.
- Internal fragmentation.
- External fragmentation.
- Free-list consistency.

## 21.2 Property-based testing

Where practical, implement property-based tests.

Verify properties such as:

- Allocated blocks never overlap.
- Every block remains within pool boundaries.
- Total block sizes equal pool capacity.
- Every active allocation has valid metadata.
- Free blocks are correctly represented.
- Deallocated blocks can be reused.
- Coalescing preserves total memory capacity.
- Allocation identifiers remain unique.
- Invalid operations do not corrupt allocator state.

Use randomized inputs with reproducible seeds.

## 21.3 Stress testing

Execute bounded stress tests.

Test:

- Repeated allocation.
- Repeated deallocation.
- Alternating allocation and deallocation.
- Large numbers of small allocations.
- Mixed allocation sizes.
- Fragmentation-heavy workloads.
- Allocation failures.
- Recovery after freeing memory.

Do not allow uncontrolled memory consumption.

## 21.4 API testing

Test:

- Valid requests.
- Invalid requests.
- Missing parameters.
- Incorrect data types.
- Invalid identifiers.
- Oversized requests.
- Malformed JSON.
- Backend unavailable.
- State synchronization.
- Error responses.

## 21.5 Frontend testing

Test:

- Component rendering.
- User input.
- API integration.
- Error states.
- Memory updates.
- Statistics updates.
- Workload controls.
- Export functionality.
- Responsive layout.

## 21.6 Regression testing

Run regression tests after meaningful changes.

Maintain a reliable test suite.

Document the command used to run each test category.

---

# 22. OBSERVATIONS AND EXPERIMENTAL ANALYSIS

The application must support observation of allocator behavior.

Users should be able to understand:

- How First-Fit chooses a block.
- How Best-Fit chooses a block.
- How fixed-size allocation behaves.
- How variable-size allocation behaves.
- How free-list organization affects allocation.
- How memory reuse works.
- How splitting changes the pool.
- How coalescing changes the pool.
- How fragmentation develops.
- Why an allocation may fail.
- How different workloads affect allocator performance.

Generate observations from actual experiment results.

Examples of valid observation categories:

- Allocation success or failure.
- Memory utilization.
- Fragmentation changes.
- Number of free blocks.
- Largest free block.
- Memory reuse.
- Allocation latency.
- Benchmark differences.

When the application generates an observation, it must reference the experiment that supports it.

Do not claim that a particular algorithm performed better unless the executed experiment supports that conclusion.

---

# 23. LIMITATIONS

The application must explicitly document its limitations.

Potential limitations include:

- Pool capacity is fixed during an experiment.
- The allocator is a simulator and demonstration of memory management.
- Performance depends on the workload.
- Results depend on the implementation.
- Benchmark measurements may be affected by the development environment.
- Fixed-size allocation may introduce internal fragmentation.
- Variable-size allocation may introduce external fragmentation.
- The allocator may not support every operating-system memory-management feature.
- The application may not implement production-grade concurrent allocation.
- The application may not provide thread safety unless explicitly implemented.
- AI agents may be unavailable or require configuration.
- Security testing is limited to the authorized environment.
- Sanitizers and Valgrind may not be available on every platform.

Document actual limitations rather than blindly copying this list.

Clearly identify any feature that is not implemented.

Do not describe an incomplete feature as fully supported.

---

# 24. USER EXPERIENCE REQUIREMENTS

The application must provide a clear and intuitive user experience.

Include:

- Responsive layout.
- Proper form validation.
- Meaningful error messages.
- Loading indicators.
- Empty states.
- Confirmation for destructive operations.
- Accessible controls.
- Clear labels.
- Tooltips where useful.
- Keyboard-friendly interactions.
- Consistent styling.
- Readable memory statistics.
- Useful charts.
- Smooth visualization transitions.

Users must understand:

- Which allocator is active.
- Which operation will execute.
- What the operation did.
- Whether the operation succeeded.
- Why it failed.
- How the memory state changed.

Avoid unnecessary complexity.

Do not hide important allocator information behind excessive navigation.

---

# 25. VISUAL DESIGN REQUIREMENTS

Use a professional modern dashboard design.

Design priorities:

1. Clear hierarchy.
2. Readable statistics.
3. Accurate memory visualization.
4. Simple controls.
5. Consistent spacing.
6. Responsive layout.
7. Useful charts.
8. Accessible color contrast.

Use a restrained color system.

Use colors to distinguish:

- Free memory.
- Allocated memory.
- Selected memory.
- Allocation failure.
- Fragmentation.
- Operation status.

Do not rely only on color to convey information.

Include labels, icons, or other visual indicators where useful.

The memory visualization must be the central visual element of the allocator simulator.

The dashboard should not look like a generic landing page.

Prioritize functionality over unnecessary animations and decorative components.

---

# 26. DATA INTEGRITY AND STATE MANAGEMENT

Maintain a single authoritative allocator state in the C++ backend.

The frontend must not maintain an independent memory model that diverges from the backend.

Frontend state may contain:

- Display preferences.
- Form values.
- Current selected allocation.
- Current experiment.
- UI loading state.
- Animation state.

The frontend must not independently determine allocation results.

After every allocator operation:

1. Receive the backend response.
2. Validate the response.
3. Update the displayed allocator state.
4. Update statistics.
5. Update the operation history.
6. Update the visualization.

If a backend request fails:

- Preserve the last confirmed backend state.
- Display the error.
- Avoid displaying an invented memory update.
- Allow the user to retry when appropriate.

Handle stale requests and concurrent operations safely.

Prevent multiple incompatible operations from executing simultaneously unless concurrency is explicitly supported.

---

# 27. PERFORMANCE AND RESOURCE MANAGEMENT

Ensure the application performs efficiently.

Requirements:

- Avoid unnecessary frontend rerenders.
- Avoid excessive API polling.
- Use efficient allocator data structures.
- Avoid unnecessary memory copies.
- Keep benchmark measurements independent of UI rendering.
- Validate large workloads.
- Limit CSV import size.
- Limit generated workload size.
- Prevent unbounded logs.
- Handle large numbers of blocks safely.

Provide reasonable configurable limits.

Document the limits.

The application must remain responsive during normal workloads.

---

# 28. IMPLEMENTATION WORKFLOW

Follow this development sequence.

## Phase 1: Project setup

- Inspect the existing project.
- Identify available tools.
- Create the project structure.
- Configure CMake.
- Configure React and Vite.
- Configure the testing framework.
- Configure the backend/frontend communication.
- Establish a working development environment.

## Phase 2: Allocator implementation

Implement:

1. Memory pool initialization.
2. Block metadata.
3. Free-list implementation.
4. First-Fit.
5. Best-Fit.
6. Fixed-size allocation.
7. Variable-size allocation.
8. Allocation.
9. Deallocation.
10. Memory reuse.
11. Splitting.
12. Coalescing.
13. Fragmentation metrics.

Write tests before implementation wherever practical.

## Phase 3: Backend API

Implement:

- API initialization.
- Pool initialization endpoint.
- Allocation endpoint.
- Deallocation endpoint.
- Memory-state endpoint.
- Statistics endpoint.
- Workload execution.
- Step execution.
- Pause and resume.
- Reset.

Verify all endpoints through automated tests.

## Phase 4: React frontend

Implement:

- Dashboard.
- Memory visualization.
- Allocation controls.
- Deallocation controls.
- Statistics.
- Workload editor.
- Experiment history.
- Benchmarking.
- Testing dashboard.

Connect the frontend to the backend.

Do not use fabricated allocator data.

## Phase 5: Workload system

Implement:

- Manual workloads.
- Custom workload editor.
- Random workloads.
- Reproducible random seeds.
- CSV import.
- Workload validation.
- Step execution.
- Pause and resume.
- Reset.

## Phase 6: Benchmarking

Implement:

- Benchmark execution.
- Algorithm comparison.
- Performance measurement.
- Statistics.
- Charts.
- CSV export.

## Phase 7: AI agent integration

Implement or configure:

- TDD Development Agent.
- Unit Testing Agent.
- Integration Testing Agent.
- Memory Safety Testing Agent.
- Authorized Penetration Testing Agent.
- Regression Testing Agent.
- Frontend Testing Agent.
- Documentation and Observation Agent.

Connect agent execution to the testing dashboard.

Ensure agent permissions and execution modes are enforced.

## Phase 8: Validation

Run:

- Unit tests.
- Integration tests.
- Property-based tests.
- Stress tests.
- Memory safety tests.
- Security tests.
- Frontend tests.
- Regression tests.

Fix actual issues.

Do not skip failures merely to produce a successful-looking result.

## Phase 9: Documentation

Create:

- README.
- Installation instructions.
- Build instructions.
- Backend startup instructions.
- Frontend startup instructions.
- API documentation.
- Allocator explanation.
- Workload instructions.
- Benchmark instructions.
- Testing instructions.
- Limitations.
- Actual observations from executed experiments.

## Phase 10: Final verification

Verify that:

- The backend builds.
- The frontend builds.
- The backend starts.
- The frontend starts.
- API communication works.
- Memory allocation works.
- Memory deallocation works.
- Memory reuse works.
- Visualization updates correctly.
- Workloads execute correctly.
- Benchmarks execute.
- CSV import works.
- CSV export works.
- Automated tests execute.
- Error handling works.

Provide a concise implementation summary and the exact commands required to run the project.

---

# 29. ACCEPTANCE CRITERIA

The project is considered complete only when the following requirements are verified.

## Core allocator

- [ ] C++ memory pool implemented.
- [ ] First-Fit implemented.
- [ ] Best-Fit implemented.
- [ ] Fixed-size allocation implemented.
- [ ] Variable-size allocation implemented.
- [ ] Free list implemented.
- [ ] Allocation works.
- [ ] Deallocation works.
- [ ] Memory reuse works.
- [ ] Block splitting works.
- [ ] Coalescing works.
- [ ] Internal fragmentation is tracked where applicable.
- [ ] External fragmentation is tracked.
- [ ] Invalid operations are handled safely.

## Backend

- [ ] Backend builds.
- [ ] Backend starts.
- [ ] API endpoints work.
- [ ] JSON communication works.
- [ ] Allocation results are actual C++ results.
- [ ] Backend state remains consistent.
- [ ] API errors are handled correctly.

## Frontend

- [ ] React dashboard works.
- [ ] Memory visualization works.
- [ ] Allocation controls work.
- [ ] Deallocation controls work.
- [ ] Statistics update.
- [ ] Workload editor works.
- [ ] Step execution works.
- [ ] Pause and resume work.
- [ ] Reset works.
- [ ] Error messages are displayed.
- [ ] Frontend displays actual backend state.

## Workloads

- [ ] Manual workloads work.
- [ ] Custom workloads work.
- [ ] Random workloads work.
- [ ] Random seed reproduces results.
- [ ] CSV import works.
- [ ] CSV validation works.
- [ ] Workload execution is controllable.
- [ ] Allocation failures are recorded.

## Benchmarking

- [ ] First-Fit benchmark works.
- [ ] Best-Fit benchmark works.
- [ ] Fixed-size benchmark works.
- [ ] Variable-size benchmark works.
- [ ] Results use actual execution.
- [ ] Statistics are calculated correctly.
- [ ] Comparison charts work.
- [ ] CSV export works.

## AI agents

- [ ] TDD Development Agent implemented or configured.
- [ ] Unit Testing Agent implemented or configured.
- [ ] Integration Testing Agent implemented or configured.
- [ ] Memory Safety Testing Agent implemented or configured.
- [ ] Authorized Penetration Testing Agent implemented or configured.
- [ ] Regression Testing Agent implemented or configured.
- [ ] Frontend Testing Agent implemented or configured.
- [ ] Documentation and Observation Agent implemented or configured.
- [ ] Agent permissions are enforced.
- [ ] Agent execution results are genuine.
- [ ] Testing dashboard displays actual results.

## Documentation

- [ ] README completed.
- [ ] Installation instructions provided.
- [ ] Build instructions provided.
- [ ] Run instructions provided.
- [ ] API documentation provided.
- [ ] Allocator behavior documented.
- [ ] Testing commands documented.
- [ ] Observations based on actual results.
- [ ] Limitations documented.

Do not mark any criterion complete unless it has been implemented and verified.

---

# 30. IMPORTANT DEVELOPMENT RULES

Follow these rules throughout the project.

### Rule 1: Actual backend execution

All allocation decisions must be performed by the C++ backend.

Do not simulate allocator behavior in React.

### Rule 2: No fake data

Do not hardcode memory states, benchmark results, test results, agent status, or experimental observations as real results.

Demo data may be provided only when clearly labeled as demonstration data.

### Rule 3: TDD

Use Red-Green-Refactor for core allocator functionality.

Write tests and execute them.

### Rule 4: Validate every operation

Validate allocation requests, deallocation requests, workloads, CSV imports, and API inputs.

### Rule 5: Preserve memory integrity

Prevent overlapping allocations, invalid block boundaries, corrupted metadata, and invalid free-list states.

### Rule 6: Use modular architecture

Separate allocator logic, backend API, frontend UI, tests, and documentation.

### Rule 7: Real statistics

Calculate statistics from actual allocator state.

### Rule 8: Honest testing

Report tests as passed, failed, skipped, or not executed.

### Rule 9: Controlled security testing

Security agents must remain within the authorized project environment.

### Rule 10: No unnecessary scope

Do not add unrelated features that distract from the memory allocator.

### Rule 11: Complete implementation

Do not stop after generating the UI.

Implement the backend, connect it to the frontend, and verify the complete workflow.

### Rule 12: No placeholder functionality

Do not leave core features as TODOs, static mockups, or fake implementations.

If a feature cannot be completed, clearly identify it as incomplete.

---

# 31. FINAL PROJECT DELIVERABLE

Build the complete working application.

The final result must include:

1. C++ memory pool allocator.
2. First-Fit allocation.
3. Best-Fit allocation.
4. Fixed-size allocation.
5. Variable-size allocation.
6. Free-list implementation.
7. Allocation and deallocation.
8. Memory reuse.
9. Block splitting.
10. Coalescing.
11. Fragmentation tracking.
12. React interactive dashboard.
13. Real-time memory visualization.
14. User-controlled allocation requests.
15. Custom workload editor.
16. Random workload generation.
17. CSV workload import.
18. CSV experiment export.
19. Benchmarking.
20. Experiment history.
21. Statistics and charts.
22. AI agent testing integration.
23. TDD workflow.
24. Unit testing.
25. Integration testing.
26. Memory safety testing.
27. Authorized penetration testing.
28. Regression testing.
29. Frontend testing.
30. Documentation and observations.
31. Limitations.
32. Installation and execution instructions.

The final application must be functional, not merely a design prototype.

---

# 32. FINAL INSTRUCTION TO THE AI CODING AGENT

Start by inspecting the existing development environment and project files.

Then create or update the project structure.

Implement the allocator in C++ first.

Write and execute tests for each allocator feature.

Implement the backend API.

Connect the React dashboard to the actual backend.

Implement the memory visualization using real allocator state.

Implement user-defined workloads, random workloads, CSV import/export, statistics, benchmarking, and experiment history.

Integrate the AI testing agents with appropriate permissions.

Execute the available tests and fix verified problems.

Document the actual implementation, execution commands, observations, test results, and limitations.

Do not stop after planning or creating a project scaffold.

Do not provide only a theoretical explanation.

Do not replace the backend with JavaScript simulation.

Do not fabricate successful tests.

Do not fabricate benchmark results.

Do not claim a feature is complete if it has not been implemented.

**Your primary goal is to deliver a complete, runnable, tested, and interactive Custom Memory Pool Allocator application in which the React dashboard visualizes the real behavior of the C++ memory allocator.**

Begin implementation now, working incrementally and verifying each major component before proceeding to the next.