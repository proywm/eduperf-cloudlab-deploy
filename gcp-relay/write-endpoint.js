'use strict';

const fs = require('node:fs');
const path = require('node:path');

const [host, certificatePath, outputPath] = process.argv.slice(2);
if (!host || !certificatePath || !outputPath || !/^[A-Za-z0-9.-]+$/.test(host)) {
  console.error('Usage: node write-endpoint.js RELAY_HOST CERTIFICATE_PATH OUTPUT_PATH');
  process.exit(2);
}

const certificate = fs.readFileSync(certificatePath, 'utf8');
if (!certificate.includes('BEGIN CERTIFICATE')) {
  throw new Error('The fetched SRL1 certificate is invalid.');
}
const endpoint = {
  schemaVersion: 1,
  url: `https://${host}:8443`,
  label: 'EduPerf U.S. classroom service',
  certificate,
};
fs.mkdirSync(path.dirname(path.resolve(outputPath)), { recursive: true });
fs.writeFileSync(outputPath, `${JSON.stringify(endpoint, null, 2)}\n`, { mode: 0o644 });
console.log(`Wrote extension endpoint: ${outputPath}`);
