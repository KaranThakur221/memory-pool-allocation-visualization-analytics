import test from 'node:test';
import assert from 'node:assert/strict';

import {
  parseWorkloadText,
  WORKLOAD_CSV_TEMPLATE,
  workloadToCsv,
} from '../src/workloadCsv.js';

test('parses a valid CSV workload and preserves implicit allocation IDs', () => {
  const workload = parseWorkloadText(WORKLOAD_CSV_TEMPLATE);
  assert.deepEqual(workload, [
    { operation: 'allocate', size: 120 },
    { operation: 'allocate', size: 240 },
    { operation: 'free', allocation_id: 1 },
  ]);
});

test('supports CRLF and quoted fields without splitting commas inside quotes', () => {
  const csv = '\uFEFFoperation,size,allocation_id\r\n"allocate",120,\r\nallocate,240,7\r\nfree,,1\r\nfree,,"7"';
  assert.deepEqual(parseWorkloadText(csv), [
    { operation: 'allocate', size: 120 },
    { operation: 'allocate', size: 240, allocation_id: 7 },
    { operation: 'free', allocation_id: 1 },
    { operation: 'free', allocation_id: 7 },
  ]);
});

test('explicit allocations advance the implicit allocation ID sequence', () => {
  assert.deepEqual(parseWorkloadText([
    'operation,size,allocation_id',
    'allocate,16,40',
    'allocate,32,',
    'free,,40',
    'free,,2',
  ].join('\n')), [
    { operation: 'allocate', size: 16, allocation_id: 40 },
    { operation: 'allocate', size: 32 },
    { operation: 'free', allocation_id: 40 },
    { operation: 'free', allocation_id: 2 },
  ]);
});

test('rejects malformed rows and reports their source line', () => {
  assert.throws(
    () => parseWorkloadText('operation,size,allocation_id\nallocate,12,\nfree,wrong,1'),
    /CSV line 3: free must not include a size/,
  );
  assert.throws(
    () => parseWorkloadText('operation,size,allocation_id\nallocate,12'),
    /CSV line 2: expected exactly 3 columns/,
  );
});

test('rejects invalid operations, sizes, IDs, and unresolved frees', () => {
  assert.throws(() => parseWorkloadText('operation,size,allocation_id\nreserve,12,'), /line 2: operation must be allocate or free/);
  assert.throws(() => parseWorkloadText('operation,size,allocation_id\nallocate,0,'), /line 2: size must be a positive safe integer/);
  assert.throws(() => parseWorkloadText('operation,size,allocation_id\nfree,,900'), /line 2: allocation_id 900 has no earlier active allocation/);
  assert.throws(() => parseWorkloadText('operation,size,allocation_id\nallocate,12,3\nallocate,16,3'), /allocation_id 3 is already active/);
  assert.throws(() => parseWorkloadText('operation,size,allocation_id\nallocate,12,3\nfree,,3\nfree,,3'), /has no earlier active allocation/);
});

test('rejects invalid CSV quoting and incorrect headers instead of skipping rows', () => {
  assert.throws(() => parseWorkloadText('operation,size,allocation_id\nallocate,"12,'), /unterminated quoted field/);
  assert.throws(() => parseWorkloadText('operation,size,id\nallocate,12,'), /expected header: operation,size,allocation_id/);
  assert.throws(() => parseWorkloadText('operation,size,allocation_id\nallocate,"12"x,'), /unexpected character after a quoted field/);
  assert.throws(() => parseWorkloadText('operation,size,allocation_id\nallocate,"12,13",'), /size must be a positive whole number/);
});

test('validates JSON workloads and emits exportable CSV', () => {
  const source = JSON.stringify([
    { operation: 'allocate', size: 16 },
    { operation: 'free', allocation_id: 1 },
  ]);
  const workload = parseWorkloadText(source);
  assert.equal(workloadToCsv(workload), 'operation,size,allocation_id\nallocate,16,\nfree,,1');
  assert.throws(() => parseWorkloadText('[{"operation":"allocate","size":0}]'), /positive safe integer/);
  assert.throws(() => parseWorkloadText('[{"operation":"free","allocation_id":1}]'), /no earlier active allocation/);
});
