const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');

const { listCases } = require('../src/case');
const {
  findBundledCaseExecutable,
  findBundledPython,
  preparePythonWorktree,
  runCppDriver,
  runExecutable,
  runPythonBehavior,
  runPythonBenchmark,
} = require('../src/runner');

async function testPython(extensionDirectory, learningCase, python, temporaryRoot) {
  const workDirectory = await preparePythonWorktree({
    caseDirectory: learningCase.directory,
    buildDirectory: path.join(temporaryRoot, learningCase.manifest.id),
    files: learningCase.manifest.files,
  });
  const check = await runPythonBehavior({
    python,
    workDirectory,
    equivalenceFile: learningCase.manifest.files.equivalence,
    timeoutMs: learningCase.manifest.runner.timeoutMs,
  });
  if (check.status !== 'pass') throw new Error(`${learningCase.manifest.id}: behavior failed`);
  const benchmark = await runPythonBenchmark({
    python,
    workDirectory,
    files: learningCase.manifest.files,
    rounds: 1,
    timeoutMs: learningCase.manifest.runner.timeoutMs,
  });
  if (!(benchmark.median_speedup > 0)) throw new Error(`${learningCase.manifest.id}: invalid runtime`);
}

async function testCpp(extensionDirectory, learningCase, temporaryRoot) {
  const executable = await findBundledCaseExecutable(
    extensionDirectory,
    learningCase.manifest.id,
  );
  if (!executable) throw new Error(`${learningCase.manifest.id}: bundled runner missing`);
  const cwd = path.join(temporaryRoot, learningCase.manifest.id);
  await fs.mkdir(cwd, { recursive: true });
  if (learningCase.manifest.runner.kind === 'cpp-driver') {
    const execution = await runCppDriver(executable, {
      cwd,
      timeoutMs: learningCase.manifest.runner.timeoutMs,
    });
    if (execution.check.status !== 'pass' || !(execution.benchmark.median_speedup > 0)) {
      throw new Error(`${learningCase.manifest.id}: bundled driver failed`);
    }
    return;
  }
  const check = await runExecutable(executable, learningCase.manifest.execution.checkMode, { cwd });
  const benchmark = await runExecutable(
    executable,
    learningCase.manifest.execution.benchmarkMode,
    { cwd },
  );
  if (check.result.status !== 'pass' || !(benchmark.result.median_speedup > 0)) {
    throw new Error(`${learningCase.manifest.id}: bundled instrumented runner failed`);
  }
}

async function main() {
  const extensionDirectory = path.resolve(__dirname, '..');
  const python = await findBundledPython(extensionDirectory);
  if (!python) throw new Error('Bundled Python runtime missing. Run npm run prepare:portable first.');
  const cases = await listCases(extensionDirectory);
  const temporaryRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'eduperf-portable-test-'));
  try {
    for (const [index, learningCase] of cases.entries()) {
      if (learningCase.manifest.language.startsWith('Python')) {
        await testPython(extensionDirectory, learningCase, python, temporaryRoot);
      } else {
        await testCpp(extensionDirectory, learningCase, temporaryRoot);
      }
      process.stdout.write(`[${index + 1}/${cases.length}] ${learningCase.manifest.perfbankId} portable run passed\n`);
    }
  } finally {
    await fs.rm(temporaryRoot, { recursive: true, force: true });
  }
}

main().catch((error) => {
  process.stderr.write(`${error.stack || error.message || error}\n`);
  process.exitCode = 1;
});
