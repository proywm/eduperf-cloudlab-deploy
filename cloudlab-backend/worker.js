const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');

const { listCases } = require('../workloads/src/case');
const { collectHpctoolkitProfile } = require('../workloads/src/hpctoolkit');
const {
  compileCase,
  findBundledCaseExecutable,
  findBundledPython,
  findCompiler,
  preparePythonWorktree,
  runCppDriver,
  runExecutable,
  runPythonBehavior,
  runPythonBenchmark,
} = require('../workloads/src/runner');

const ACTIONS = new Set(['verify', 'run', 'profile', 'run-and-profile']);

class EduPerfWorker {
  constructor(options = {}) {
    this.workloadDirectory = path.resolve(options.workloadDirectory);
    this.workRoot = path.resolve(options.workRoot);
    this.hpctoolkitRoot = options.hpctoolkitRoot || '';
    this.workerLabel = options.workerLabel || process.env.EDUPERF_WORKER_LABEL || 'cloudlab-worker';
    this.nodeType = options.nodeType || process.env.EDUPERF_NODE_TYPE || 'unknown';
    this.casesPromise = listCases(this.workloadDirectory);
    this.compilations = new Map();
  }

  async cases() {
    return this.casesPromise;
  }

  async learningCase(caseId) {
    const learningCase = (await this.cases()).find((candidate) => candidate.manifest.id === caseId);
    if (!learningCase) throw new Error(`Unknown EduPerf case: ${caseId}`);
    return learningCase;
  }

  async capabilities() {
    const cases = await this.cases();
    return cases.map(({ manifest }) => ({
      caseId: manifest.id,
      perfbankId: manifest.perfbankId,
      runtime: true,
      profile: manifest.profiling.kind === 'hpctoolkit',
    }));
  }

  async bundledExecutable(caseId) {
    const executable = await findBundledCaseExecutable(this.workloadDirectory, caseId);
    if (!executable) {
      throw new Error(`The CloudLab image is missing the bundled runner for ${caseId}.`);
    }
    return executable;
  }

  async runRuntime(learningCase, jobDirectory, onProgress, verifyOnly = false) {
    const { manifest } = learningCase;
    if (manifest.runner.kind === 'python-bench') {
      onProgress('Preparing the embedded Python adapter');
      const python = await findBundledPython(this.workloadDirectory);
      if (!python) throw new Error('The CloudLab image is missing embedded Python.');
      const workDirectory = await preparePythonWorktree({
        caseDirectory: learningCase.directory,
        buildDirectory: jobDirectory,
        files: manifest.files,
      });
      const check = await runPythonBehavior({
        python,
        workDirectory,
        equivalenceFile: manifest.files.equivalence,
        timeoutMs: manifest.runner.timeoutMs,
      });
      if (check.status !== 'pass') throw new Error('Behavior differs; remote timing was stopped.');
      if (verifyOnly) return { check: cleanCheck(check) };
      const benchmark = await runPythonBenchmark({
        python,
        workDirectory,
        files: manifest.files,
        rounds: manifest.runner.rounds,
        timeoutMs: manifest.runner.timeoutMs,
        onRound: (round, rounds, variant) => onProgress(
          `Timing ${variant}, round ${round}/${rounds}`,
        ),
      });
      return { check: cleanCheck(check), benchmark };
    }

    const executable = await this.bundledExecutable(manifest.id);
    if (manifest.runner.kind === 'cpp-driver') {
      onProgress('Running the preserved C++ differential and timing protocol');
      const execution = await runCppDriver(executable, {
        cwd: jobDirectory,
        timeoutMs: manifest.runner.timeoutMs,
      });
      if (execution.check.status !== 'pass') throw new Error('Behavior differs; remote timing was stopped.');
      return {
        check: cleanCheck(execution.check),
        ...(!verifyOnly ? { benchmark: execution.benchmark } : {}),
      };
    }

    onProgress('Running the shared-input behavioral check');
    const checkExecution = await runExecutable(
      executable,
      manifest.execution.checkMode,
      { cwd: jobDirectory },
    );
    if (checkExecution.result.status !== 'pass') {
      throw new Error('Behavior differs; remote timing was stopped.');
    }
    if (verifyOnly) return { check: cleanCheck(checkExecution.result) };
    onProgress('Running seven alternating timing rounds');
    const benchmarkExecution = await runExecutable(
      executable,
      manifest.execution.benchmarkMode,
      { cwd: jobDirectory },
    );
    return { check: cleanCheck(checkExecution.result), benchmark: benchmarkExecution.result };
  }

  async profilingExecutable(learningCase) {
    const caseId = learningCase.manifest.id;
    if (!this.compilations.has(caseId)) {
      this.compilations.set(caseId, (async () => {
        const compiler = await findCompiler();
        const buildDirectory = path.join(this.workRoot, 'profile-builds', caseId);
        const compilation = await compileCase({
          compiler,
          caseId,
          caseDirectory: learningCase.directory,
          buildDirectory,
          sourceFiles: [
            learningCase.manifest.files.before,
            learningCase.manifest.files.after,
            learningCase.manifest.files.harness,
          ],
          flags: learningCase.manifest.build.flags,
        });
        return { ...compilation, buildDirectory };
      })());
    }
    try {
      return await this.compilations.get(caseId);
    } catch (error) {
      this.compilations.delete(caseId);
      throw error;
    }
  }

  async runProfile(learningCase, runId, onProgress) {
    if (learningCase.manifest.profiling.kind !== 'hpctoolkit') {
      throw new Error(
        'This case does not yet have a source-attributed CloudLab profiling adapter.',
      );
    }
    onProgress('Preparing the optimized source-mapped profiling runner');
    const compilation = await this.profilingExecutable(learningCase);
    const profile = await collectHpctoolkitProfile({
      learningCase,
      executable: compilation.executable,
      buildDirectory: compilation.buildDirectory,
      configuredRoot: this.hpctoolkitRoot,
      compiler: compilation.compiler,
      provenanceKind: 'local',
      onProgress,
    });
    profile.provenance.kind = 'cloudlab';
    profile.provenance.label = 'Live CloudLab HPCToolkit profile';
    profile.provenance.backendRunId = runId;
    profile.provenance.worker = this.workerLabel;
    profile.provenance.nodeType = this.nodeType;
    profile.provenance.kernel = os.release();
    return profile;
  }

  async execute({ runId, caseId, action, onProgress = () => {} }) {
    if (!ACTIONS.has(action)) throw new Error(`Unsupported action: ${action}`);
    const learningCase = await this.learningCase(caseId);
    const jobDirectory = path.join(this.workRoot, 'jobs', runId);
    await fs.mkdir(jobDirectory, { recursive: true });
    try {
      const result = {
        schemaVersion: 1,
        runId,
        caseId,
        perfbankId: learningCase.manifest.perfbankId,
        action,
        environment: {
          kind: 'cloudlab',
          worker: this.workerLabel,
          nodeType: this.nodeType,
          platform: `${process.platform} ${process.arch}`,
          processor: os.cpus()[0]?.model.trim() || 'Unknown processor',
          kernel: os.release(),
        },
      };
      if (action === 'verify' || action === 'run' || action === 'run-and-profile') {
        result.runtime = await this.runRuntime(
          learningCase,
          jobDirectory,
          onProgress,
          action === 'verify',
        );
      }
      if (action === 'profile' || action === 'run-and-profile') {
        result.profile = await this.runProfile(learningCase, runId, onProgress);
      }
      return result;
    } finally {
      await fs.rm(jobDirectory, { recursive: true, force: true });
    }
  }
}

function cleanCheck(check) {
  return {
    status: check.status,
    kind: check.kind,
    cases: check.cases,
    message: check.message,
  };
}

module.exports = { ACTIONS, EduPerfWorker };
