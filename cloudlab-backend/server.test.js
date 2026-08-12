const assert = require('node:assert/strict');
const http = require('node:http');
const test = require('node:test');

const { SerialJobQueue, createHandler, safeEqual } = require('./server');

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
  const token = 'a'.repeat(48);
  const queue = new SerialJobQueue(worker);
  const server = http.createServer(createHandler({ queue, worker, token }));
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  context.after(() => server.close());
  const port = server.address().port;

  assert.equal((await request(port, 'GET', '/v1/health')).status, 200);
  assert.equal((await request(port, 'GET', '/v1/cases', 'wrong')).status, 401);
  const first = await request(port, 'POST', '/v1/runs', token, {
    caseId: 'matrix-unrolling', action: 'run-and-profile',
  });
  const second = await request(port, 'POST', '/v1/runs', token, {
    caseId: 'matrix-unrolling', action: 'run',
  });
  assert.equal(first.status, 202);
  assert.equal(second.status, 202);

  await new Promise((resolve) => setTimeout(resolve, 50));
  const firstStatus = await request(port, 'GET', `/v1/runs/${first.body.runId}`, token);
  const secondStatus = await request(port, 'GET', `/v1/runs/${second.body.runId}`, token);
  assert.equal(firstStatus.body.state, 'complete');
  assert.equal(secondStatus.body.state, 'complete');
  assert.equal(maximumActive, 1);
});
