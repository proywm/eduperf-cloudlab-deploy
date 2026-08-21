const crypto = require('node:crypto');
const fs = require('node:fs/promises');
const path = require('node:path');

function canonical(value) {
  if (Array.isArray(value)) return value.map(canonical);
  if (!value || typeof value !== 'object') return value;
  return Object.fromEntries(Object.keys(value).sort().map((key) => [key, canonical(value[key])]));
}

function adapterFiles(manifest) {
  const names = [];
  for (const [key, value] of Object.entries(manifest.files || {})) {
    if (key === 'referenceProfile') continue;
    if (Array.isArray(value)) names.push(...value);
    else if (typeof value === 'string') names.push(value);
  }
  return [...new Set(names)].sort();
}

async function adapterFingerprint(directory, manifest) {
  const files = [];
  for (const name of adapterFiles(manifest)) {
    const content = await fs.readFile(path.join(directory, name));
    files.push({
      name,
      sha256: crypto.createHash('sha256').update(content).digest('hex'),
    });
  }
  const definition = canonical({
    schemaVersion: manifest.schemaVersion,
    id: manifest.id,
    perfbankId: manifest.perfbankId,
    files: Object.fromEntries(
      Object.entries(manifest.files || {}).filter(([key]) => key !== 'referenceProfile'),
    ),
    runner: manifest.runner,
    build: manifest.build,
    execution: manifest.execution,
    profiling: {
      kind: manifest.profiling?.kind,
      targets: manifest.profiling?.targets,
      events: manifest.profiling?.events,
      sampleFrequencyHz: manifest.profiling?.sampleFrequencyHz,
    },
    code: manifest.code,
    adapters: files,
  });
  return crypto.createHash('sha256').update(JSON.stringify(definition)).digest('hex');
}

module.exports = { adapterFingerprint, adapterFiles };
