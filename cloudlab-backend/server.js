const crypto = require('node:crypto');
const fs = require('node:fs');
const http = require('node:http');
const https = require('node:https');
const path = require('node:path');

const { createEmailAuthManager } = require('./auth');
const { ACTIONS, EduPerfWorker } = require('./worker');

const MAX_BODY_BYTES = 64 * 1024;
const MAX_JOBS = 500;
const MAX_QUEUED_JOBS = 250;
const MAX_USER_OUTSTANDING_JOBS = 3;

function safeEqual(left, right) {
  const leftBuffer = Buffer.from(String(left || ''));
  const rightBuffer = Buffer.from(String(right || ''));
  return leftBuffer.length === rightBuffer.length
    && crypto.timingSafeEqual(leftBuffer, rightBuffer);
}

function readCredentials(filePath) {
  const contents = fs.readFileSync(filePath, 'utf8').trim();
  if (contents.startsWith('{')) {
    const document = JSON.parse(contents);
    if (document.schemaVersion !== 1 || !Array.isArray(document.credentials)) {
      throw new Error('The API credentials file uses an unsupported schema.');
    }
    if (document.credentials.length < 1 || document.credentials.length > 100) {
      throw new Error('The API credentials file must contain 1 to 100 credentials.');
    }
    const ids = new Set();
    return document.credentials.map((credential) => {
      const id = String(credential.id || '');
      const label = String(credential.label || '');
      const token = String(credential.token || '');
      if (!/^[A-Za-z0-9@._+-]{1,128}$/.test(id) || !label || token.length < 32) {
        throw new Error('The API credentials file contains an invalid credential.');
      }
      if (ids.has(id)) throw new Error(`Duplicate API credential id: ${id}`);
      ids.add(id);
      return { id, label, token };
    });
  }
  if (contents.length < 32) throw new Error('The API token must contain at least 32 characters.');
  return [{ id: 'instructor', label: 'Instructor', token: contents }];
}

function send(response, status, value) {
  const body = `${JSON.stringify(value)}\n`;
  response.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Content-Length': Buffer.byteLength(body),
    'Cache-Control': 'no-store',
    'X-Content-Type-Options': 'nosniff',
  });
  response.end(body);
}

async function readJson(request) {
  let size = 0;
  const chunks = [];
  for await (const chunk of request) {
    size += chunk.length;
    if (size > MAX_BODY_BYTES) throw new Error('Request body is too large.');
    chunks.push(chunk);
  }
  try {
    return JSON.parse(Buffer.concat(chunks).toString('utf8'));
  } catch {
    throw new Error('Request body must be valid JSON.');
  }
}

function publicJob(job) {
  return {
    runId: job.runId,
    caseId: job.caseId,
    action: job.action,
    requestedBy: job.requestedBy,
    state: job.state,
    queuedAt: job.queuedAt,
    startedAt: job.startedAt,
    completedAt: job.completedAt,
    progress: job.progress,
    position: job.position,
    result: job.result,
    error: job.error,
  };
}

class SerialJobQueue {
  constructor(worker) {
    this.worker = worker;
    this.jobs = new Map();
    this.pending = [];
    this.running = false;
    this.activeJob = undefined;
  }

  enqueue(caseId, action, requestedBy = 'instructor') {
    if (!ACTIONS.has(action)) throw new Error(`Unsupported action: ${action}`);
    const userOutstanding = this.pending.filter(
      (job) => job.requestedBy === requestedBy,
    ).length + (this.activeJob?.requestedBy === requestedBy ? 1 : 0);
    if (userOutstanding >= MAX_USER_OUTSTANDING_JOBS) {
      const error = new Error(
        'You already have three measurements queued or running. Wait for one to finish.',
      );
      error.statusCode = 429;
      throw error;
    }
    if (this.pending.length >= MAX_QUEUED_JOBS) {
      const error = new Error('The classroom measurement queue is full. Try again shortly.');
      error.statusCode = 503;
      throw error;
    }
    const runId = crypto.randomUUID();
    const job = {
      runId,
      caseId,
      action,
      requestedBy,
      state: 'queued',
      queuedAt: new Date().toISOString(),
      progress: ['Waiting for the dedicated measurement worker'],
    };
    this.jobs.set(runId, job);
    this.pending.push(job);
    this.updatePositions();
    this.trim();
    void this.drain();
    return job;
  }

  updatePositions() {
    this.pending.forEach((job, index) => { job.position = index + (this.running ? 1 : 0); });
  }

  async drain() {
    if (this.running) return;
    this.running = true;
    while (this.pending.length > 0) {
      const job = this.pending.shift();
      this.activeJob = job;
      delete job.position;
      this.updatePositions();
      job.state = 'running';
      job.startedAt = new Date().toISOString();
      try {
        job.result = await this.worker.execute({
          runId: job.runId,
          caseId: job.caseId,
          action: job.action,
          onProgress: (message) => {
            job.progress.push(String(message));
            job.progress = job.progress.slice(-20);
          },
        });
        job.state = 'complete';
      } catch (error) {
        job.state = 'failed';
        job.error = sanitizeError(error);
      }
      job.completedAt = new Date().toISOString();
      this.activeJob = undefined;
    }
    this.running = false;
  }

  get(runId) {
    return this.jobs.get(runId);
  }

  trim() {
    if (this.jobs.size <= MAX_JOBS) return;
    for (const [runId, job] of this.jobs) {
      if (['complete', 'failed'].includes(job.state)) this.jobs.delete(runId);
      if (this.jobs.size <= MAX_JOBS) break;
    }
  }
}

function sanitizeError(error) {
  return String(error?.message || error || 'Unknown backend failure')
    .replaceAll(process.env.EDUPERF_WORK_DIR || '/local/eduperf/work', '[work]')
    .split('\n')
    .slice(0, 12)
    .join('\n')
    .slice(0, 4000);
}

function createHandler({ queue, worker, token, credentials, authManager }) {
  const acceptedCredentials = credentials || [
    { id: 'instructor', label: 'Instructor', token },
  ];
  return async (request, response) => {
    try {
      const url = new URL(request.url, 'https://eduperf.invalid');
      if (request.method === 'GET' && url.pathname === '/v1/health') {
        send(response, 200, {
          status: 'ready',
          service: 'eduperf-cloudlab-worker',
          schemaVersion: 1,
          queueDepth: queue.pending.length + (queue.running ? 1 : 0),
        });
        return;
      }

      if (request.method === 'GET' && url.pathname === '/v1/auth/config') {
        send(response, 200, authManager
          ? { schemaVersion: 1, ...authManager.configuration() }
          : { schemaVersion: 1, mode: 'connection-file', deliveryReady: false });
        return;
      }

      if (request.method === 'POST' && url.pathname === '/v1/auth/request-code') {
        if (!authManager) {
          send(response, 404, { error: 'Email authentication is not enabled.' });
          return;
        }
        const body = await readJson(request);
        try {
          await authManager.requestCode(body.email, request.socket.remoteAddress || 'unknown');
        } catch (error) {
          // Do not let a provider outage reveal that one address is allowlisted
          // while another is not. Validation and throttling errors remain useful
          // client feedback; delivery errors are recorded only in service logs.
          if (Number(error?.statusCode) !== 503) throw error;
          process.stderr.write(`EduPerf sign-in email delivery failed: ${sanitizeError(error)}\n`);
        }
        send(response, 202, {
          accepted: true,
          message: 'If this email is allowed, a one-time code has been sent.',
        });
        return;
      }

      if (request.method === 'POST' && url.pathname === '/v1/auth/verify-code') {
        if (!authManager) {
          send(response, 404, { error: 'Email authentication is not enabled.' });
          return;
        }
        const body = await readJson(request);
        send(response, 200, { schemaVersion: 1, ...authManager.verifyCode(body.email, body.code) });
        return;
      }

      const authorization = request.headers.authorization || '';
      const suppliedToken = authorization.startsWith('Bearer ')
        ? authorization.slice(7)
        : '';
      const credential = acceptedCredentials.find(
        (candidate) => safeEqual(suppliedToken, candidate.token),
      ) || authManager?.authenticate(suppliedToken);
      if (!credential) {
        send(response, 401, { error: 'Unauthorized' });
        return;
      }

      if (request.method === 'GET' && url.pathname === '/v1/cases') {
        send(response, 200, {
          schemaVersion: 1,
          authenticatedAs: credential.id,
          cases: await worker.capabilities(),
        });
        return;
      }

      if (request.method === 'POST' && url.pathname === '/v1/runs') {
        const body = await readJson(request);
        if (typeof body.caseId !== 'string' || !/^[a-z0-9-]+$/.test(body.caseId)) {
          send(response, 400, { error: 'A valid caseId is required.' });
          return;
        }
        if (!ACTIONS.has(body.action)) {
          send(response, 400, {
            error: 'action must be verify, run, profile, or run-and-profile.',
          });
          return;
        }
        await worker.learningCase(body.caseId);
        const job = queue.enqueue(body.caseId, body.action, credential.id);
        send(response, 202, {
          runId: job.runId,
          state: job.state,
          statusPath: `/v1/runs/${job.runId}`,
        });
        return;
      }

      const runMatch = url.pathname.match(/^\/v1\/runs\/([0-9a-f-]{36})$/);
      if (request.method === 'GET' && runMatch) {
        const job = queue.get(runMatch[1]);
        if (!job || job.requestedBy !== credential.id) {
          send(response, 404, { error: 'Run not found.' });
          return;
        }
        send(response, 200, publicJob(job));
        return;
      }

      send(response, 404, { error: 'Not found' });
    } catch (error) {
      send(response, Number(error?.statusCode) || 400, { error: sanitizeError(error) });
    }
  };
}

async function main() {
  const repositoryRoot = path.resolve(__dirname, '..');
  const workloadDirectory = process.env.EDUPERF_WORKLOAD_DIR
    || path.join(repositoryRoot, 'workloads');
  const workRoot = process.env.EDUPERF_WORK_DIR || '/local/eduperf/work';
  const allowlistFile = process.env.EDUPERF_ALLOWED_EMAILS_FILE || '';
  const authSecretFile = process.env.EDUPERF_AUTH_SECRET_FILE || '';
  const credentialsFile = process.env.EDUPERF_API_CREDENTIALS_FILE
    || process.env.EDUPERF_API_TOKEN_FILE
    || (!allowlistFile ? '/local/eduperf/api-token' : '');
  const worker = new EduPerfWorker({
    workloadDirectory,
    workRoot,
    hpctoolkitRoot: process.env.EDUPERF_HPCTOOLKIT_ROOT || '',
  });
  await fs.promises.mkdir(workRoot, { recursive: true });
  await worker.cases();
  const credentials = credentialsFile ? readCredentials(credentialsFile) : [];
  const authManager = allowlistFile && authSecretFile
    ? createEmailAuthManager({ allowlistFile, secretFile: authSecretFile })
    : undefined;
  if (credentials.length === 0 && !authManager) {
    throw new Error('No EduPerf authentication method is configured.');
  }
  const queue = new SerialJobQueue(worker);
  const handler = createHandler({ queue, worker, credentials, authManager });
  const port = Number(process.env.EDUPERF_PORT || 8443);
  const host = process.env.EDUPERF_HOST || '0.0.0.0';
  const certPath = process.env.EDUPERF_TLS_CERT || '';
  const keyPath = process.env.EDUPERF_TLS_KEY || '';
  const server = certPath && keyPath
    ? https.createServer({ cert: fs.readFileSync(certPath), key: fs.readFileSync(keyPath) }, handler)
    : http.createServer(handler);
  server.requestTimeout = 30_000;
  server.headersTimeout = 10_000;
  server.listen(port, host, () => {
    process.stdout.write(JSON.stringify({
      event: 'ready',
      protocol: certPath && keyPath ? 'https' : 'http',
      host,
      port,
    }) + '\n');
  });
}

if (require.main === module) {
  main().catch((error) => {
    process.stderr.write(`${error.stack || error}\n`);
    process.exitCode = 1;
  });
}

module.exports = {
  SerialJobQueue,
  createHandler,
  publicJob,
  readCredentials,
  readJson,
  safeEqual,
  sanitizeError,
};
