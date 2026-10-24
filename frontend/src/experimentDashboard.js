export const MAX_COMPARISON_EXPERIMENTS = 4;

export const COMPARISON_METRICS = [
  { key: 'execution_time_ms', label: 'Execution time', unit: 'ms', lowerIsBetter: true },
  { key: 'total_allocation_time_ms', label: 'Total allocation time', unit: 'ms', lowerIsBetter: true },
  { key: 'average_allocation_latency_ms', label: 'Avg allocation latency', unit: 'ms', lowerIsBetter: true },
  { key: 'median_allocation_latency_ms', label: 'Median allocation latency', unit: 'ms', lowerIsBetter: true },
  { key: 'maximum_allocation_latency_ms', label: 'Max allocation latency', unit: 'ms', lowerIsBetter: true },
  { key: 'p95_allocation_latency_ms', label: 'P95 allocation latency', unit: 'ms', lowerIsBetter: true },
  { key: 'p99_allocation_latency_ms', label: 'P99 allocation latency', unit: 'ms', lowerIsBetter: true },
  { key: 'allocation_success_rate', label: 'Allocation success rate', unit: '%', fraction: true, lowerIsBetter: false },
  { key: 'allocation_failure_rate', label: 'Allocation failure rate', unit: '%', fraction: true, lowerIsBetter: true },
  { key: 'peak_memory_utilization', label: 'Peak memory utilization', unit: '%', lowerIsBetter: null },
  { key: 'peak_memory_usage', label: 'Peak memory usage', unit: 'bytes', lowerIsBetter: null },
  { key: 'final_memory_utilization', label: 'Final memory utilization', unit: '%', lowerIsBetter: null },
  { key: 'internal_fragmentation', label: 'Internal fragmentation', unit: '%', lowerIsBetter: true },
  { key: 'external_fragmentation', label: 'External fragmentation', unit: '%', lowerIsBetter: true },
  { key: 'largest_free_block', label: 'Largest free block', unit: 'bytes', lowerIsBetter: false },
  { key: 'free_block_count', label: 'Free blocks', unit: 'blocks', lowerIsBetter: null },
  { key: 'allocation_requests', label: 'Allocation requests', unit: 'requests', lowerIsBetter: null },
  { key: 'successful_allocations', label: 'Successful allocations', unit: 'requests', lowerIsBetter: null },
  { key: 'failed_allocations', label: 'Failed allocations', unit: 'requests', lowerIsBetter: true },
  { key: 'total_deallocation_time_ms', label: 'Total deallocation time', unit: 'ms', lowerIsBetter: true },
  { key: 'average_deallocation_latency_ms', label: 'Avg deallocation latency', unit: 'ms', lowerIsBetter: true },
  { key: 'maximum_deallocation_latency_ms', label: 'Max deallocation latency', unit: 'ms', lowerIsBetter: true },
  { key: 'deallocation_requests', label: 'Deallocation requests', unit: 'requests', lowerIsBetter: null },
  { key: 'successful_deallocations', label: 'Successful deallocations', unit: 'requests', lowerIsBetter: null },
  { key: 'failed_deallocations', label: 'Failed deallocations', unit: 'requests', lowerIsBetter: true },
  { key: 'block_splits', label: 'Block splits', unit: 'operations', lowerIsBetter: null },
  { key: 'coalescing_operations', label: 'Coalescing operations', unit: 'operations', lowerIsBetter: null },
];

export const WORKLOAD_GRAPH_GROUPS = [
  {
    key: 'allocation-performance',
    title: 'Allocation performance',
    metrics: ['total_allocation_time_ms', 'average_allocation_latency_ms', 'median_allocation_latency_ms', 'p95_allocation_latency_ms', 'p99_allocation_latency_ms'],
  },
  {
    key: 'memory-utilization',
    title: 'Memory utilization',
    metrics: ['pool_capacity', 'allocated_memory', 'free_memory', 'peak_memory_utilization'],
  },
  {
    key: 'fragmentation',
    title: 'Fragmentation',
    metrics: ['internal_fragmentation', 'external_fragmentation', 'largest_free_block', 'free_block_count'],
  },
  {
    key: 'allocation-success',
    title: 'Allocation success',
    metrics: ['successful_allocations', 'failed_allocations', 'allocation_success_rate', 'allocation_failure_rate'],
  },
];

export function getComparisonMetric(key) {
  return COMPARISON_METRICS.find((metric) => metric.key === key) ?? null;
}

export function toggleComparisonSelection(selectedIds, experimentId, limit = MAX_COMPARISON_EXPERIMENTS) {
  if (selectedIds.includes(experimentId)) {
    return selectedIds.filter((id) => id !== experimentId);
  }
  if (selectedIds.length >= limit) {
    return selectedIds;
  }
  return [...selectedIds, experimentId];
}

export function getConditionDifferences(experiments) {
  if (experiments.length < 2) {
    return [];
  }

  const baseline = experiments[0];
  const differences = new Set();
  for (const experiment of experiments.slice(1)) {
    if (experiment.pool_capacity !== baseline.pool_capacity) differences.add('Pool capacities differ');
    if (experiment.allocation_mode !== baseline.allocation_mode) differences.add('Allocation modes differ');
    if (experiment.fixed_block_size !== baseline.fixed_block_size) differences.add('Fixed block sizes differ');
    if (experiment.workload_signature !== baseline.workload_signature) differences.add('Workload operation sequences differ');
    if (experiment.workload_configuration !== baseline.workload_configuration) differences.add('Workload settings differ');
    if (experiment.random_seed !== baseline.random_seed) differences.add('Random seeds differ');
  }
  return [...differences];
}

export function formatMetricValue(metric, value) {
  if (value === null || value === undefined || !Number.isFinite(Number(value))) {
    return '—';
  }
  const numeric = Number(value) * (metric.fraction ? 100 : 1);
  const precision = metric.unit === 'bytes' || metric.unit === 'blocks' || metric.unit === 'requests' || metric.unit === 'operations'
    ? 0
    : metric.unit === 'ms' && metric.key !== 'execution_time_ms' ? 4 : 2;
  return `${numeric.toLocaleString(undefined, { maximumFractionDigits: precision })}${metric.unit === '%' ? '%' : ` ${metric.unit}`}`;
}

export function getMetricDelta(value, baseline, metric) {
  const currentValue = Number(value);
  const baselineValue = Number(baseline);
  if (!Number.isFinite(currentValue) || !Number.isFinite(baselineValue) || baselineValue === 0) {
    return null;
  }

  const deltaPercent = ((currentValue - baselineValue) / Math.abs(baselineValue)) * 100;
  const neutral = metric.lowerIsBetter === null;
  const improved = neutral || deltaPercent === 0 ? null : metric.lowerIsBetter ? deltaPercent < 0 : deltaPercent > 0;
  const signed = deltaPercent > 0 ? '+' : '';
  return {
    value: deltaPercent,
    improved,
    unchanged: deltaPercent === 0,
    label: `${signed}${deltaPercent.toFixed(1)}%`,
  };
}
