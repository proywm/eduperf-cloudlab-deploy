const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const test = require('node:test');

const { EduPerfWorker } = require('./worker');

test('the deployed worker discovers 100 cases and executes a fresh comparison', async (context) => {
  const workRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'eduperf-worker-test-'));
  context.after(() => fs.rm(workRoot, { recursive: true, force: true }));
  const worker = new EduPerfWorker({
    workloadDirectory: path.resolve(__dirname, '..', 'workloads'),
    workRoot,
    workerLabel: 'integration-test',
    nodeType: 'test-node',
    backendRevision: 'integration-test-revision',
  });

  const capabilities = await worker.capabilities();
  assert.equal(capabilities.length, 100);
  assert.equal(capabilities.filter((entry) => entry.profile).length, 100);
  assert.equal(capabilities.find((entry) => entry.caseId === 'e14').profile, true);

  const result = await worker.execute({
    runId: 'integration-run',
    caseId: 'matrix-unrolling',
    action: 'run',
  });
  assert.equal(result.runtime.check.status, 'pass');
  assert.equal(result.evidenceProtocol, 3);
  assert.equal(result.backendRevision, 'integration-test-revision');
  assert.match(result.adapterHash, /^[a-f0-9]{64}$/);
  assert.ok(result.runtime.benchmark.median_speedup > 0);
  assert.equal(result.environment.worker, 'integration-test');
  assert.equal(result.environment.nodeType, 'test-node');
});
