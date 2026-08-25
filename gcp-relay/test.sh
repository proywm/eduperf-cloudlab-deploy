#!/usr/bin/env bash
set -euo pipefail

readonly script_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
temporary_root="$(mktemp -d)"
cleanup() { rm -rf -- "${temporary_root}"; }
trap cleanup EXIT

bash -n \
  "${script_root}/startup.sh" \
  "${script_root}/configure-srl1.sh" \
  "${script_root}/deploy.sh"

/usr/bin/openssl req -x509 -newkey rsa:2048 -sha256 -nodes -days 1 \
  -keyout "${temporary_root}/key.pem" \
  -out "${temporary_root}/cert.pem" \
  -subj '/CN=203.0.113.10' \
  -addext 'subjectAltName=IP:203.0.113.10' \
  >/dev/null 2>&1
node "${script_root}/write-endpoint.js" \
  203.0.113.10 "${temporary_root}/cert.pem" "${temporary_root}/endpoint.json" \
  >/dev/null
node - "${temporary_root}/endpoint.json" <<'NODE'
const fs = require('node:fs');
const endpoint = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
if (endpoint.schemaVersion !== 1) throw new Error('Wrong endpoint schema.');
if (endpoint.url !== 'https://203.0.113.10:8443') throw new Error('Wrong endpoint URL.');
if (endpoint.label !== 'EduPerf U.S. classroom service') throw new Error('Wrong label.');
if (!endpoint.certificate.includes('BEGIN CERTIFICATE')) throw new Error('Missing certificate.');
NODE

if "${script_root}/deploy.sh" --region europe-west1 --zone europe-west1-b >/dev/null 2>&1; then
  echo "The relay deployment accepted a non-U.S. region." >&2
  exit 1
fi

echo "U.S. relay deployment contract passed"
