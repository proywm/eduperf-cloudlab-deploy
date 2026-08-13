const fs = require('node:fs/promises');
const path = require('node:path');

const DEFAULT_CASE_ID = 'matrix-unrolling';

const BANK_PROFILE_EVENTS = [
  'CYCLES',
  'INSTRUCTIONS',
  'BRANCHES',
  'BRANCH-MISSES',
  'CACHE-MISSES',
];

const BANK_PROFILE_METRICS = [
  {
    id: 'cycles',
    label: 'CPU cycles',
    unit: 'cycles',
    direction: 'lower',
    explanation: 'Estimated cycles for one preserved benchmark execution on the hosted measurement CPU.',
  },
  {
    id: 'instructions',
    label: 'Instructions',
    unit: 'instructions',
    direction: 'lower',
    explanation: 'Estimated retired instructions for the same preserved workload and interpreter or executable.',
  },
  {
    id: 'ipc',
    label: 'IPC',
    unit: 'instructions/cycle',
    direction: 'higher',
    explanation: 'Instructions per cycle, derived from the HPCToolkit instruction and cycle estimates.',
  },
  {
    id: 'branches',
    label: 'Branches',
    unit: 'branches',
    direction: 'lower',
    explanation: 'Estimated branch instructions executed by the preserved workload.',
  },
  {
    id: 'branchMissRate',
    label: 'Branch-miss rate',
    unit: '%',
    direction: 'lower',
    explanation: 'Estimated branch misses divided by branch instructions.',
  },
  {
    id: 'cacheMisses',
    label: 'Cache misses',
    unit: 'misses',
    direction: 'lower',
    secondary: true,
    explanation: 'Estimated cache misses; use this as supporting evidence when the source change affects locality.',
  },
];

function normalizeBankProfiling(manifest) {
  if (manifest.profiling.kind !== 'recorded') return manifest;
  manifest.profiling = {
    ...manifest.profiling,
    kind: 'hosted-hpctoolkit',
    actionLabel: `Inspect ${manifest.runner.kind === 'python-bench' ? 'Python execution' : 'calling context'}`,
    events: [...BANK_PROFILE_EVENTS],
    sampleFrequencyHz: 1009,
    metrics: BANK_PROFILE_METRICS.map((metric) => ({ ...metric })),
  };
  return manifest;
}

function assertObject(value, label) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    throw new Error(`${label} must be an object.`);
  }
}

function assertString(value, label) {
  if (typeof value !== 'string' || value.length === 0) {
    throw new Error(`${label} must be a non-empty string.`);
  }
}

function validateCaseManifest(manifest) {
  assertObject(manifest, 'Case manifest');
  if (manifest.schemaVersion !== 1) {
    throw new Error(`Unsupported case schema version: ${manifest.schemaVersion}`);
  }
  assertString(manifest.id, 'Case id');
  assertString(manifest.perfbankId, 'PerfBank id');
  assertString(manifest.title, 'Case title');
  assertObject(manifest.files, 'Case files');
  for (const key of ['before', 'after', 'harness', 'header']) {
    assertString(manifest.files[key], `Case files.${key}`);
    if (path.basename(manifest.files[key]) !== manifest.files[key]) {
      throw new Error(`Case files.${key} must be a file name, not a path.`);
    }
  }
  assertObject(manifest.build, 'Case build configuration');
  if (!Array.isArray(manifest.build.flags) || !manifest.build.flags.every((item) => typeof item === 'string')) {
    throw new Error('Case build.flags must be an array of strings.');
  }
  assertObject(manifest.execution, 'Case execution configuration');
  assertString(manifest.execution.profileWorkload, 'Case profile workload');
  assertObject(manifest.execution.profileModes, 'Case profile modes');
  for (const variant of ['before', 'after']) {
    assertString(manifest.execution.profileModes[variant], `Case profile mode ${variant}`);
  }
  assertObject(manifest.profiling, 'Case profiling configuration');
  assertObject(manifest.profiling.targets, 'Case profiling targets');
  assertString(manifest.profiling.actionLabel, 'Case profiling action label');
  for (const variant of ['before', 'after']) {
    assertString(manifest.profiling.targets[variant], `Case profiling target ${variant}`);
  }
  if (
    !Array.isArray(manifest.profiling.events)
    || manifest.profiling.events.length === 0
    || !manifest.profiling.events.every((event) => typeof event === 'string' && event.length > 0)
  ) {
    throw new Error('Case profiling.events must contain at least one event.');
  }
  if (!Array.isArray(manifest.profiling.metrics) || manifest.profiling.metrics.length === 0) {
    throw new Error('Case profiling.metrics must contain at least one metric.');
  }
  const metricIds = new Set();
  for (const metric of manifest.profiling.metrics) {
    assertObject(metric, 'Case metric');
    assertString(metric.id, 'Case metric id');
    assertString(metric.label, `Case metric ${metric.id} label`);
    assertString(metric.explanation, `Case metric ${metric.id} explanation`);
    if (metricIds.has(metric.id)) {
      throw new Error(`Case metric id is duplicated: ${metric.id}`);
    }
    metricIds.add(metric.id);
  }
  return manifest;
}

function validateBankManifest(manifest) {
  assertObject(manifest, 'Bank case manifest');
  if (manifest.schemaVersion !== 2) {
    throw new Error(`Unsupported bank case schema version: ${manifest.schemaVersion}`);
  }
  for (const key of ['id', 'perfbankId', 'title', 'subtitle', 'language', 'course']) {
    assertString(manifest[key], `Bank case ${key}`);
  }
  if (!/^[epc]\d{2}$/.test(manifest.id)) {
    throw new Error(`Invalid bank case id: ${manifest.id}`);
  }
  assertObject(manifest.files, 'Bank case files');
  for (const [key, fileName] of Object.entries(manifest.files)) {
    if (key === 'auxiliary') {
      if (!Array.isArray(fileName) || !fileName.every((name) => typeof name === 'string' && path.basename(name) === name)) {
        throw new Error('Bank case auxiliary files must be an array of file names.');
      }
      continue;
    }
    assertString(fileName, `Bank case files.${key}`);
    if (path.basename(fileName) !== fileName) {
      throw new Error(`Bank case files.${key} must be a file name, not a path.`);
    }
  }
  assertObject(manifest.runner, 'Bank case runner');
  if (!['python-bench', 'cpp-driver'].includes(manifest.runner.kind)) {
    throw new Error(`Unsupported bank runner: ${manifest.runner.kind}`);
  }
  if (!['fresh', 'recorded'].includes(manifest.runner.behavior)) {
    throw new Error(`Unsupported bank behavior mode: ${manifest.runner.behavior}`);
  }
  if (manifest.runner.kind === 'python-bench') {
    for (const key of ['before', 'after', 'harness']) {
      assertString(manifest.files[key], `Python bank case files.${key}`);
    }
  } else {
    assertString(manifest.files.harness, 'C++ bank case harness');
    if (!Array.isArray(manifest.runner.flags) || !manifest.runner.flags.every((flag) => typeof flag === 'string')) {
      throw new Error('C++ bank runner flags must be an array of strings.');
    }
  }
  assertObject(manifest.code, 'Bank case code');
  for (const key of ['before', 'after', 'beforeName', 'afterName']) {
    assertString(manifest.code[key], `Bank case code.${key}`);
  }
  assertObject(manifest.profiling, 'Bank case profiling');
  assertObject(manifest.evidence, 'Bank case evidence');
  assertObject(manifest.learning, 'Bank case learning');
  return manifest;
}

function validateProfile(profile, expectedCaseId = DEFAULT_CASE_ID) {
  assertObject(profile, 'Profile');
  if (profile.schemaVersion !== 1) {
    throw new Error(`Unsupported profile schema version: ${profile.schemaVersion}`);
  }
  if (profile.caseId !== expectedCaseId) {
    throw new Error(`Profile belongs to ${profile.caseId || 'an unknown case'}, not ${expectedCaseId}.`);
  }
  if (profile.status === 'placeholder') {
    throw new Error(profile.message || 'The bundled reference profile has not been collected.');
  }
  assertObject(profile.provenance, 'Profile provenance');
  assertObject(profile.workload, 'Profile workload');
  assertString(profile.workload.description, 'Profile workload description');
  if (!Number.isFinite(profile.workload.callsPerVariant) || profile.workload.callsPerVariant < 1) {
    throw new Error('Profile workload callsPerVariant must be a positive number.');
  }
  assertObject(profile.variants, 'Profile variants');
  for (const variant of ['before', 'after']) {
    assertObject(profile.variants[variant], `Profile ${variant} variant`);
    assertObject(profile.variants[variant].metrics, `Profile ${variant} metrics`);
    if (!Number.isFinite(profile.variants[variant].callCount) || profile.variants[variant].callCount < 1) {
      throw new Error(`Profile ${variant} callCount must be a positive number.`);
    }
    if (!Array.isArray(profile.variants[variant].context)) {
      throw new Error(`Profile ${variant} context must be an array.`);
    }
  }
  return profile;
}

async function readJson(filePath) {
  try {
    return JSON.parse(await fs.readFile(filePath, 'utf8'));
  } catch (error) {
    throw new Error(`Could not read ${path.basename(filePath)}: ${error.message}`);
  }
}

async function loadCase(extensionPath, caseId = DEFAULT_CASE_ID) {
  const caseDirectory = path.join(extensionPath, 'cases', caseId);
  const manifestPath = path.join(caseDirectory, 'case.json');
  const manifest = validateCaseManifest(await readJson(manifestPath));
  if (manifest.id !== caseId) {
    throw new Error(`Expected case ${caseId}, but manifest declares ${manifest.id}.`);
  }

  const [beforeCode, afterCode] = await Promise.all([
    fs.readFile(path.join(caseDirectory, manifest.files.before), 'utf8'),
    fs.readFile(path.join(caseDirectory, manifest.files.after), 'utf8'),
  ]);

  return {
    directory: caseDirectory,
    manifest,
    beforeCode,
    afterCode,
  };
}

async function loadBankCases(extensionPath) {
  const catalogPath = path.join(extensionPath, 'catalog', 'cases.json');
  let catalog;
  try {
    catalog = await readJson(catalogPath);
  } catch (error) {
    if (error.cause && error.cause.code === 'ENOENT') return [];
    throw error;
  }
  assertObject(catalog, 'Bank catalog');
  if (catalog.schemaVersion !== 1 || !Array.isArray(catalog.cases)) {
    throw new Error('Unsupported or malformed bank catalog.');
  }
  return Promise.all(catalog.cases.map(async (entry) => {
    const manifest = normalizeBankProfiling(validateBankManifest(entry));
    const directory = path.join(extensionPath, 'bank', manifest.id);
    for (const variant of ['before', 'after']) {
      const fileName = manifest.files[variant] || manifest.files.harness;
      if (!fileName) continue;
      const source = await fs.readFile(path.join(directory, fileName), 'utf8');
      const snippetIndex = source.indexOf(manifest.code[variant]);
      if (snippetIndex >= 0) {
        manifest.code[`${variant}StartLine`] = source.slice(0, snippetIndex).split('\n').length;
      }
    }
    return {
      directory,
      manifest,
      beforeCode: manifest.code.before,
      afterCode: manifest.code.after,
    };
  }));
}

async function listCases(extensionPath) {
  const casesDirectory = path.join(extensionPath, 'cases');
  const entries = await fs.readdir(casesDirectory, { withFileTypes: true });
  const adaptedCases = await Promise.all(
    entries
      .filter((entry) => entry.isDirectory())
      .map((entry) => loadCase(extensionPath, entry.name)),
  );
  const bankCases = await loadBankCases(extensionPath);
  const bankByPerfbankId = new Map(
    bankCases.map((learningCase) => [learningCase.manifest.perfbankId, learningCase]),
  );
  const adaptedIds = new Set(adaptedCases.map((learningCase) => learningCase.manifest.perfbankId));
  for (const learningCase of adaptedCases) {
    const catalogCase = bankByPerfbankId.get(learningCase.manifest.perfbankId);
    learningCase.manifest.order = catalogCase?.manifest.order ?? learningCase.manifest.order;
    learningCase.manifest.tier = catalogCase?.manifest.tier || 'C++';
    learningCase.manifest.runner = {
      kind: 'instrumented-cpp',
      behavior: 'fresh',
      rounds: 7,
      protocol: 'The adapted harness checks shared inputs, then times seven alternating rounds.',
    };
    learningCase.manifest.profiling.kind = 'hpctoolkit';
    learningCase.manifest.evidence = catalogCase?.manifest.evidence || {
      status: 'cloudlab-verified',
      speedup: learningCase.manifest.provenance.reportedSpeedup,
      wins: '7/7',
    };
  }
  const cases = [
    ...bankCases.filter((learningCase) => !adaptedIds.has(learningCase.manifest.perfbankId)),
    ...adaptedCases,
  ];
  return cases.sort((left, right) => {
    const leftOrder = Number.isFinite(left.manifest.order) ? left.manifest.order : 999;
    const rightOrder = Number.isFinite(right.manifest.order) ? right.manifest.order : 999;
    return leftOrder - rightOrder || left.manifest.title.localeCompare(right.manifest.title);
  });
}

module.exports = {
  BANK_PROFILE_EVENTS,
  BANK_PROFILE_METRICS,
  CASE_ID: DEFAULT_CASE_ID,
  DEFAULT_CASE_ID,
  listCases,
  loadCase,
  loadBankCases,
  validateCaseManifest,
  validateBankManifest,
  validateProfile,
};
