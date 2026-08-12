const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const os = require('node:os');
const path = require('node:path');
const test = require('node:test');

const { EmailAuthManager } = require('./auth');
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

test('email code signs in without a connection file and isolates each user run', async (context) => {
  const messages = [];
  const authManager = new EmailAuthManager({
    allowedEmails: new Set(['probirr@umich.edu', 'sjiao2@ncsu.edu']),
    secret: Buffer.alloc(48, 3),
    mailer: {
      kind: 'capture',
      deliveryReady: true,
      async sendCode(email, code) { messages.push({ email, code }); },
    },
    codeGenerator: () => '842019',
  });
  const worker = {
    learningCase: async () => ({}),
    capabilities: async () => [{ caseId: 'matrix-unrolling', runtime: true, profile: true }],
    execute: async ({ runId, caseId, action }) => ({ runId, caseId, action }),
  };
  const queue = new SerialJobQueue(worker);
  const server = http.createServer(createHandler({
    queue, worker, credentials: [], authManager,
  }));
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  context.after(() => server.close());
  const port = server.address().port;

  const config = await request(port, 'GET', '/v1/auth/config');
  assert.equal(config.body.mode, 'email-code');
  assert.equal(config.body.deliveryReady, true);
  const requested = await request(port, 'POST', '/v1/auth/request-code', undefined, {
    email: 'probirr@umich.edu',
  });
  assert.equal(requested.status, 202);
  assert.deepEqual(messages, [{ email: 'probirr@umich.edu', code: '842019' }]);
  const verified = await request(port, 'POST', '/v1/auth/verify-code', undefined, {
    email: 'probirr@umich.edu', code: '842019',
  });
  assert.equal(verified.status, 200);
  assert.equal(verified.body.email, 'probirr@umich.edu');

  const cases = await request(port, 'GET', '/v1/cases', verified.body.token);
  assert.equal(cases.status, 200);
  assert.equal(cases.body.authenticatedAs, 'probirr@umich.edu');
  const submitted = await request(port, 'POST', '/v1/runs', verified.body.token, {
    caseId: 'matrix-unrolling', action: 'run',
  });
  assert.equal(submitted.status, 202);
  await new Promise((resolve) => setTimeout(resolve, 10));

  await request(port, 'POST', '/v1/auth/request-code', undefined, {
    email: 'sjiao2@ncsu.edu',
  });
  const second = await request(port, 'POST', '/v1/auth/verify-code', undefined, {
    email: 'sjiao2@ncsu.edu', code: '842019',
  });
  assert.equal(second.status, 200);
  assert.equal((await request(
    port, 'GET', `/v1/runs/${submitted.body.runId}`, second.body.token,
  )).status, 404);
  assert.equal((await request(
    port, 'GET', `/v1/runs/${submitted.body.runId}`, verified.body.token,
  )).status, 200);
});

test('email-provider failures do not disclose allowlist membership', async (context) => {
  const authManager = new EmailAuthManager({
    allowedEmails: new Set(['probirr@umich.edu']),
    secret: Buffer.alloc(48, 4),
    mailer: {
      kind: 'failing-provider',
      deliveryReady: true,
      async sendCode() {
        const error = new Error('provider unavailable');
        error.statusCode = 503;
        throw error;
      },
    },
    codeGenerator: () => '842019',
  });
  const worker = { capabilities: async () => [], learningCase: async () => ({}) };
  const queue = new SerialJobQueue(worker);
  const server = http.createServer(createHandler({
    queue, worker, credentials: [], authManager,
  }));
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  context.after(() => server.close());
  const port = server.address().port;

  const originalWrite = process.stderr.write;
  process.stderr.write = () => true;
  context.after(() => { process.stderr.write = originalWrite; });
  const allowed = await request(port, 'POST', '/v1/auth/request-code', undefined, {
    email: 'probirr@umich.edu',
  });
  const unknown = await request(port, 'POST', '/v1/auth/request-code', undefined, {
    email: 'unknown@example.edu',
  });
  process.stderr.write = originalWrite;
  assert.equal(allowed.status, 202);
  assert.deepEqual(allowed.body, unknown.body);
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
