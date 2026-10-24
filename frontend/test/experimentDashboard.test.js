import test from 'node:test';
import assert from 'node:assert/strict';

import {
  getConditionDifferences,
  getMetricDelta,
  toggleComparisonSelection,
  COMPARISON_METRICS,
  formatMetricValue,
  getComparisonMetric,
  WORKLOAD_GRAPH_GROUPS,
} from '../src/experimentDashboard.js';

test('comparison selection adds, removes, and respects its limit', () => {
  assert.deepEqual(toggleComparisonSelection([], 'a'), ['a']);
  assert.deepEqual(toggleComparisonSelection(['a', 'b'], 'a'), ['b']);
  assert.deepEqual(toggleComparisonSelection(['a', 'b', 'c', 'd'], 'e'), ['a', 'b', 'c', 'd']);
});

test('condition checking detects workload and configuration mismatches', () => {
  const baseline = {
    pool_capacity: 1024,
    allocation_mode: 'variable_size',
    fixed_block_size: 64,
    workload_signature: 'allocate:32:0;',
    workload_configuration: 'Seeded random: operations=1, min=32, max=32',
    random_seed: 7,
  };
  assert.deepEqual(getConditionDifferences([baseline, { ...baseline }]), []);
  assert.deepEqual(
    getConditionDifferences([
      baseline,
      {
        ...baseline,
        pool_capacity: 2048,
        workload_signature: 'allocate:64:0;',
        workload_configuration: 'Seeded random: operations=1, min=64, max=64',
        random_seed: 8,
      },
    ]),
    ['Pool capacities differ', 'Workload operation sequences differ', 'Workload settings differ', 'Random seeds differ'],
  );
});

test('metric deltas encode direction and avoid division by zero', () => {
  const latency = COMPARISON_METRICS.find((metric) => metric.key === 'execution_time_ms');
  assert.deepEqual(getMetricDelta(75, 100, latency), { value: -25, improved: true, unchanged: false, label: '-25.0%' });
  assert.equal(getMetricDelta(100, 100, latency).unchanged, true);
  assert.equal(getMetricDelta(10, 0, latency), null);
});

test('fraction metrics are rendered as percentages', () => {
  const successRate = COMPARISON_METRICS.find((metric) => metric.key === 'allocation_success_rate');
  assert.equal(formatMetricValue(successRate, 0.875), '87.5%');
  const latency = COMPARISON_METRICS.find((metric) => metric.key === 'average_allocation_latency_ms');
  assert.equal(formatMetricValue(latency, 0.0035), '0.0035 ms');
});

test('workload graph groups reference available comparison metrics', () => {
  assert.equal(getComparisonMetric('average_allocation_latency_ms').label, 'Avg allocation latency');
  assert.equal(WORKLOAD_GRAPH_GROUPS.length, 4);
  assert.ok(WORKLOAD_GRAPH_GROUPS.every((group) => group.metrics.length >= 4));
});
