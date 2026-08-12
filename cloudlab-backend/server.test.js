const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const os = require('node:os');
const path = require('node:path');
const test = require('node:test');

const {
  SerialJobQueue,
  createHandler,
  readCredentials,
  safeEqual,
} = require('./server');

function request(port, method, route, token, body) {
  return new Promise((resolve, reject) => {
    const payload = body === undefined ? undefined : JSON.stringify(body);
    const operation = http.request({
      host: '127.0.0.1',
      port,
      method,
      path: route,
      headers: {
        ...(token ? { Authorization: `Bearer ${token}` } : {}),
        ...(payload ? {
          'Content-Type': 'application/json',
          'Content-Length': Buffer.byteLength(payload),
        } : {}),
      },
    }, (response) => {
      const chunks = [];
      response.on('data', (chunk) => chunks.push(chunk));
      response.on('end', () => resolve({
        status: response.statusCode,
        body: JSON.parse(Buffer.concat(chunks).toString('utf8')),
      }));
    });
    operation.on('error', reject);
    if (payload) operation.write(payload);
    operation.end();
  });
}

test('compares API tokens without accepting prefixes', () => {
  assert.equal(safeEqual('abc', 'abc'), true);
  assert.equal(safeEqual('abc', 'abcd'), false);
  assert.equal(safeEqual('abc', 'abd'), false);
});

test('loads separate revocable faculty credentials', (context) => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'eduperf-credentials-'));
  context.after(() => fs.rmSync(directory, { recursive: true, force: true }));
  const file = path.join(directory, 'credentials.json');
  fs.writeFileSync(file, JSON.stringify({
    schemaVersion: 1,
    credentials: [
      { id: 'faculty-a', label: 'Faculty A', token: 'a'.repeat(64) },
      { id: 'faculty-b@example.edu', label: 'Faculty B', token: 'b'.repeat(64) },
    ],
  }));
  assert.deepEqual(readCredentials(file).map(({ id, label }) => ({ id, label })), [
    { id: 'faculty-a', label: 'Faculty A' },
    { id: 'faculty-b@example.edu', label: 'Faculty B' },
  ]);
});

test('queues one measurement at a time and exposes authenticated status', async (context) => {
  let active = 0;
  let maximumActive = 0;
  const worker = {
    learningCase: async () => ({}),
    capabilities: async () => [{ caseId: 'matrix-unrolling', runtime: true, profile: true }],
    execute: async ({ runId, caseId, action, onProgress }) => {
      active += 1;
      maximumActive = Math.max(maximumActive, active);
      onProgress('measuring');
      await new Promise((resolve) => setTimeout(resolve, 15));
      active -= 1;
      return { schemaVersion: 1, runId, caseId, action };
    },
  };
  const credentials = [
    { id: 'faculty-a', label: 'Faculty A', token: 'a'.repeat(48) },
    { id: 'faculty-b', label: 'Faculty B', token: 'b'.repeat(48) },
  ];
  const queue = new SerialJobQueue(worker);
  const server = http.createServer(createHandler({ queue, worker, credentials }));
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  context.after(() => server.close());
  const port = server.address().port;

  assert.equal((await request(port, 'GET', '/v1/health')).status, 200);
  assert.equal((await request(port, 'GET', '/v1/cases', 'wrong')).status, 401);
  const first = await request(port, 'POST', '/v1/runs', credentials[0].token, {
    caseId: 'matrix-unrolling', action: 'run-and-profile',
  });
  const second = await request(port, 'POST', '/v1/runs', credentials[1].token, {
    caseId: 'matrix-unrolling', action: 'run',
  });
  assert.equal(first.status, 202);
  assert.equal(second.status, 202);

  await new Promise((resolve) => setTimeout(resolve, 50));
  const firstStatus = await request(
    port, 'GET', `/v1/runs/${first.body.runId}`, credentials[0].token,
  );
  const secondStatus = await request(
    port, 'GET', `/v1/runs/${second.body.runId}`, credentials[1].token,
  );
  assert.equal(firstStatus.body.state, 'complete');
  assert.equal(secondStatus.body.state, 'complete');
  assert.equal(firstStatus.body.requestedBy, 'faculty-a');
  assert.equal(secondStatus.body.requestedBy, 'faculty-b');
  assert.equal(maximumActive, 1);
});
