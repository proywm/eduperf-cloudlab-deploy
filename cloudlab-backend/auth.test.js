const assert = require('node:assert/strict');
const test = require('node:test');

const {
  CODE_TTL_MS,
  EmailAuthManager,
  SESSION_TTL_MS,
  normalizeEmail,
} = require('./auth');

function fixture() {
  let now = Date.UTC(2026, 7, 12, 12);
  const messages = [];
  const mailer = {
    kind: 'capture',
    deliveryReady: true,
    async sendCode(email, code) { messages.push({ email, code }); },
  };
  const manager = new EmailAuthManager({
    allowedEmails: new Set(['probirr@umich.edu', 'sjiao2@ncsu.edu', 'jit623@lehigh.edu']),
    secret: Buffer.alloc(48, 7),
    mailer,
    now: () => now,
    codeGenerator: () => '042731',
  });
  return {
    manager,
    messages,
    advance(milliseconds) { now += milliseconds; },
  };
}

test('normalizes institutional email addresses and rejects malformed input', () => {
  assert.equal(normalizeEmail('  ProbirR@UMICH.EDU '), 'probirr@umich.edu');
  assert.throws(() => normalizeEmail('not-an-email'), /valid institutional email/);
});

test('sends codes only to exact allowlist entries without disclosing membership', async () => {
  const { manager, messages } = fixture();
  assert.deepEqual(await manager.requestCode('probirr@UMICH.edu', '198.51.100.8'), { accepted: true });
  assert.equal(messages.length, 1);
  assert.deepEqual(messages[0], { email: 'probirr@umich.edu', code: '042731' });

  assert.deepEqual(await manager.requestCode('student@example.edu', '198.51.100.8'), { accepted: true });
  assert.equal(messages.length, 1);
});

test('one-time code creates a signed seven-day session bound to the allowlist', async () => {
  const { manager, advance } = fixture();
  await manager.requestCode('probirr@umich.edu', '198.51.100.8');
  const session = manager.verifyCode('probirr@umich.edu', '042731');
  assert.equal(session.email, 'probirr@umich.edu');
  assert.ok(session.token.startsWith('v1.'));
  assert.deepEqual(manager.authenticate(session.token), {
    id: 'probirr@umich.edu', label: 'probirr@umich.edu',
  });
  assert.throws(() => manager.verifyCode('probirr@umich.edu', '042731'), /invalid or expired/);

  advance(SESSION_TTL_MS);
  assert.equal(manager.authenticate(session.token), undefined);
});

test('expired codes and repeated guesses cannot be used', async () => {
  const expired = fixture();
  await expired.manager.requestCode('probirr@umich.edu', '198.51.100.8');
  expired.advance(CODE_TTL_MS);
  assert.throws(() => expired.manager.verifyCode('probirr@umich.edu', '042731'), /invalid or expired/);

  const guessed = fixture();
  await guessed.manager.requestCode('probirr@umich.edu', '198.51.100.8');
  for (let attempt = 0; attempt < 5; attempt += 1) {
    assert.throws(() => guessed.manager.verifyCode('probirr@umich.edu', '000000'), /invalid or expired/);
  }
  assert.throws(() => guessed.manager.verifyCode('probirr@umich.edu', '042731'), /invalid or expired/);
});

test('request rate limits apply per address and per source', async () => {
  const perEmail = fixture();
  await perEmail.manager.requestCode('probirr@umich.edu', '198.51.100.8');
  await assert.rejects(
    perEmail.manager.requestCode('probirr@umich.edu', '198.51.100.8'),
    /wait one minute/,
  );

  const perSource = fixture();
  for (let index = 0; index < 30; index += 1) {
    await perSource.manager.requestCode(`unknown-${index}@example.edu`, '198.51.100.9');
  }
  await assert.rejects(
    perSource.manager.requestCode('another@example.edu', '198.51.100.9'),
    /Too many sign-in codes/,
  );
});
