const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');

const { listCases } = require('../workloads/src/case');
const {
  collectHpctoolkitProfile,
  collectPreservedBankProfile,
} = require('../workloads/src/hpctoolkit');
const {
  compileCase,
  compileDriver,
  findBundledCaseExecutable,
  findBundledPython,
  findCompiler,
  findPython,
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
    this.profilePython = options.profilePython || process.env.EDUPERF_PROFILE_PYTHON || '';
    this.workerLabel = options.workerLabel || process.env.EDUPERF_WORKER_LABEL || 'cloudlab-worker';
    this.nodeType = options.nodeType || process.env.EDUPERF_NODE_TYPE || 'unknown';
    this.environmentKind = options.environmentKind
      || process.env.EDUPERF_ENVIRONMENT_KIND
      || 'cloudlab';
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
      profile: ['hpctoolkit', 'hosted-hpctoolkit'].includes(manifest.profiling.kind),
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
        rounds: verifyOnly ? 1 : 5,
        onRound: verifyOnly ? undefined : (round, rounds) => onProgress(
          `Repeating preserved C++ timing, round ${round}/${rounds}`,
        ),
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
        const manifest = learningCase.manifest;
        const profilingFlags = (manifest.runner.flags || []).map((flag) => {
          if (flag === '-std=c++20') return '-std=c++2a';
          if (flag === '-I/usr/include/eigen3') {
            return `-I${path.join(
              this.workloadDirectory,
              'portable',
              'linux-x64',
              'dependencies',
              'eigen-3.4.0',
            )}`;
          }
          return flag;
        });
        const compilation = manifest.runner.kind === 'cpp-driver'
          ? await compileDriver({
            compiler,
            caseId,
            caseDirectory: learningCase.directory,
            buildDirectory,
            sourceFile: manifest.files.harness,
            flags: profilingFlags,
          })
          : await compileCase({
            compiler,
            caseId,
            caseDirectory: learningCase.directory,
            buildDirectory,
            sourceFiles: [
              manifest.files.before,
              manifest.files.after,
              manifest.files.harness,
            ],
            flags: manifest.build.flags,
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

  async runProfile(learningCase, runId, jobDirectory, onProgress) {
    const { manifest } = learningCase;
    let profile;
    if (manifest.profiling.kind === 'hosted-hpctoolkit') {
      onProgress('Preparing the preserved profiling adapter');
      if (manifest.runner.kind === 'python-bench') {
        if (!this.profilePython) {
          throw new Error('The hosted worker is missing its HPCToolkit-matched Python runtime.');
        }
        const python = await findPython(this.profilePython);
        const workDirectory = await preparePythonWorktree({
          caseDirectory: learningCase.directory,
          buildDirectory: path.join(jobDirectory, 'profile'),
          files: manifest.files,
        });
        profile = await collectPreservedBankProfile({
          learningCase,
          executable: python,
          workDirectory,
          configuredRoot: this.hpctoolkitRoot,
          provenanceKind: this.environmentKind,
          onProgress,
        });
      } else {
        const compilation = await this.profilingExecutable(learningCase);
        profile = await collectPreservedBankProfile({
          learningCase,
          executable: compilation.executable,
          workDirectory: jobDirectory,
          sourceDirectory: learningCase.directory,
          configuredRoot: this.hpctoolkitRoot,
          provenanceKind: this.environmentKind,
          onProgress,
        });
      }
    } else if (manifest.profiling.kind === 'hpctoolkit') {
      onProgress('Preparing the optimized source-mapped profiling runner');
      const compilation = await this.profilingExecutable(learningCase);
      profile = await collectHpctoolkitProfile({
        learningCase,
        executable: compilation.executable,
        buildDirectory: compilation.buildDirectory,
        configuredRoot: this.hpctoolkitRoot,
        compiler: compilation.compiler,
        provenanceKind: this.environmentKind,
        onProgress,
      });
    } else {
      throw new Error('This case does not provide a hosted HPCToolkit profiling adapter.');
    }
    profile.provenance.kind = this.environmentKind;
    profile.provenance.label = this.environmentKind === 'cloudlab'
      ? 'Live CloudLab HPCToolkit profile'
      : 'Live hosted HPCToolkit profile';
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
        evidenceProtocol: 2,
        runId,
        caseId,
        perfbankId: learningCase.manifest.perfbankId,
        action,
        environment: {
          kind: this.environmentKind,
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
        result.profile = await this.runProfile(learningCase, runId, jobDirectory, onProgress);
      }
      return result;
    } finally {
      await fs.rm(jobDirectory, { recursive: true, force: true });
    }
  }
}

function cleanCheck(check) {
  const domains = Array.isArray(check.domains) ? check.domains.slice(0, 8).map((domain) => ({
    id: String(domain.id || '').slice(0, 80),
    label: String(domain.label || '').slice(0, 160),
    cases: Number.isFinite(domain.cases) ? domain.cases : undefined,
    mismatches: Number.isFinite(domain.mismatches) ? domain.mismatches : undefined,
    validForDecision: domain.validForDecision === true,
  })) : undefined;
  return {
    status: check.status,
    kind: check.kind,
    cases: check.cases,
    message: check.message,
    scope: check.scope === 'stated-precondition' ? check.scope : undefined,
    domains,
  };
}

module.exports = { ACTIONS, EduPerfWorker, cleanCheck };
