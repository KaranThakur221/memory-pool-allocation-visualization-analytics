import test from 'node:test';
import assert from 'node:assert/strict';

import {
  createRandomWorkloadRequest,
  validateRandomWorkloadConfiguration,
} from '../src/randomWorkload.js';

const baseConfiguration = {
  operation_count: 40,
  minimum_size: 16,
  maximum_size: 256,
  allocation_probability: 70,
  deallocation_probability: 30,
  random_seed: 42,
  strategy: 'first_fit',
  allocation_mode: 'variable_size',
  fixed_block_size: 256,
};

test('validates all placement algorithm and allocation mode combinations', () => {
  for (const strategy of ['first_fit', 'best_fit']) {
    for (const allocation_mode of ['fixed_size', 'variable_size']) {
      const configuration = { ...baseConfiguration, strategy, allocation_mode };
      assert.equal(validateRandomWorkloadConfiguration(configuration, 4096), '');
    }
  }
});

test('requires a compatible fixed block size only in fixed-size mode', () => {
  const fixed = { ...baseConfiguration, allocation_mode: 'fixed_size', fixed_block_size: 128 };
  assert.match(validateRandomWorkloadConfiguration(fixed, 4096), /at least the maximum allocation size/);
  assert.equal(validateRandomWorkloadConfiguration({
    ...baseConfiguration,
    allocation_mode: 'variable_size',
    fixed_block_size: 1,
  }, 4096), '');
});

test('validates workload settings, probability totals, and selection values', () => {
  assert.match(validateRandomWorkloadConfiguration({ ...baseConfiguration, operation_count: 0 }, 4096), /Operations/);
  assert.match(validateRandomWorkloadConfiguration({ ...baseConfiguration, minimum_size: 257 }, 4096), /Allocation sizes/);
  assert.match(validateRandomWorkloadConfiguration({ ...baseConfiguration, allocation_probability: 60 }, 4096), /total 100%/);
  assert.match(validateRandomWorkloadConfiguration({ ...baseConfiguration, random_seed: -1 }, 4096), /Seed/);
  assert.match(validateRandomWorkloadConfiguration({ ...baseConfiguration, strategy: 'unknown' }, 4096), /algorithm/);
  assert.match(validateRandomWorkloadConfiguration({ ...baseConfiguration, allocation_mode: 'unknown' }, 4096), /mode/);
});

test('builds an API payload carrying independent algorithm, mode, and fixed-block settings', () => {
  const request = createRandomWorkloadRequest({
    ...baseConfiguration,
    strategy: 'best_fit',
    allocation_mode: 'fixed_size',
    fixed_block_size: 256,
  });
  assert.deepEqual(request, {
    operation_count: 40,
    minimum_size: 16,
    maximum_size: 256,
    allocation_probability: 70,
    deallocation_probability: 30,
    random_seed: 42,
    strategy: 'best_fit',
    allocation_mode: 'fixed_size',
    fixed_block_size: 256,
  });
});
