const COMPARISON_COLUMNS = [
  ['experiment_id', 'Experiment ID'],
  ['experiment_name', 'Experiment name'],
  ['execution_timestamp', 'Execution timestamp'],
  ['allocator_algorithm', 'Allocator algorithm'],
  ['workload_type', 'Workload type'],
  ['allocation_mode', 'Allocation mode'],
  ['fixed_block_size_bytes', 'Fixed block size (bytes)'],
  ['workload_configuration', 'Workload configuration'],
  ['total_operations', 'Total operations'],
  ['allocation_requests', 'Allocation requests'],
  ['successful_allocations', 'Successful allocations'],
  ['failed_allocations', 'Failed allocations'],
  ['allocation_success_rate', 'Allocation success rate'],
  ['allocation_failure_rate', 'Allocation failure rate'],
  ['average_allocation_latency_ms', 'Average allocation latency (ms)'],
  ['maximum_allocation_latency_ms', 'Maximum allocation latency (ms)'],
  ['total_allocation_time_ms', 'Total allocation time (ms)'],
  ['execution_time_ms', 'Execution time (ms)'],
  ['deallocation_requests', 'Deallocation requests'],
  ['successful_deallocations', 'Successful deallocations'],
  ['failed_deallocations', 'Failed deallocations'],
  ['total_deallocation_time_ms', 'Total deallocation time (ms)'],
  ['average_deallocation_latency_ms', 'Average deallocation latency (ms)'],
  ['maximum_deallocation_latency_ms', 'Maximum deallocation latency (ms)'],
  ['pool_capacity_bytes', 'Pool capacity (bytes)'],
  ['allocated_memory_bytes', 'Allocated memory (bytes)'],
  ['free_memory_bytes', 'Free memory (bytes)'],
  ['peak_memory_usage_bytes', 'Peak memory usage (bytes)'],
  ['peak_memory_utilization', 'Peak memory utilization'],
  ['final_memory_utilization', 'Final memory utilization'],
  ['internal_fragmentation', 'Internal fragmentation'],
  ['external_fragmentation', 'External fragmentation'],
  ['largest_free_block_bytes', 'Largest free block (bytes)'],
  ['free_block_count', 'Free block count'],
  ['block_splits', 'Block splits'],
  ['coalescing_operations', 'Coalescing operations'],
  ['conditions_match', 'Comparison conditions match'],
];

function csvEscape(value) {
  if (value === null || value === undefined) return '';
  const text = String(value);
  return /[",\r\n]/.test(text) ? `"${text.replaceAll('"', '""')}"` : text;
}

function comparisonRow(result, experiment, baselineId, conditionsMatch) {
  return {
    experiment_id: result.experiment_id,
    experiment_name: experiment?.name,
    execution_timestamp: experiment?.timestamp,
    allocator_algorithm: result.strategy,
    workload_type: experiment?.allocation_mode,
    allocation_mode: result.allocation_mode,
    fixed_block_size_bytes: experiment?.fixed_block_size,
    workload_configuration: experiment?.workload_configuration,
    total_operations: experiment?.total_operations,
    allocation_requests: result.allocation_requests,
    successful_allocations: result.successful_allocations,
    failed_allocations: result.failed_allocations,
    allocation_success_rate: result.allocation_success_rate,
    allocation_failure_rate: result.allocation_failure_rate,
    average_allocation_latency_ms: result.average_allocation_latency_ms,
    maximum_allocation_latency_ms: result.maximum_allocation_latency_ms,
    total_allocation_time_ms: result.total_allocation_time_ms,
    execution_time_ms: result.execution_time_ms,
    deallocation_requests: result.deallocation_requests,
    successful_deallocations: result.successful_deallocations,
    failed_deallocations: result.failed_deallocations,
    total_deallocation_time_ms: result.total_deallocation_time_ms,
    average_deallocation_latency_ms: result.average_deallocation_latency_ms,
    maximum_deallocation_latency_ms: result.maximum_deallocation_latency_ms,
    pool_capacity_bytes: experiment?.pool_capacity,
    allocated_memory_bytes: experiment?.allocated_memory,
    free_memory_bytes: experiment?.free_memory,
    peak_memory_usage_bytes: result.peak_memory_usage,
    peak_memory_utilization: result.peak_memory_utilization,
    final_memory_utilization: result.final_memory_utilization,
    internal_fragmentation: result.internal_fragmentation,
    external_fragmentation: result.external_fragmentation,
    largest_free_block_bytes: result.largest_free_block,
    free_block_count: result.free_block_count,
    block_splits: result.block_splits,
    coalescing_operations: result.coalescing_operations,
    comparison_baseline_experiment_id: baselineId,
    conditions_match: conditionsMatch === null ? '' : conditionsMatch,
  };
}

export function comparisonToCsv(results, experiments, conditionsMatch = null) {
  if (!Array.isArray(results) || results.length === 0) {
    throw new Error('Generate a comparative analysis before exporting.');
  }

  const experimentById = new Map((experiments ?? []).map((experiment) => [experiment.experiment_id, experiment]));
  const baselineId = results[0]?.experiment_id ?? '';
  const rows = results.map((result) => comparisonRow(result, experimentById.get(result.experiment_id), baselineId, conditionsMatch));
  const columns = [...COMPARISON_COLUMNS, ['comparison_baseline_experiment_id', 'Comparison baseline experiment ID']];
  const header = columns.map(([, label]) => csvEscape(label)).join(',');
  const body = rows.map((row) => columns.map(([key]) => csvEscape(row[key])).join(','));
  return `${header}\r\n${body.join('\r\n')}\r\n`;
}

export function comparisonFilename(date = new Date()) {
  const pad = (value) => String(value).padStart(2, '0');
  return `memforge_comparative_analysis_${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}_${pad(date.getHours())}-${pad(date.getMinutes())}-${pad(date.getSeconds())}.csv`;
}
