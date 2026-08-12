const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');

const { listCases } = require('../src/case');
const { compileCase, findCompiler, runExecutable } = require('../src/runner');

async function main() {
  const extensionDirectory = path.resolve(__dirname, '..');
  const cases = (await listCases(extensionDirectory))
    .filter((learningCase) => learningCase.manifest.runner.kind === 'instrumented-cpp');
  for (const learningCase of cases) {
    const buildDirectory = await fs.mkdtemp(path.join(os.tmpdir(), `eduperf-${learningCase.manifest.id}-`));
    try {
    const compiler = await findCompiler();
    const compilation = await compileCase({
      compiler,
      caseId: learningCase.manifest.id,
      caseDirectory: learningCase.directory,
      buildDirectory,
      sourceFiles: [
        learningCase.manifest.files.before,
        learningCase.manifest.files.after,
        learningCase.manifest.files.harness,
      ],
      flags: learningCase.manifest.build.flags,
    });
    const check = await runExecutable(
      compilation.executable,
      learningCase.manifest.execution.checkMode,
    );
    if (
      check.result.status !== 'pass'
      || check.result.cases !== learningCase.manifest.execution.expectedCheckCases
    ) {
      throw new Error(`Unexpected equivalence result: ${JSON.stringify(check.result)}`);
    }
    process.stdout.write(check.stdout);

    const benchmark = await runExecutable(
      compilation.executable,
      learningCase.manifest.execution.benchmarkMode,
    );
    if (!(benchmark.result.median_speedup > 0)) {
      throw new Error(`Unexpected benchmark result: ${JSON.stringify(benchmark.result)}`);
    }
    process.stdout.write(benchmark.stdout);

    for (const variant of ['before', 'after']) {
      const execution = await runExecutable(
        compilation.executable,
        learningCase.manifest.execution.profileModes[variant],
      );
      if (execution.result.status !== 'pass' || execution.result.variant !== variant) {
        throw new Error(`Unexpected ${variant} profile workload result.`);
      }
      if (execution.result.calls !== execution.result.iterations || execution.result.calls < 1) {
        throw new Error(`Invalid ${variant} profile call count.`);
      }
      process.stdout.write(execution.stdout);
    }
    } finally {
      await fs.rm(buildDirectory, { recursive: true, force: true });
    }
  }
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
