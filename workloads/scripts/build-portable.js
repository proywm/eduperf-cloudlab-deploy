const crypto = require('node:crypto');
const fs = require('node:fs');
const fsp = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const { pipeline } = require('node:stream/promises');

const { listCases } = require('../src/case');
const { compileCase, compileDriver, findCompiler } = require('../src/runner');

const PYTHON = {
  version: '3.11.15',
  release: '20260718',
  url: 'https://github.com/astral-sh/python-build-standalone/releases/download/20260718/cpython-3.11.15%2B20260718-x86_64-unknown-linux-gnu-install_only_stripped.tar.gz',
  sha256: '23ccae6f1ff73e8aa8378436f869da003b8eb7d6c95f2bc706f494115ba1447d',
};

const EIGEN = {
  version: '3.4.0',
  url: 'https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz',
  sha256: '8586084f71f9bde545ee7fa6d00288b264a2b7ac3607b974e54d13e7162c1c72',
};

function run(command, args, options = {}) {
  const { execFile } = require('node:child_process');
  return new Promise((resolve, reject) => {
    execFile(command, args, options, (error, stdout, stderr) => {
      if (error) {
        error.stdout = stdout;
        error.stderr = stderr;
        reject(error);
      } else {
        resolve({ stdout, stderr });
      }
    });
  });
}

async function sha256(filePath) {
  const hash = crypto.createHash('sha256');
  await pipeline(fs.createReadStream(filePath), hash);
  return hash.digest('hex');
}

async function download(url, destination) {
  const response = await fetch(url, { redirect: 'follow' });
  if (!response.ok || !response.body) {
    throw new Error(`Could not download portable Python (${response.status} ${response.statusText}).`);
  }
  await pipeline(response.body, fs.createWriteStream(destination));
}

async function installPython(portableDirectory) {
  const python = path.join(portableDirectory, 'python', 'bin', 'python3.11');
  try {
    await fsp.access(python);
    return python;
  } catch {
    // Continue with the pinned, checksum-verified build dependency.
  }

  const temporaryDirectory = await fsp.mkdtemp(path.join(os.tmpdir(), 'eduperf-python-'));
  const archive = path.join(temporaryDirectory, 'python.tar.gz');
  try {
    process.stdout.write(`Downloading portable CPython ${PYTHON.version}…\n`);
    await download(PYTHON.url, archive);
    const digest = await sha256(archive);
    if (digest !== PYTHON.sha256) {
      throw new Error(`Portable Python checksum mismatch: expected ${PYTHON.sha256}, received ${digest}.`);
    }
    await run('tar', ['-xzf', archive, '-C', portableDirectory]);
    await fsp.chmod(python, 0o755);
    return python;
  } finally {
    await fsp.rm(temporaryDirectory, { recursive: true, force: true });
  }
}

async function prunePythonRuntime(portableDirectory) {
  // The adapters never initialize a terminal. Removing terminfo also avoids
  // case-only filename collisions that the cross-platform VSIX format rejects.
  await fsp.rm(
    path.join(portableDirectory, 'python', 'share', 'terminfo'),
    { recursive: true, force: true },
  );
}

async function installEigen(portableDirectory) {
  const dependenciesDirectory = path.join(portableDirectory, 'dependencies');
  const eigenDirectory = path.join(dependenciesDirectory, `eigen-${EIGEN.version}`);
  try {
    await fsp.access(path.join(eigenDirectory, 'Eigen', 'Core'));
    return eigenDirectory;
  } catch {
    // Continue with the pinned, checksum-verified header dependency.
  }

  const temporaryDirectory = await fsp.mkdtemp(path.join(os.tmpdir(), 'eduperf-eigen-'));
  const archive = path.join(temporaryDirectory, 'eigen.tar.gz');
  try {
    process.stdout.write(`Downloading Eigen ${EIGEN.version}…\n`);
    await download(EIGEN.url, archive);
    const digest = await sha256(archive);
    if (digest !== EIGEN.sha256) {
      throw new Error(`Eigen checksum mismatch: expected ${EIGEN.sha256}, received ${digest}.`);
    }
    await fsp.mkdir(dependenciesDirectory, { recursive: true });
    await fsp.rm(eigenDirectory, { recursive: true, force: true });
    await run('tar', ['-xzf', archive, '-C', dependenciesDirectory]);
    return eigenDirectory;
  } finally {
    await fsp.rm(temporaryDirectory, { recursive: true, force: true });
  }
}

async function buildCases(extensionDirectory, portableDirectory, eigenDirectory) {
  const compiler = await findCompiler(process.env.EDUPERF_BUILD_COMPILER || '');
  const outputDirectory = path.join(portableDirectory, 'cases');
  await fsp.mkdir(outputDirectory, { recursive: true });
  const cases = await listCases(extensionDirectory);
  const cppCases = cases.filter((learningCase) => learningCase.manifest.language.startsWith('C++'));
  for (const [index, learningCase] of cppCases.entries()) {
    const manifest = learningCase.manifest;
    process.stdout.write(`[${index + 1}/${cppCases.length}] Building ${manifest.id}\n`);
    const staticRuntimeFlags = ['-static-libstdc++', '-static-libgcc'];
    // GCC 9 implements the final C++20 language features under the provisional
    // c++2a spelling. Newer GCC and Clang retain that alias, so normalize the
    // three C++20 bank adapters for older hosted classroom machines.
    const runnerFlags = (manifest.runner.flags || []).map((flag) => {
      if (flag === '-std=c++20') return '-std=c++2a';
      if (flag === '-I/usr/include/eigen3') return `-I${eigenDirectory}`;
      return flag;
    });
    const compilation = manifest.runner.kind === 'cpp-driver'
      ? await compileDriver({
        compiler,
        caseDirectory: learningCase.directory,
        buildDirectory: outputDirectory,
        caseId: manifest.id,
        sourceFile: manifest.files.harness,
        flags: [...runnerFlags, ...staticRuntimeFlags],
      })
      : await compileCase({
        compiler,
        caseDirectory: learningCase.directory,
        buildDirectory: outputDirectory,
        caseId: manifest.id,
        sourceFiles: [manifest.files.before, manifest.files.after, manifest.files.harness],
        flags: [...manifest.build.flags, ...staticRuntimeFlags],
      });
    await run('strip', ['--strip-unneeded', compilation.executable]);
    await fsp.chmod(compilation.executable, 0o755);
  }
  return { compiler, count: cppCases.length };
}

async function main() {
  if (process.platform !== 'linux' || process.arch !== 'x64') {
    throw new Error('This package builder currently creates the linux-x64 classroom VSIX.');
  }
  const extensionDirectory = path.resolve(__dirname, '..');
  const portableDirectory = path.join(extensionDirectory, 'portable', 'linux-x64');
  await fsp.mkdir(portableDirectory, { recursive: true });
  const python = await installPython(portableDirectory);
  await prunePythonRuntime(portableDirectory);
  const eigenDirectory = await installEigen(portableDirectory);
  const built = await buildCases(extensionDirectory, portableDirectory, eigenDirectory);
  const pythonVersion = (await run(python, ['--version'])).stdout.trim()
    || (await run(python, ['--version'])).stderr.trim();
  const manifest = {
    schemaVersion: 1,
    target: 'linux-x64',
    python: {
      version: pythonVersion,
      distribution: 'astral-sh/python-build-standalone install_only_stripped',
      release: PYTHON.release,
      sha256: PYTHON.sha256,
    },
    cpp: {
      caseExecutables: built.count,
      compiler: built.compiler,
      staticLanguageRuntime: true,
      eigen: {
        version: EIGEN.version,
        sha256: EIGEN.sha256,
      },
    },
  };
  await fsp.writeFile(
    path.join(portableDirectory, 'manifest.json'),
    `${JSON.stringify(manifest, null, 2)}\n`,
    'utf8',
  );
  process.stdout.write(`Portable classroom runtime ready: ${pythonVersion}; ${built.count} C++ runners.\n`);
}

main().catch((error) => {
  const detail = [error.message, error.stdout, error.stderr].filter(Boolean).join('\n');
  process.stderr.write(`${detail}\n`);
  process.exitCode = 1;
});
