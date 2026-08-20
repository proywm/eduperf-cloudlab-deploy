const assert = require('node:assert/strict');
const test = require('node:test');

const { cleanCheck } = require('./worker');

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
