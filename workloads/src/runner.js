const { execFile } = require('node:child_process');
const fs = require('node:fs/promises');
const path = require('node:path');

const DEFAULT_MAX_BUFFER = 4 * 1024 * 1024;

function abortMessage(error) {
  return error && (error.name === 'AbortError' || error.code === 'ABORT_ERR');
}

function executeFile(command, args, options = {}) {
  return new Promise((resolve, reject) => {
    execFile(
      command,
      args,
      {
        timeout: options.timeout || 60_000,
        windowsHide: true,
        maxBuffer: options.maxBuffer || DEFAULT_MAX_BUFFER,
        cwd: options.cwd,
        env: options.env,
        signal: options.signal,
      },
      (error, stdout, stderr) => {
        if (error) {
          error.stdout = stdout;
          error.stderr = stderr;
          reject(error);
          return;
        }
        resolve({ stdout, stderr });
      },
    );
  });
}

async function commandWorks(command, versionArgs = ['--version']) {
  try {
    await executeFile(command, versionArgs, { timeout: 5_000 });
    return true;
  } catch {
    return false;
  }
}

async function findExecutable(configuredPath, candidates, label) {
  if (configuredPath) {
    if (await commandWorks(configuredPath)) {
      return configuredPath;
    }
    throw new Error(`The configured ${label} could not run: ${configuredPath}`);
  }
  for (const candidate of candidates) {
    if (await commandWorks(candidate)) {
      return candidate;
    }
  }
  return undefined;
}

async function findCompiler(configuredPath = '', platform = process.platform) {
  const candidates = platform === 'win32'
    ? ['g++.exe', 'clang++.exe']
    : platform === 'darwin'
      ? ['clang++', 'g++']
      : ['g++', 'clang++'];
  const compiler = await findExecutable(configuredPath, candidates, 'C++ compiler');
  if (!compiler) {
    throw new Error(
      'No C++ compiler was found. Install clang++ or g++, or set eduperf.compilerPath.',
    );
  }
  return compiler;
}

async function findPython(configuredPath = '', platform = process.platform) {
  const candidates = platform === 'win32' ? ['python.exe', 'py.exe'] : ['python3', 'python'];
  const python = await findExecutable(configuredPath, candidates, 'Python interpreter');
  if (!python) {
    throw new Error(
      'No Python 3 interpreter was found. Install Python 3, or set eduperf.pythonPath.',
    );
  }
  return python;
}

function portablePlatformKey(platform = process.platform, arch = process.arch) {
  if (platform === 'linux' && arch === 'x64') return 'linux-x64';
  return undefined;
}

async function executableFile(filePath) {
  try {
    await fs.access(filePath);
    // VSIX extraction normally preserves Unix modes. Repair the executable bit
    // defensively so a classroom installation remains one click.
    if (process.platform !== 'win32') await fs.chmod(filePath, 0o755);
    return filePath;
  } catch {
    return undefined;
  }
}

async function findBundledPython(extensionPath, platform = process.platform, arch = process.arch) {
  const platformKey = portablePlatformKey(platform, arch);
  if (!platformKey) return undefined;
  const name = platform === 'win32' ? 'python.exe' : 'python3.11';
  return executableFile(path.join(extensionPath, 'portable', platformKey, 'python', 'bin', name));
}

async function findBundledCaseExecutable(
  extensionPath,
  caseId,
  platform = process.platform,
  arch = process.arch,
) {
  const platformKey = portablePlatformKey(platform, arch);
  if (!platformKey) return undefined;
  return executableFile(path.join(
    extensionPath,
    'portable',
    platformKey,
    'cases',
    executableName(caseId, platform),
  ));
}

function executableName(caseId = 'matrix-unrolling', platform = process.platform) {
  if (!/^[a-z0-9-]+$/.test(caseId)) {
    throw new Error(`Invalid case id: ${caseId}`);
  }
  return platform === 'win32' ? `${caseId}.exe` : caseId;
}

function formatFailure(prefix, error) {
  if (abortMessage(error)) {
    return new Error('Operation canceled.');
  }
  const detail = [error.stdout, error.stderr, error.message]
    .filter(Boolean)
    .join('\n')
    .trim();
  return new Error(detail ? `${prefix}\n${detail}` : prefix);
}

async function compileCase({
  compiler,
  caseDirectory,
  buildDirectory,
  caseId = 'matrix-unrolling',
  sourceFiles = ['before.cpp', 'after.cpp', 'harness.cpp'],
  flags = ['-std=c++17', '-O2', '-g', '-fno-omit-frame-pointer'],
  signal,
}) {
  await fs.mkdir(buildDirectory, { recursive: true });
  const executable = path.join(buildDirectory, executableName(caseId));
  const sources = sourceFiles
    .map((name) => path.join(caseDirectory, name));
  const args = [...flags, ...sources, '-o', executable];

  try {
    const result = await executeFile(compiler, args, {
      cwd: caseDirectory,
      timeout: 60_000,
      signal,
    });
    return {
      compiler,
      args,
      executable,
      stdout: result.stdout,
      stderr: result.stderr,
    };
  } catch (error) {
    throw formatFailure('Compilation failed.', error);
  }
}

async function compileDriver({
  compiler,
  caseDirectory,
  buildDirectory,
  caseId,
  sourceFile = 'driver.cpp',
  flags = ['-std=c++17', '-O2', '-g', '-fno-omit-frame-pointer', '-pthread'],
  signal,
}) {
  return compileCase({
    compiler,
    caseDirectory,
    buildDirectory,
    caseId,
    sourceFiles: [sourceFile],
    flags,
    signal,
  });
}

async function runExecutable(executable, mode, options = {}) {
  if (!/^[a-z-]+$/.test(mode)) {
    throw new Error(`Invalid case mode: ${mode}`);
  }
  const timeout = mode === 'benchmark' ? 120_000 : 45_000;
  try {
    const result = await executeFile(executable, [`--${mode}`], {
      timeout,
      signal: options.signal,
      cwd: options.cwd,
      env: { ...process.env, LC_ALL: 'C', ...(options.env || {}) },
    });
    return {
      stdout: result.stdout,
      stderr: result.stderr,
      result: parseResult(result.stdout, mode),
    };
  } catch (error) {
    throw formatFailure(`The ${mode} run failed.`, error);
  }
}

function parseResult(output, expectedMode) {
  const line = output
    .split(/\r?\n/)
    .find((candidate) => candidate.startsWith('PERFBANK_RESULT '));

  if (!line) {
    throw new Error('The case runner did not emit a PERFBANK_RESULT line.');
  }

  const fields = Object.fromEntries(
    [...line.matchAll(/([a-z_]+)=([^\s]+)/g)].map((match) => [match[1], match[2]]),
  );
  if (fields.mode !== expectedMode) {
    throw new Error(`Expected a ${expectedMode} result but received ${fields.mode || 'unknown'}.`);
  }

  const numericFields = [
    'cases',
    'rounds',
    'median_speedup',
    'before_us',
    'after_us',
    'elapsed_us',
    'iterations',
    'calls',
  ];
  for (const key of numericFields) {
    if (fields[key] !== undefined) {
      fields[key] = Number(fields[key]);
      if (!Number.isFinite(fields[key])) {
        throw new Error(`The case runner emitted an invalid numeric ${key}.`);
      }
    }
  }
  return fields;
}

function median(values) {
  const sorted = [...values].sort((left, right) => left - right);
  if (sorted.length === 0) throw new Error('Cannot compute a median from an empty sample.');
  const middle = Math.floor(sorted.length / 2);
  return sorted.length % 2 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / 2;
}

function parsePythonTime(output) {
  const match = output.match(/(?:^|\n)TIME_SECONDS=([0-9.eE+-]+)/);
  const seconds = match ? Number(match[1]) : Number.NaN;
  if (!Number.isFinite(seconds) || seconds < 0) {
    throw new Error('The preserved Python benchmark did not emit a valid TIME_SECONDS value.');
  }
  return seconds;
}

function parseEquivalence(output) {
  const text = String(output || '');
  const negative = [
    /EQUIV(?:ALENT)?\s*[:=]\s*(?:no|false|0)\b/i,
    /EQUIV_FAIL\b/i,
    /(?:^|\s)(?:mismatches|divergences|fails)\s*[:=]\s*[1-9]\d*/i,
  ];
  if (negative.some((pattern) => pattern.test(text))) {
    return { status: 'fail', kind: 'fresh', cases: extractCaseCount(text) };
  }
  const positive = [
    /EQUIV(?:ALENT)?\s*[:=]\s*(?:yes|true|1)\b/i,
    /EQUIV(?:ALENT)?_OK\b/i,
    /\bEQUIVALENT\s+over\b/i,
    /\bequivalent\s*=\s*true\b/i,
    /\b(?:total\s+)?mismatches\s*[:=]\s*0\b/i,
    /\bdivergences\s*[:=]\s*0\b/i,
    /\bfaithful_mismatches\s*[:=]\s*0\b/i,
    /\bEQUIV\s*:\s*cases=\d+\s+mism=0\b/i,
    /\bEQUIVALENT\s*:\s*calls=\d+\b/i,
    /^EQUIVALENT$/im,
  ];
  if (!positive.some((pattern) => pattern.test(text))) {
    throw new Error('The preserved adapter did not emit a recognizable equivalence verdict.');
  }
  return { status: 'pass', kind: 'fresh', cases: extractCaseCount(text) };
}

function extractCaseCount(output) {
  const patterns = [
    /\bcases\s*[:=]\s*(\d+)/i,
    /\bcalls\s*[:=]\s*(\d+)/i,
    /\bEQUIVALENT\s+over\s+(\d+)\s+inputs/i,
    /\btotal\s*[:=]\s*(\d+)/i,
  ];
  for (const pattern of patterns) {
    const match = output.match(pattern);
    if (match) return Number(match[1]);
  }
  return undefined;
}

async function preparePythonWorktree({ caseDirectory, buildDirectory, files }) {
  const workDirectory = path.join(buildDirectory, 'python-work');
  await fs.mkdir(workDirectory, { recursive: true });
  const fileNames = [
    files.before,
    files.after,
    files.harness,
    files.equivalence,
    ...(files.auxiliary || []),
  ].filter(Boolean);
  for (const fileName of fileNames) {
    await fs.copyFile(path.join(caseDirectory, fileName), path.join(workDirectory, fileName));
  }
  return workDirectory;
}

async function runPythonBehavior({ python, workDirectory, equivalenceFile, timeoutMs, signal }) {
  if (!equivalenceFile) {
    return {
      status: 'pass',
      kind: 'recorded',
      message: 'Behavioral equivalence is the bank\'s recorded validation verdict.',
    };
  }
  try {
    const execution = await executeFile(python, [equivalenceFile], {
      cwd: workDirectory,
      timeout: timeoutMs || 120_000,
      signal,
      env: {
        ...process.env,
        LC_ALL: 'C',
        PYTHONDONTWRITEBYTECODE: '1',
        PYTHONHASHSEED: '0',
        PYTHONNOUSERSITE: '1',
        PYTHONPATH: '',
      },
    });
    return { ...parseEquivalence(execution.stdout), stdout: execution.stdout, stderr: execution.stderr };
  } catch (error) {
    throw formatFailure('The preserved behavioral-equivalence adapter failed.', error);
  }
}

async function runPythonBenchmark({
  python,
  workDirectory,
  files,
  rounds = 7,
  timeoutMs = 120_000,
  signal,
  onRound,
}) {
  const samples = { before: [], after: [] };
  for (let round = 0; round < rounds; round += 1) {
    const order = round % 2 === 0 ? ['before', 'after'] : ['after', 'before'];
    for (const variant of order) {
      await fs.copyFile(
        path.join(workDirectory, files[variant]),
        path.join(workDirectory, 'target.py'),
      );
      if (onRound) onRound(round + 1, rounds, variant);
      try {
        const execution = await executeFile(python, [files.harness], {
          cwd: workDirectory,
          timeout: timeoutMs,
          signal,
          env: {
            ...process.env,
            LC_ALL: 'C',
            PYTHONDONTWRITEBYTECODE: '1',
            PYTHONHASHSEED: '0',
            PYTHONNOUSERSITE: '1',
            PYTHONPATH: '',
          },
        });
        samples[variant].push(parsePythonTime(execution.stdout));
      } catch (error) {
        throw formatFailure(`The preserved Python ${variant} benchmark failed.`, error);
      }
    }
  }
  const beforeSeconds = median(samples.before);
  const afterSeconds = median(samples.after);
  return {
    status: 'pass',
    rounds,
    before_us: beforeSeconds * 1_000_000,
    after_us: afterSeconds * 1_000_000,
    median_speedup: beforeSeconds / afterSeconds,
    samples,
  };
}

function numericMatch(text, patterns) {
  for (const pattern of patterns) {
    const match = text.match(pattern);
    if (!match) continue;
    const value = Number(match[1]);
    if (Number.isFinite(value)) return value;
  }
  return undefined;
}

function timingPair(output) {
  const lines = output.split(/\r?\n/).filter((line) => /before/i.test(line) && /after/i.test(line));
  const candidates = [];
  for (const line of lines) {
    const before = numericMatch(line, [
      /(?:med(?:ian)?[_ ]?)?before(?:_ns(?:_median)?|[_ ]ns)?\s*[:=]\s*([0-9.eE+-]+)/i,
      /\bbefore\s*=\s*([0-9.eE+-]+)\s*ns/i,
    ]);
    const after = numericMatch(line, [
      /(?:med(?:ian)?[_ ]?)?after(?:_ns(?:_median)?|[_ ]ns)?\s*[:=]\s*([0-9.eE+-]+)/i,
      /\bafter\s*=\s*([0-9.eE+-]+)\s*ns/i,
    ]);
    if (Number.isFinite(before) && Number.isFinite(after)) {
      const score = /median|MED_|TIMING/i.test(line) ? 2 : 1;
      candidates.push({ before, after, score });
    }
  }
  const bestScore = Math.max(0, ...candidates.map((candidate) => candidate.score));
  const best = candidates.filter((candidate) => candidate.score === bestScore);
  if (bestScore === 1 && best.length > 1) return undefined;
  return best.at(-1);
}

function parseCppDriver(output) {
  const check = parseEquivalence(output);
  const pair = timingPair(output);
  let beforeNs = pair && pair.before;
  let afterNs = pair && pair.after;
  if (!Number.isFinite(beforeNs) || !Number.isFinite(afterNs)) {
    beforeNs = numericMatch(output, [/^BEFORE_NS=([0-9.eE+-]+)$/im]);
    afterNs = numericMatch(output, [/^AFTER_NS=([0-9.eE+-]+)$/im]);
  }
  let speedup = numericMatch(output, [
    /\bmedian_speedup\s*[:=]\s*([0-9.eE+-]+)/i,
    /\bSPEEDUP_MEDIAN\s*[:=]\s*([0-9.eE+-]+)/i,
    /\bMEDIAN_SPEEDUP\s*[:=]\s*([0-9.eE+-]+)/i,
    /\bspeedup_median\s*[:=]\s*([0-9.eE+-]+)/i,
    /\bSPEEDUP\s+median\([^)]*\)\s*=\s*([0-9.eE+-]+)/i,
    /\bSPEEDUP\s*[:=]\s*([0-9.eE+-]+)/i,
    /\bspeedup\s*[:=]\s*([0-9.eE+-]+)/i,
  ]);
  if (!Number.isFinite(speedup) && Number.isFinite(beforeNs) && Number.isFinite(afterNs)) {
    speedup = beforeNs / afterNs;
  }
  if (!Number.isFinite(speedup) || speedup <= 0) {
    throw new Error('The preserved C++ driver did not emit a valid runtime ratio.');
  }
  return {
    check,
    benchmark: {
      status: 'pass',
      rounds: 1,
      protocol: 'preserved-driver',
      median_speedup: speedup,
      ...(Number.isFinite(beforeNs) ? { before_us: beforeNs / 1000 } : {}),
      ...(Number.isFinite(afterNs) ? { after_us: afterNs / 1000 } : {}),
    },
  };
}

async function runCppDriver(executable, options = {}) {
  try {
    const execution = await executeFile(executable, [], {
      timeout: options.timeoutMs || 90_000,
      signal: options.signal,
      cwd: options.cwd,
      env: { ...process.env, LC_ALL: 'C', ...(options.env || {}) },
    });
    return { ...parseCppDriver(execution.stdout), stdout: execution.stdout, stderr: execution.stderr };
  } catch (error) {
    throw formatFailure('The preserved C++ measurement driver failed.', error);
  }
}

module.exports = {
  commandWorks,
  compileCase,
  compileDriver,
  executableName,
  executeFile,
  findBundledCaseExecutable,
  findBundledPython,
  findCompiler,
  findPython,
  findExecutable,
  formatFailure,
  parseResult,
  parseCppDriver,
  parseEquivalence,
  parsePythonTime,
  preparePythonWorktree,
  portablePlatformKey,
  runCppDriver,
  runExecutable,
  runPythonBehavior,
  runPythonBenchmark,
};
