const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');

const {
  executeFile,
  findExecutable,
  parseCppDriver,
  parsePythonTime,
  parseResult,
} = require('./runner');

const EVENT_KEYS = {
  CYCLES: 'cycles',
  INSTRUCTIONS: 'instructions',
  BRANCHES: 'branches',
  'BRANCH-INSTRUCTIONS': 'branches',
  'BRANCH-MISSES': 'branchMisses',
  'CACHE-MISSES': 'cacheMisses',
  PAPI_TOT_CYC: 'cycles',
  PAPI_TOT_INS: 'instructions',
  PAPI_BR_INS: 'branches',
  PAPI_BR_MSP: 'branchMisses',
  PAPI_L2_DCM: 'cacheMisses',
};

const PROFILE_SIZED_PYTHON_CASES = new Set(['e12', 'e32', 'e35', 'p08', 'p35']);

function executableOnPath(name) {
  const pathValue = process.env.PATH || '';
  return Promise.all(
    pathValue.split(path.delimiter).filter(Boolean).map(async (directory) => {
      const candidate = path.join(directory, name);
      try {
        await fs.access(candidate, fs.constants.X_OK);
        return candidate;
      } catch {
        return undefined;
      }
    }),
  ).then((matches) => matches.find(Boolean));
}

async function firstWorking(candidates) {
  for (const candidate of candidates.filter(Boolean)) {
    try {
      await executeFile(candidate, ['--version'], { timeout: 5_000 });
      return candidate;
    } catch (error) {
      // Some HPCToolkit 2024 utilities print a valid version and exit 1.
      const response = `${error.stdout || ''}\n${error.stderr || ''}`;
      if (response.includes('HPCToolkit') && response.includes('version')) {
        return candidate;
      }
    }
  }
  return undefined;
}

async function detectHpctoolkit(configuredRoot = '', platform = process.platform) {
  if (platform !== 'linux') {
    throw new Error('Local HPCToolkit collection is currently supported on Linux only.');
  }

  let hpcrun;
  let hpcstruct;
  let hpcproftt;
  if (configuredRoot) {
    const root = path.resolve(configuredRoot);
    hpcrun = await firstWorking([path.join(root, 'bin', 'hpcrun')]);
    hpcstruct = await firstWorking([path.join(root, 'bin', 'hpcstruct')]);
    hpcproftt = await firstWorking([
      path.join(root, 'libexec', 'hpctoolkit', 'hpcproftt'),
      path.join(root, 'bin', 'hpcproftt'),
    ]);
  } else {
    hpcrun = await executableOnPath('hpcrun');
    hpcstruct = await executableOnPath('hpcstruct');
    hpcproftt = await executableOnPath('hpcproftt');
    if (hpcrun && !hpcproftt) {
      const prefix = path.dirname(path.dirname(await fs.realpath(hpcrun)));
      hpcproftt = await firstWorking([
        path.join(prefix, 'libexec', 'hpctoolkit', 'hpcproftt'),
      ]);
    }
  }

  const addr2line = await findExecutable('', ['addr2line', 'llvm-addr2line'], 'address resolver');
  const missing = [
    !hpcrun && 'hpcrun',
    !hpcstruct && 'hpcstruct',
    !hpcproftt && 'hpcproftt',
    !addr2line && 'addr2line',
  ].filter(Boolean);
  if (missing.length > 0) {
    throw new Error(
      `HPCToolkit local profiling is unavailable (${missing.join(', ')} not found). `
      + 'Set eduperf.hpctoolkitRoot to the installation prefix.',
    );
  }
  return { hpcrun, hpcstruct, hpcproftt, addr2line };
}

async function toolVersion(command) {
  try {
    const result = await executeFile(command, ['--version'], { timeout: 5_000 });
    return `${result.stdout}\n${result.stderr}`
      .split(/\r?\n/)
      .map((line) => line.trim())
      .find(Boolean) || path.basename(command);
  } catch {
    return path.basename(command);
  }
}

function parseHpcproftt(text) {
  const programMatch = text.match(/\[nv-pair: 'program-path', '([^']+)'\]/);
  const programPath = programMatch ? programMatch[1] : undefined;
  const loadModules = [];
  for (const match of text.matchAll(/\[\(id:\s*(\d+)\) \(nm:\s*(.*?)\) \(flg:/g)) {
    loadModules.push({ id: Number(match[1]), path: match[2] });
  }

  const nodes = [];
  const nodePattern = /\[node: \(id:\s*(\d+)\) \(id-parent:\s*(\d+)\) \(lm-id:\s*(\d+)\) \(lm-ip:\s*(0x[0-9a-fA-F]+)\)([^\n]*)\]/g;
  for (const match of text.matchAll(nodePattern)) {
    const labels = [...match[5].matchAll(/["']([^"']+)["']/g)].map((item) => item[1]);
    nodes.push({
      id: Number(match[1]),
      parentId: Number(match[2]),
      loadModuleId: Number(match[3]),
      address: match[4],
      rawLabel: labels.at(-1),
      metrics: {},
    });
  }

  const metrics = [];
  const metricTable = text.match(/\[metric-tbl:[\s\S]*?\n\]/);
  if (metricTable) {
    const metricPattern = /\[\(id:\s*(\d+)\) \(nm:\s*([^)]+)\)[\s\S]*?\(period:\s*([0-9.eE+-]+)\)[\s\S]*?\(frequency:\s*(\d+)\)[\s\S]*?\(period-mean:\s*([0-9.eE+-]+)\)[\s\S]*?\(num-samples:\s*(\d+)\)\]/g;
    for (const match of metricTable[0].matchAll(metricPattern)) {
      metrics.push({
        id: Number(match[1]),
        name: match[2].trim(),
        period: Number(match[3]),
        frequency: match[4] === '1',
        periodMean: Number(match[5]),
        samples: Number(match[6]),
      });
    }
  }

  const nodeById = new Map(nodes.map((node) => [node.id, node]));
  for (const match of text.matchAll(/\(cct node id:\s*(\d+)\)((?: \(metric \d+:[^)]+\))+)/g)) {
    const node = nodeById.get(Number(match[1]));
    if (!node) {
      continue;
    }
    for (const metricMatch of match[2].matchAll(/\(metric\s+(\d+):([^)]+)\)/g)) {
      node.metrics[Number(metricMatch[1])] = Number(metricMatch[2]);
    }
  }

  return { programPath, loadModules, nodes, metrics };
}

function parseAddr2line(output) {
  const lines = output.split(/\r?\n/).filter((line) => line.length > 0);
  const resolved = new Map();
  for (let index = 0; index + 2 < lines.length; index += 3) {
    const address = canonicalAddress(lines[index]);
    const functionName = lines[index + 1];
    const location = lines[index + 2];
    const locationMatch = location.match(/^(.*?):(\d+)(?:\s|$)/);
    resolved.set(address, {
      functionName,
      file: locationMatch && locationMatch[1] !== '??' ? locationMatch[1] : undefined,
      line: locationMatch ? Number(locationMatch[2]) : undefined,
    });
  }
  return resolved;
}

function parseLogicalMetadata(buffer) {
  const magic = Buffer.from('HPCLOGICAL');
  if (!Buffer.isBuffer(buffer) || buffer.length < magic.length
      || !buffer.subarray(0, magic.length).equals(magic)) {
    throw new Error('HPCToolkit logical metadata has an invalid header.');
  }
  const entries = new Map();
  let offset = magic.length;
  const readUInt32 = () => {
    if (offset + 4 > buffer.length) throw new Error('HPCToolkit logical metadata is truncated.');
    const value = buffer.readUInt32BE(offset);
    offset += 4;
    return value;
  };
  const readString = (terminated = false) => {
    const length = readUInt32();
    if (length > 1024 * 1024 || offset + length > buffer.length) {
      throw new Error('HPCToolkit logical metadata contains an invalid string.');
    }
    const value = buffer.subarray(offset, offset + length).toString('utf8');
    offset += length;
    if (terminated) {
      if (offset >= buffer.length || buffer[offset] !== 0) {
        throw new Error('HPCToolkit logical metadata string is not terminated.');
      }
      offset += 1;
    }
    return value;
  };
  while (offset < buffer.length) {
    const id = readUInt32();
    const functionName = readString(true);
    const file = readString();
    const line = readUInt32();
    entries.set(id, { functionName, file: file || undefined, line: line || undefined });
  }
  return entries;
}

async function resolveLogicalNodes(parsed, measurements) {
  const measurementRoot = path.resolve(measurements);
  for (const module of parsed.loadModules.filter((candidate) =>
    candidate.path.includes(`${path.sep}logical${path.sep}`),
  )) {
    const metadataPath = path.resolve(module.path);
    if (!metadataPath.startsWith(`${measurementRoot}${path.sep}`)) continue;
    const entries = parseLogicalMetadata(await fs.readFile(metadataPath));
    for (const node of parsed.nodes.filter((candidate) => candidate.loadModuleId === module.id)) {
      const address = BigInt(node.address);
      const logicalId = Number(address >> 32n);
      const sampledLine = Number(address & 0xffffffffn);
      const metadata = entries.get(logicalId);
      if (!metadata) continue;
      node.resolution = {
        functionName: metadata.functionName,
        file: metadata.file,
        line: sampledLine || metadata.line,
      };
    }
  }
}

function canonicalAddress(value) {
  try {
    return `0x${BigInt(value).toString(16)}`;
  } catch {
    return String(value).toLowerCase();
  }
}

async function resolveApplicationNodes(parsed, executable, addr2line, signal) {
  // Exact path is the normal case; basename handles HPCToolkit's
  // canonicalized path when a symlink was used to launch.
  const selectedModule = parsed.loadModules.find((module) => module.path === executable)
    || parsed.loadModules.find((module) => module.path === parsed.programPath)
    || parsed.loadModules.find((module) => path.basename(module.path) === path.basename(executable));
  if (!selectedModule) {
    throw new Error('HPCToolkit output did not identify the profiled executable.');
  }

  const applicationNodes = parsed.nodes.filter(
    (node) => node.loadModuleId === selectedModule.id && node.address !== '0x0',
  );
  const addresses = [...new Set(applicationNodes.map((node) => node.address.toLowerCase()))];
  if (addresses.length === 0) {
    throw new Error('HPCToolkit did not record any application calling-context nodes.');
  }
  const result = await executeFile(
    addr2line,
    ['-a', '-f', '-C', '-e', executable, ...addresses],
    { timeout: 30_000, maxBuffer: 8 * 1024 * 1024, signal },
  );
  const resolutions = parseAddr2line(result.stdout);
  for (const node of applicationNodes) {
    node.resolution = resolutions.get(canonicalAddress(node.address));
  }
  return { applicationNodes, mainModuleId: selectedModule.id };
}

function normalizedMetrics(rawMetrics, metricDefinitions) {
  const result = {};
  for (const [metricId, value] of Object.entries(rawMetrics)) {
    const definition = metricDefinitions.find((metric) => metric.id === Number(metricId));
    const key = definition && EVENT_KEYS[definition.name.toUpperCase()];
    if (key) {
      result[key] = (result[key] || 0) + value;
    }
  }
  return result;
}

function addDerivedMetrics(metrics) {
  const result = { ...metrics };
  result.ipc = result.cycles > 0 && Number.isFinite(result.instructions)
    ? result.instructions / result.cycles
    : null;
  result.branchMissRate = result.branches > 0 && Number.isFinite(result.branchMisses)
    ? (100 * result.branchMisses) / result.branches
    : null;
  return result;
}

function sumMetrics(target, source) {
  for (const [key, value] of Object.entries(source)) {
    if (Number.isFinite(value) && !['ipc', 'branchMissRate'].includes(key)) {
      target[key] = (target[key] || 0) + value;
    }
  }
  return target;
}

function profiledWorkloadMetrics(parsed) {
  const total = {};
  for (const node of parsed.nodes) {
    sumMetrics(total, normalizedMetrics(node.metrics, parsed.metrics));
  }
  return addDerivedMetrics(total);
}

function shortFunctionName(name) {
  if (!name || name === '??') {
    return 'application code';
  }
  return name.replace(/\s+\[clone .*\]$/, '');
}

function buildContext(parsed, applicationNodes, targetFunction, variant) {
  const allById = new Map(parsed.nodes.map((node) => [node.id, node]));
  const appById = new Map(applicationNodes.map((node) => [node.id, node]));
  const childrenByParent = new Map();
  for (const node of parsed.nodes) {
    if (!childrenByParent.has(node.parentId)) childrenByParent.set(node.parentId, []);
    childrenByParent.get(node.parentId).push(node);
  }
  const matchesTarget = typeof targetFunction === 'function'
    ? targetFunction
    : (name) => name.startsWith(`${targetFunction}(`);
  const targetNodes = applicationNodes.filter((node) =>
    node.resolution
    && node.resolution.functionName
    && matchesTarget(node.resolution.functionName),
  );
  if (targetNodes.length === 0) {
    const label = typeof targetFunction === 'string' ? targetFunction : `${variant} application code`;
    throw new Error(`HPCToolkit did not resolve samples to ${label}.`);
  }
  const targetIds = new Set(targetNodes.map((node) => node.id));
  const targetRoots = targetNodes.filter((node) => {
    let parent = allById.get(node.parentId);
    while (parent) {
      if (targetIds.has(parent.id)) return false;
      parent = allById.get(parent.parentId);
    }
    return true;
  });

  const included = new Set();
  for (const targetNode of targetNodes) {
    let cursor = targetNode;
    while (cursor) {
      if (appById.has(cursor.id)) {
        included.add(cursor.id);
      }
      cursor = allById.get(cursor.parentId);
    }
  }

  const entries = new Map();
  for (const id of included) {
    const raw = appById.get(id);
    const resolution = raw.resolution || {};
    entries.set(id, {
      id: `${variant}-${id}`,
      label: shortFunctionName(resolution.functionName || raw.rawLabel),
      kind: resolution.functionName && matchesTarget(resolution.functionName)
        ? 'target'
        : 'caller',
      source: resolution.file && resolution.line
        ? { file: path.basename(resolution.file), line: resolution.line }
        : undefined,
      selfMetrics: normalizedMetrics(raw.metrics, parsed.metrics),
      metrics: {},
      children: [],
      rawParentId: raw.parentId,
    });
  }

  // The student-facing target total is inclusive: copies, allocation, string
  // helpers, and other callees are part of the source-level routine's cost.
  // Keep the visible tree focused on application callers/target lines while
  // folding metrics from omitted descendants into the top target node.
  function omittedDescendantMetrics(root) {
    const total = {};
    const pending = [...(childrenByParent.get(root.id) || [])];
    while (pending.length > 0) {
      const node = pending.pop();
      if (!included.has(node.id)) {
        sumMetrics(total, normalizedMetrics(node.metrics, parsed.metrics));
      }
      pending.push(...(childrenByParent.get(node.id) || []));
    }
    return total;
  }
  for (const targetRoot of targetRoots) {
    const entry = entries.get(targetRoot.id);
    if (entry) sumMetrics(entry.selfMetrics, omittedDescendantMetrics(targetRoot));
  }

  const roots = [];
  for (const [id, entry] of entries) {
    let parent = allById.get(entry.rawParentId);
    while (parent && !entries.has(parent.id)) {
      parent = allById.get(parent.parentId);
    }
    if (parent && entries.has(parent.id)) {
      entries.get(parent.id).children.push(entry);
    } else {
      roots.push(entry);
    }
    delete entry.rawParentId;
  }

  function aggregate(node) {
    const total = { ...node.selfMetrics };
    for (const child of node.children) {
      sumMetrics(total, aggregate(child));
    }
    node.metrics = addDerivedMetrics(total);
    delete node.selfMetrics;
    node.children.sort((left, right) => (right.metrics.cycles || 0) - (left.metrics.cycles || 0));
    return total;
  }
  roots.forEach(aggregate);
  roots.sort((left, right) => (right.metrics.cycles || 0) - (left.metrics.cycles || 0));

  const summary = {};
  for (const node of targetRoots) {
    const entry = entries.get(node.id);
    if (entry) sumMetrics(summary, entry.metrics);
  }
  return { context: roots, metrics: addDerivedMetrics(summary) };
}

function visibleSampledContext(parsed, variant, maximumLeaves = 18) {
  const nodeById = new Map(parsed.nodes.map((node) => [node.id, node]));
  const moduleById = new Map(parsed.loadModules.map((module) => [module.id, module]));
  const scored = parsed.nodes
    .map((node) => ({
      node,
      metrics: normalizedMetrics(node.metrics, parsed.metrics),
    }))
    .filter(({ metrics }) => Object.values(metrics).some((value) => Number.isFinite(value) && value > 0))
    .sort((left, right) =>
      (right.metrics.cycles || right.metrics.instructions || 0)
      - (left.metrics.cycles || left.metrics.instructions || 0),
    )
    .slice(0, maximumLeaves);

  const included = new Set();
  for (const { node } of scored) {
    let cursor = node;
    let depth = 0;
    while (cursor && depth < 24) {
      included.add(cursor.id);
      cursor = nodeById.get(cursor.parentId);
      depth += 1;
    }
  }

  function nodeLabel(raw) {
    const resolution = raw.resolution || {};
    if (resolution.functionName && resolution.functionName !== '??') {
      return shortFunctionName(resolution.functionName);
    }
    if (raw.rawLabel && !['^Primary', '| Main  ', '<program root>'].includes(raw.rawLabel)) {
      return raw.rawLabel;
    }
    const module = moduleById.get(raw.loadModuleId);
    if (module?.path) return `${path.basename(module.path)} · ${raw.address}`;
    return raw.rawLabel || 'profiled execution';
  }

  function sourceFor(raw) {
    const resolution = raw.resolution || {};
    if (resolution.file && resolution.line) {
      return { file: path.basename(resolution.file), line: resolution.line };
    }
    const module = moduleById.get(raw.loadModuleId);
    if (module?.path?.endsWith('.py')) {
      return { file: path.basename(module.path), line: Number.parseInt(raw.address, 16) || 1 };
    }
    return undefined;
  }

  const entries = new Map();
  for (const id of included) {
    const raw = nodeById.get(id);
    const source = sourceFor(raw);
    entries.set(id, {
      id: `${variant}-${id}`,
      label: nodeLabel(raw),
      kind: source && ['target.py', 'target_before.py', 'target_after.py'].includes(source.file)
        ? 'target'
        : 'caller',
      source,
      selfMetrics: normalizedMetrics(raw.metrics, parsed.metrics),
      metrics: {},
      children: [],
      rawParentId: raw.parentId,
    });
  }

  const roots = [];
  for (const entry of entries.values()) {
    let parent = nodeById.get(entry.rawParentId);
    while (parent && !entries.has(parent.id)) parent = nodeById.get(parent.parentId);
    if (parent && entries.has(parent.id)) entries.get(parent.id).children.push(entry);
    else roots.push(entry);
    delete entry.rawParentId;
  }

  function aggregate(entry) {
    const metrics = { ...entry.selfMetrics };
    for (const child of entry.children) sumMetrics(metrics, aggregate(child));
    entry.metrics = addDerivedMetrics(metrics);
    delete entry.selfMetrics;
    entry.children.sort((left, right) =>
      (right.metrics.cycles || right.metrics.instructions || 0)
      - (left.metrics.cycles || left.metrics.instructions || 0),
    );
    return metrics;
  }
  roots.forEach(aggregate);
  roots.sort((left, right) =>
    (right.metrics.cycles || right.metrics.instructions || 0)
    - (left.metrics.cycles || left.metrics.instructions || 0),
  );
  return roots;
}

async function findProfileFile(directory) {
  const pending = [directory];
  while (pending.length > 0) {
    const current = pending.pop();
    for (const entry of await fs.readdir(current, { withFileTypes: true })) {
      const entryPath = path.join(current, entry.name);
      if (entry.isDirectory()) {
        pending.push(entryPath);
      } else if (entry.name.endsWith('.hpcrun')) {
        return entryPath;
      }
    }
  }
  throw new Error('HPCToolkit did not produce an .hpcrun profile.');
}

async function validateStructure(structurePath, targetFunctions) {
  const structure = await fs.readFile(structurePath, 'utf8');
  for (const target of targetFunctions) {
    if (!structure.includes(`n="${target}(`)) {
      throw new Error(`hpcstruct could not recover the optimized routine ${target}.`);
    }
  }
}

async function collectVariant({
  tools,
  executable,
  directory,
  variant,
  mode,
  targetFunction,
  events,
  frequency,
  signal,
}) {
  const measurements = path.join(directory, `${variant}-measurements`);
  const eventArgs = events.flatMap((event) => ['-e', `${event}@f${frequency}`]);
  let run;
  try {
    run = await executeFile(
      tools.hpcrun,
      [...eventArgs, '-o', measurements, executable, `--${mode}`],
      {
        timeout: 90_000,
        signal,
        maxBuffer: 8 * 1024 * 1024,
        env: { ...process.env, LC_ALL: 'C' },
      },
    );
  } catch (error) {
    const detail = [error.stderr, error.stdout, error.message].filter(Boolean).join('\n').trim();
    throw new Error(`HPCToolkit could not collect the ${variant} profile.\n${detail}`);
  }
  const execution = parseResult(run.stdout, mode);
  const profileFile = await findProfileFile(measurements);
  const dump = await executeFile(tools.hpcproftt, ['-g', profileFile], {
    timeout: 30_000,
    signal,
    maxBuffer: 32 * 1024 * 1024,
  });
  const parsed = parseHpcproftt(dump.stdout);
  const { applicationNodes } = await resolveApplicationNodes(
    parsed,
    executable,
    tools.addr2line,
    signal,
  );
  const normalized = buildContext(parsed, applicationNodes, targetFunction, variant);
  // Sampling can land inside libc or the kernel while an I/O-heavy target is
  // blocked. Preserve target attribution when it exists, and use the complete
  // profiled workload for otherwise unavailable events. This avoids presenting
  // a missing cycle count merely because the interrupt arrived below an
  // unwinding boundary. The profile records the scope for the UI/provenance.
  const workloadMetrics = profiledWorkloadMetrics(parsed);
  const targetMetrics = normalized.metrics;
  const requestedMetricKeys = [...new Set(parsed.metrics
    .map((metric) => EVENT_KEYS[metric.name.toUpperCase()])
    .filter(Boolean))];
  const useWorkloadMetrics = requestedMetricKeys.some(
    (key) => !Number.isFinite(targetMetrics[key]),
  );
  if (useWorkloadMetrics) {
    normalized.metrics = workloadMetrics;
    if (normalized.context.length > 0) normalized.context[0].metrics = workloadMetrics;
  }
  return {
    runtimeUs: execution.elapsed_us,
    callCount: execution.calls,
    metrics: normalized.metrics,
    metricScope: useWorkloadMetrics ? 'profiled-workload' : 'target-inclusive',
    context: normalized.context,
    samples: Object.fromEntries(parsed.metrics.map((metric) => [metric.name, metric.samples])),
  };
}

async function collectPreservedVariant({
  tools,
  executable,
  args,
  cwd,
  directory,
  variant,
  events,
  frequency,
  pythonFrames,
  timeout,
  signal,
  environment,
  parseRuntime,
  targetMatcher,
}) {
  const measurements = path.join(directory, `${variant}-measurements`);
  const eventArgs = events.flatMap((event) => ['-e', `${event}@f${frequency}`]);
  let run;
  try {
    run = await executeFile(
      tools.hpcrun,
      [
        ...(pythonFrames ? ['-a', 'python'] : []),
        ...eventArgs,
        '-o', measurements,
        executable,
        ...args,
      ],
      {
        cwd,
        timeout,
        signal,
        maxBuffer: 8 * 1024 * 1024,
        env: environment,
      },
    );
  } catch (error) {
    const detail = [error.stderr, error.stdout, error.message].filter(Boolean).join('\n').trim();
    throw new Error(`HPCToolkit could not collect the ${variant} preserved profile.\n${detail}`);
  }

  const profileFile = await findProfileFile(measurements);
  const dump = await executeFile(tools.hpcproftt, ['-g', profileFile], {
    timeout: 30_000,
    signal,
    maxBuffer: 32 * 1024 * 1024,
  });
  const parsed = parseHpcproftt(dump.stdout);
  await resolveLogicalNodes(parsed, measurements);
  let applicationNodes = [];
  try {
    ({ applicationNodes } = await resolveApplicationNodes(
      parsed,
      executable,
      tools.addr2line,
      signal,
    ));
  } catch {
    // Logical Python frames can be useful even when the interpreter executable
    // lacks source debug information. They are preserved by the general CCT.
  }

  let normalized;
  if (targetMatcher && applicationNodes.length > 0) {
    try {
      normalized = buildContext(parsed, applicationNodes, targetMatcher, variant);
    } catch {
      normalized = undefined;
    }
  }
  const workloadMetrics = profiledWorkloadMetrics(parsed);
  const context = normalized?.context?.length
    ? normalized.context
    : visibleSampledContext(parsed, variant);
  const requestedMetricKeys = [...new Set(parsed.metrics
    .map((metric) => EVENT_KEYS[metric.name.toUpperCase()])
    .filter(Boolean))];
  const metrics = {};
  let usedWorkloadFallback = !normalized;
  for (const key of requestedMetricKeys) {
    if (Number.isFinite(normalized?.metrics?.[key])) metrics[key] = normalized.metrics[key];
    else if (Number.isFinite(workloadMetrics[key])) {
      metrics[key] = workloadMetrics[key];
      usedWorkloadFallback = true;
    }
  }
  return {
    runtimeUs: parseRuntime(run.stdout),
    callCount: 1,
    metrics: addDerivedMetrics(metrics),
    metricScope: normalized
      ? usedWorkloadFallback ? 'variant-inclusive with workload fallbacks' : 'variant-inclusive'
      : 'profiled-workload',
    context,
    samples: Object.fromEntries(parsed.metrics.map((metric) => [metric.name, metric.samples])),
  };
}

async function collectPreservedBankProfile({
  learningCase,
  executable,
  workDirectory,
  sourceDirectory = workDirectory,
  configuredRoot = '',
  signal,
  onProgress = () => {},
  provenanceKind = 'local',
}) {
  const tools = await detectHpctoolkit(configuredRoot);
  const manifest = learningCase.manifest;
  const events = manifest.profiling.events;
  const frequency = manifest.profiling.sampleFrequencyHz || 1009;
  const sessionDirectory = await fs.mkdtemp(path.join(workDirectory, 'hpctoolkit-bank-'));
  const pythonFrames = manifest.runner.kind === 'python-bench';
  const profileSizedInput = pythonFrames && PROFILE_SIZED_PYTHON_CASES.has(manifest.id);
  const environment = {
    ...process.env,
    LC_ALL: 'C',
    PYTHONDONTWRITEBYTECODE: '1',
    PYTHONHASHSEED: '0',
    PYTHONNOUSERSITE: '1',
    PYTHONPATH: '',
    // A few preserved adapters use multi-million-element timing workloads.
    // Keep Run & Compare unchanged, but let those adapters select a smaller,
    // deterministic workload under Python-frame sampling so collection stays
    // within the classroom request deadline.
    ...(profileSizedInput ? { EDUPERF_PROFILE: '1' } : {}),
  };
  try {
    const variants = {};
    for (const variant of ['before', 'after']) {
      onProgress(`Collecting ${variant} hardware events and calling context…`);
      if (pythonFrames) {
        await fs.copyFile(
          path.join(workDirectory, manifest.files[variant]),
          path.join(workDirectory, 'target.py'),
        );
      }
      variants[variant] = await collectPreservedVariant({
        tools,
        executable,
        args: pythonFrames ? [manifest.files.harness] : [],
        cwd: workDirectory,
        directory: sessionDirectory,
        variant,
        events,
        frequency,
        pythonFrames,
        timeout: manifest.runner.timeoutMs || 120_000,
        signal,
        environment,
        parseRuntime: pythonFrames
          ? (stdout) => parsePythonTime(stdout) * 1_000_000
          : (stdout) => {
            const benchmark = parseCppDriver(stdout).benchmark;
            return variant === 'before' ? benchmark.before_us : benchmark.after_us;
          },
        targetMatcher: pythonFrames
          ? undefined
          : (name) => new RegExp(`(?:^|::)v_${variant}::|${variant}`, 'i').test(name),
      });
      const pending = [...variants[variant].context];
      const sourceName = manifest.files[variant] || manifest.files.harness;
      const displayName = manifest.code[`${variant}Name`] || sourceName;
      const snippetStart = manifest.code[`${variant}StartLine`];
      const snippetLines = manifest.code[variant].replace(/\n$/, '').split('\n').length;
      const sourceLines = (await fs.readFile(path.join(sourceDirectory, sourceName), 'utf8')).split('\n');
      const displayLines = manifest.code[variant].replace(/\n$/, '').split('\n');
      const displayPositions = new Map();
      displayLines.forEach((line, index) => {
        const normalized = line.trim();
        if (!normalized) return;
        const positions = displayPositions.get(normalized) || [];
        positions.push(index + 1);
        displayPositions.set(normalized, positions);
      });
      const sourceToDisplay = new Map();
      sourceLines.forEach((line, index) => {
        const positions = displayPositions.get(line.trim());
        if (positions?.length === 1) sourceToDisplay.set(index + 1, positions[0]);
      });
      while (pending.length > 0) {
        const node = pending.pop();
        const isVariantSource = node.source?.file === sourceName
          || (pythonFrames && node.source?.file === 'target.py');
        const mappedLine = isVariantSource && sourceToDisplay.get(node.source.line);
        if (mappedLine) {
          node.source = { file: displayName, line: mappedLine };
        } else if (isVariantSource && Number.isFinite(snippetStart)
            && node.source.line >= snippetStart
            && node.source.line < snippetStart + snippetLines) {
          node.source = {
            file: displayName,
            line: node.source.line - snippetStart + 1,
          };
        }
        pending.push(...node.children);
      }
    }

    const hpctoolkitVersion = await toolVersion(tools.hpcrun);
    const runtimeVersion = await toolVersion(executable);
    return {
      schemaVersion: 1,
      caseId: manifest.id,
      provenance: {
        kind: provenanceKind,
        label: provenanceKind === 'cloudlab'
          ? 'Live CloudLab HPCToolkit profile'
          : provenanceKind === 'hosted'
            ? 'Live hosted HPCToolkit profile'
            : 'Local HPCToolkit profile',
        collectedAt: new Date().toISOString(),
        hpctoolkitVersion,
        compiler: pythonFrames ? runtimeVersion : 'EduPerf optimized C++ profiling build',
        buildFlags: pythonFrames ? ['HPCToolkit -a python'] : manifest.runner.flags,
        platform: `${process.platform} ${process.arch}`,
        processor: os.cpus()[0]?.model.trim() || 'Unknown processor',
        events,
        method: pythonFrames
          ? 'HPCToolkit statistical hardware-counter sampling with Python logical calling-context unwinding'
          : 'HPCToolkit statistical hardware-counter sampling with source-resolved C++ calling contexts',
      },
      workload: {
        description: profileSizedInput
          ? 'one deterministic profiling execution of the preserved PerfBank adapter using profile-sized parameters'
          : 'one complete execution of the preserved PerfBank benchmark adapter',
        callsPerVariant: 1,
        unitLabel: 'adapter run',
        sameInput: true,
        profileSizedInput,
      },
      variants,
    };
  } finally {
    await fs.rm(sessionDirectory, { recursive: true, force: true });
  }
}

async function collectHpctoolkitProfile({
  learningCase,
  executable,
  buildDirectory,
  configuredRoot = '',
  compiler,
  signal,
  onProgress = () => {},
  provenanceKind = 'local',
}) {
  const tools = await detectHpctoolkit(configuredRoot);
  await fs.mkdir(buildDirectory, { recursive: true });
  const sessionDirectory = await fs.mkdtemp(path.join(buildDirectory, 'hpctoolkit-'));
  const manifest = learningCase.manifest;
  try {
    onProgress('Recovering optimized source structure…');
    const structurePath = path.join(sessionDirectory, `${manifest.id}.hpcstruct`);
    await executeFile(tools.hpcstruct, ['-o', structurePath, executable], {
      timeout: 90_000,
      signal,
      maxBuffer: 8 * 1024 * 1024,
    });
    await validateStructure(structurePath, Object.values(manifest.profiling.targets));

    const variants = {};
    for (const variant of ['before', 'after']) {
      onProgress(`Collecting ${variant} calling-context metrics…`);
      variants[variant] = await collectVariant({
        tools,
        executable,
        directory: sessionDirectory,
        variant,
        mode: manifest.execution.profileModes[variant],
        targetFunction: manifest.profiling.targets[variant],
        events: manifest.profiling.events,
        frequency: manifest.profiling.sampleFrequencyHz,
        signal,
      });
    }
    if (variants.before.callCount !== variants.after.callCount) {
      throw new Error('The before and after HPCToolkit workloads used different call counts.');
    }

    const hpctoolkitVersion = await toolVersion(tools.hpcrun);
    const compilerVersion = await toolVersion(compiler);
    return {
      schemaVersion: 1,
      caseId: manifest.id,
      provenance: {
        kind: provenanceKind,
        label: provenanceKind === 'reference'
          ? 'Bundled HPCToolkit reference profile'
          : 'Local HPCToolkit profile',
        collectedAt: new Date().toISOString(),
        hpctoolkitVersion,
        compiler: compilerVersion,
        buildFlags: manifest.build.flags,
        platform: `${process.platform} ${process.arch}`,
        processor: os.cpus()[0] ? os.cpus()[0].model.trim() : 'Unknown processor',
        events: manifest.profiling.events,
        method: 'HPCToolkit statistical calling-context sampling; hpcstruct structure recovery; optimized debug-info source resolution',
      },
      workload: {
        description: manifest.execution.profileWorkload,
        callsPerVariant: variants.before.callCount,
        sameInput: true,
      },
      variants,
    };
  } finally {
    await fs.rm(sessionDirectory, { recursive: true, force: true });
  }
}

module.exports = {
  EVENT_KEYS,
  addDerivedMetrics,
  buildContext,
  collectHpctoolkitProfile,
  collectPreservedBankProfile,
  detectHpctoolkit,
  parseAddr2line,
  parseHpcproftt,
  parseLogicalMetadata,
  profiledWorkloadMetrics,
  visibleSampledContext,
  validateStructure,
};
