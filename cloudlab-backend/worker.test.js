const assert = require('node:assert/strict');
const test = require('node:test');

const { cleanCheck } = require('./worker');
const {
  harmonizeVariantScopes,
  matchesAdaptedVariantTarget,
  profileFrequency,
  selectScopedMetrics,
  usesProfileSizedInput,
} = require('../workloads/src/hpctoolkit');
const { parseResult } = require('../workloads/src/runner');

test('uses the bounded profile path for known instrumentation-heavy Python cases', () => {
  assert.equal(usesProfileSizedInput({ id: 'p01', runner: { kind: 'python-bench' } }), true);
  assert.equal(usesProfileSizedInput({ id: 'p33', runner: { kind: 'python-bench' } }), true);
  assert.equal(usesProfileSizedInput({ id: 'e01', runner: { kind: 'python-bench' } }), false);
  assert.equal(usesProfileSizedInput({ id: 'p01', runner: { kind: 'cpp-driver' } }), false);
  assert.equal(profileFrequency({ id: 'p01', profiling: { sampleFrequencyHz: 1009 } }), 4009);
  assert.equal(profileFrequency({ id: 'e01', profiling: { sampleFrequencyHz: 1009 } }), 1009);
});

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

test('harmonizes mismatched variant scopes before comparing metrics', () => {
  const variants = {
    before: {
      metricScope: 'variant-inclusive',
      metrics: { instructions: 120, cycles: 80, ipc: 1.5 },
      profiledWorkloadMetrics: { instructions: 1200, cycles: 800 },
      context: [{ kind: 'target', label: 'v_before::target' }],
    },
    after: {
      metricScope: 'profiled-workload',
      metrics: { instructions: 900, cycles: 700 },
      profiledWorkloadMetrics: { instructions: 900, cycles: 700 },
      context: [],
    },
  };
  harmonizeVariantScopes(variants);
  assert.equal(variants.before.metricScope, 'profiled-workload');
  assert.equal(variants.after.metricScope, 'profiled-workload');
  assert.deepEqual(variants.before.metrics, {
    instructions: 1200, cycles: 800, ipc: 1.5, branchMissRate: null,
  });
  assert.deepEqual(variants.after.metrics, {
    instructions: 900, cycles: 700, ipc: 900 / 700, branchMissRate: null,
  });
  assert.equal(variants.before.profiledWorkloadMetrics, undefined);
  assert.equal(variants.after.profiledWorkloadMetrics, undefined);
  assert.equal(variants.before.context[0].kind, 'target');
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
