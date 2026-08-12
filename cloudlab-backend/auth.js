const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

const CODE_TTL_MS = 10 * 60 * 1000;
const SESSION_TTL_MS = 7 * 24 * 60 * 60 * 1000;
const REQUEST_INTERVAL_MS = 60 * 1000;
const MAX_REQUESTS_PER_HOUR = 5;
const MAX_SOURCE_REQUESTS_PER_HOUR = 30;
const MAX_VERIFY_ATTEMPTS = 5;

class AuthError extends Error {
  constructor(message, statusCode) {
    super(message);
    this.name = 'AuthError';
    this.statusCode = statusCode;
  }
}

function normalizeEmail(value) {
  const email = String(value || '').trim().toLowerCase();
  if (email.length > 254 || !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)) {
    throw new AuthError('Enter a valid institutional email address.', 400);
  }
  return email;
}

function loadAllowedEmails(filePath) {
  const emails = fs.readFileSync(filePath, 'utf8')
    .split(/\r?\n/)
    .map((line) => line.replace(/\s+#.*$/, '').trim())
    .filter(Boolean)
    .map(normalizeEmail);
  if (emails.length < 1) throw new Error('The email allowlist is empty.');
  return new Set(emails);
}

function loadSecret(filePath) {
  const secret = fs.readFileSync(filePath);
  if (secret.length < 32) throw new Error('The email-auth secret must contain at least 32 bytes.');
  return secret;
}

function hmac(secret, value) {
  return crypto.createHmac('sha256', secret).update(value).digest();
}

function safeEqual(left, right) {
  const a = Buffer.isBuffer(left) ? left : Buffer.from(String(left || ''));
  const b = Buffer.isBuffer(right) ? right : Buffer.from(String(right || ''));
  return a.length === b.length && crypto.timingSafeEqual(a, b);
}

class ResendMailer {
  constructor({ apiKey, from }) {
    this.apiKey = apiKey;
    this.from = from;
    this.kind = 'resend';
    this.deliveryReady = Boolean(apiKey && from);
  }

  async sendCode(email, code) {
    if (!this.deliveryReady) throw new AuthError('Email delivery is not configured.', 503);
    const response = await fetch('https://api.resend.com/emails', {
      method: 'POST',
      headers: {
        Authorization: `Bearer ${this.apiKey}`,
        'Content-Type': 'application/json',
        'Idempotency-Key': crypto.randomUUID(),
        'User-Agent': 'EduPerf email authentication/1',
      },
      body: JSON.stringify({
        from: this.from,
        to: [email],
        subject: 'Your EduPerf sign-in code',
        text: `Your EduPerf sign-in code is ${code}. It expires in 10 minutes and can be used once.`,
      }),
      signal: AbortSignal.timeout(15_000),
    });
    if (!response.ok) {
      throw new AuthError(`Email delivery failed (${response.status}).`, 503);
    }
  }
}

class FileOutboxMailer {
  constructor(directory) {
    this.directory = directory;
    this.kind = 'test-outbox';
    this.deliveryReady = false;
  }

  async sendCode(email, code) {
    fs.mkdirSync(this.directory, { recursive: true, mode: 0o700 });
    const record = {
      email,
      code,
      createdAt: new Date().toISOString(),
    };
    const file = path.join(this.directory, `${crypto.randomUUID()}.json`);
    fs.writeFileSync(file, `${JSON.stringify(record)}\n`, { mode: 0o600 });
  }
}

class DisabledMailer {
  constructor() {
    this.kind = 'disabled';
    this.deliveryReady = false;
  }

  async sendCode() {
    throw new AuthError('Email delivery is not configured.', 503);
  }
}

function createMailer(environment = process.env) {
  if (environment.EDUPERF_RESEND_API_KEY && environment.EDUPERF_EMAIL_FROM) {
    return new ResendMailer({
      apiKey: environment.EDUPERF_RESEND_API_KEY,
      from: environment.EDUPERF_EMAIL_FROM,
    });
  }
  if (environment.EDUPERF_EMAIL_OUTBOX) {
    return new FileOutboxMailer(path.resolve(environment.EDUPERF_EMAIL_OUTBOX));
  }
  return new DisabledMailer();
}

class EmailAuthManager {
  constructor({
    allowedEmails,
    secret,
    mailer,
    now = () => Date.now(),
    codeGenerator = () => String(crypto.randomInt(0, 1_000_000)).padStart(6, '0'),
  }) {
    this.allowedEmails = allowedEmails;
    this.secret = secret;
    this.mailer = mailer;
    this.now = now;
    this.codeGenerator = codeGenerator;
    this.challenges = new Map();
    this.emailRequests = new Map();
    this.sourceRequests = new Map();
  }

  configuration() {
    return {
      mode: 'email-code',
      deliveryReady: this.mailer.deliveryReady,
      delivery: this.mailer.kind,
      codeDigits: 6,
      codeExpiresMinutes: CODE_TTL_MS / 60_000,
      sessionExpiresDays: SESSION_TTL_MS / 86_400_000,
    };
  }

  enforceRequestLimit(email, source) {
    const now = this.now();
    const hourAgo = now - 60 * 60 * 1000;
    const emailKey = `${source}\0${email}`;
    const recentForEmail = (this.emailRequests.get(emailKey) || [])
      .filter((time) => time > hourAgo);
    const recentForSource = (this.sourceRequests.get(source) || [])
      .filter((time) => time > hourAgo);
    if (recentForEmail.length > 0 && now - recentForEmail.at(-1) < REQUEST_INTERVAL_MS) {
      throw new AuthError('Please wait one minute before requesting another code.', 429);
    }
    if (recentForEmail.length >= MAX_REQUESTS_PER_HOUR
        || recentForSource.length >= MAX_SOURCE_REQUESTS_PER_HOUR) {
      throw new AuthError('Too many sign-in codes were requested. Try again later.', 429);
    }
    recentForEmail.push(now);
    recentForSource.push(now);
    this.emailRequests.set(emailKey, recentForEmail);
    this.sourceRequests.set(source, recentForSource);

    // Bound state created by invalid-address probes without penalizing normal classrooms.
    if (this.emailRequests.size > 5_000) {
      for (const [key, times] of this.emailRequests) {
        if (!times.some((time) => time > hourAgo)) this.emailRequests.delete(key);
      }
    }
    if (this.sourceRequests.size > 5_000) {
      for (const [key, times] of this.sourceRequests) {
        if (!times.some((time) => time > hourAgo)) this.sourceRequests.delete(key);
      }
    }
  }

  async requestCode(value, source = 'unknown') {
    const email = normalizeEmail(value);
    this.enforceRequestLimit(email, source);
    if (!this.allowedEmails.has(email)) {
      return { accepted: true };
    }
    const code = this.codeGenerator();
    if (!/^\d{6}$/.test(code)) throw new Error('The email code generator returned an invalid code.');
    const salt = crypto.randomBytes(16).toString('base64url');
    this.challenges.set(email, {
      digest: hmac(this.secret, `${email}\0${salt}\0${code}`),
      salt,
      expiresAt: this.now() + CODE_TTL_MS,
      attempts: 0,
    });
    try {
      await this.mailer.sendCode(email, code);
    } catch (error) {
      this.challenges.delete(email);
      throw error;
    }
    return { accepted: true };
  }

  verifyCode(value, codeValue) {
    const email = normalizeEmail(value);
    const code = String(codeValue || '').trim();
    const challenge = this.challenges.get(email);
    if (!challenge || challenge.expiresAt <= this.now() || challenge.attempts >= MAX_VERIFY_ATTEMPTS) {
      this.challenges.delete(email);
      throw new AuthError('The sign-in code is invalid or expired.', 401);
    }
    challenge.attempts += 1;
    const digest = hmac(this.secret, `${email}\0${challenge.salt}\0${code}`);
    if (!safeEqual(digest, challenge.digest)) {
      if (challenge.attempts >= MAX_VERIFY_ATTEMPTS) this.challenges.delete(email);
      throw new AuthError('The sign-in code is invalid or expired.', 401);
    }
    this.challenges.delete(email);
    return {
      email,
      token: this.issueSession(email),
      expiresAt: new Date(this.now() + SESSION_TTL_MS).toISOString(),
    };
  }

  issueSession(email) {
    const payload = Buffer.from(JSON.stringify({
      version: 1,
      email,
      expiresAt: this.now() + SESSION_TTL_MS,
    })).toString('base64url');
    const signature = hmac(this.secret, `eduperf-session\0${payload}`).toString('base64url');
    return `v1.${payload}.${signature}`;
  }

  authenticate(token) {
    const match = String(token || '').match(/^v1\.([A-Za-z0-9_-]+)\.([A-Za-z0-9_-]+)$/);
    if (!match) return undefined;
    const expected = hmac(this.secret, `eduperf-session\0${match[1]}`);
    let supplied;
    try {
      supplied = Buffer.from(match[2], 'base64url');
    } catch {
      return undefined;
    }
    if (!safeEqual(expected, supplied)) return undefined;
    let payload;
    try {
      payload = JSON.parse(Buffer.from(match[1], 'base64url').toString('utf8'));
    } catch {
      return undefined;
    }
    if (payload.version !== 1 || payload.expiresAt <= this.now()) return undefined;
    let email;
    try {
      email = normalizeEmail(payload.email);
    } catch {
      return undefined;
    }
    if (!this.allowedEmails.has(email)) return undefined;
    return { id: email, label: email };
  }
}

function createEmailAuthManager({ allowlistFile, secretFile, environment = process.env }) {
  return new EmailAuthManager({
    allowedEmails: loadAllowedEmails(allowlistFile),
    secret: loadSecret(secretFile),
    mailer: createMailer(environment),
  });
}

module.exports = {
  AuthError,
  CODE_TTL_MS,
  EmailAuthManager,
  FileOutboxMailer,
  SESSION_TTL_MS,
  createEmailAuthManager,
  createMailer,
  loadAllowedEmails,
  normalizeEmail,
};
