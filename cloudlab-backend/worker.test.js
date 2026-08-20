const assert = require('node:assert/strict');
const test = require('node:test');

const { cleanCheck } = require('./worker');
const { matchesAdaptedVariantTarget, selectScopedMetrics } = require('../workloads/src/hpctoolkit');
const { parseResult } = require('../workloads/src/runner');

test('preserves bounded conditional behavior domains in API results', () => {
  assert.deepEqual(cleanCheck({
    status: 'pass',
    kind: 'fresh',
    cases: 200000,
    scope: 'stated-precondition',
    domains: [
      {
        id: 'preserved-precondition', label: 'Within the stated precondition',
        cases: 200000, mismatches: 0, validForDecision: true,
      },
      {
        id: 'outside-precondition', label: 'Outside the stated precondition',
        cases: 200000, mismatches: 137616, validForDecision: false,
      },
    ],
  }), {
    status: 'pass', kind: 'fresh', cases: 200000, message: undefined,
    scope: 'stated-precondition',
    domains: [
      {
        id: 'preserved-precondition', label: 'Within the stated precondition',
        cases: 200000, mismatches: 0, validForDecision: true,
      },
      {
        id: 'outside-precondition', label: 'Outside the stated precondition',
        cases: 200000, mismatches: 137616, validForDecision: false,
      },
    ],
  });
});

test('matches only the explicit adapted C++ variant namespace', () => {
  assert.equal(matchesAdaptedVariantTarget('v_before::lookup(int)', 'before'), true);
  assert.equal(matchesAdaptedVariantTarget('outer::v_after::lookup(int)', 'after'), true);
  assert.equal(matchesAdaptedVariantTarget('std::_M_find_before_node', 'before'), false);
  assert.equal(matchesAdaptedVariantTarget('before_helper', 'before'), false);
});

test('keeps all profile metrics on one attribution scope', () => {
  assert.deepEqual(selectScopedMetrics(
    { metrics: { instructions: 120 } },
    { instructions: 900, cycles: 450 },
    ['instructions', 'cycles'],
  ), {
    metrics: { instructions: 120, ipc: null, branchMissRate: null },
    metricScope: 'variant-inclusive',
  });
  assert.deepEqual(selectScopedMetrics(
    undefined,
    { instructions: 900, cycles: 450 },
    ['instructions', 'cycles'],
  ), {
    metrics: { instructions: 900, cycles: 450, ipc: 2, branchMissRate: null },
    metricScope: 'profiled-workload',
  });
});

test('preserves seven round samples emitted by an enhanced adapter', () => {
  const parsed = parseResult(
    'PERFBANK_RESULT mode=benchmark status=pass rounds=7 median_speedup=2 before_us=20 after_us=10 before_samples_us=19,20,21 after_samples_us=9,10,11 speedup_samples=2.1,2,1.9',
    'benchmark',
  );
  assert.deepEqual(parsed.samples, {
    before: [0.000019, 0.00002, 0.000021],
    after: [0.000009, 0.00001, 0.000011],
  });
  assert.deepEqual(parsed.speedup_samples, [2.1, 2, 1.9]);
});
