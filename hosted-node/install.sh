#!/usr/bin/env bash
set -euo pipefail

PUBLIC_HOST="${1:-${EDUPERF_PUBLIC_HOST:-}}"
if (( $# > 0 )); then
  shift
fi
FACULTY_IDS=("$@")
if (( ${#FACULTY_IDS[@]} == 0 )); then
  FACULTY_IDS=("instructor")
fi
readonly PUBLIC_HOST
readonly FACULTY_IDS
readonly REPOSITORY_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
readonly STATE_ROOT="${XDG_STATE_HOME:-${HOME}/.local/state}/eduperf"
readonly CONFIG_ROOT="${XDG_CONFIG_HOME:-${HOME}/.config}/eduperf"
readonly SERVICE_DIRECTORY="${XDG_CONFIG_HOME:-${HOME}/.config}/systemd/user"
readonly SERVICE_FILE="${SERVICE_DIRECTORY}/eduperf-backend.service"
readonly NODE_BIN="$(command -v node || true)"

if [[ -z "${PUBLIC_HOST}" ]]; then
  echo "Usage: $0 PUBLIC_IP_OR_DNS_NAME [FACULTY_ID ...]" >&2
  exit 2
fi
if [[ ! "${PUBLIC_HOST}" =~ ^[A-Za-z0-9.:_-]+$ ]]; then
  echo "The public host contains unsupported characters." >&2
  exit 2
fi
for faculty_id in "${FACULTY_IDS[@]}"; do
  if [[ ! "${faculty_id}" =~ ^[A-Za-z0-9@._+-]{1,128}$ ]]; then
    echo "Invalid faculty credential id: ${faculty_id}" >&2
    exit 2
  fi
done

for command_name in curl g++ git node openssl strip systemctl tar; do
  if ! command -v "${command_name}" >/dev/null 2>&1; then
    echo "Required command is missing: ${command_name}" >&2
    exit 1
  fi
done
if [[ ! -r /usr/include/eigen3/Eigen/Core ]]; then
  echo "Eigen headers are missing at /usr/include/eigen3." >&2
  exit 1
fi
if ! systemctl --user show-environment >/dev/null 2>&1; then
  echo "The systemd user manager is unavailable." >&2
  exit 1
fi

node_major="$(node -p 'Number(process.versions.node.split(".")[0])')"
if (( node_major < 22 )); then
  echo "Node.js 22 or newer is required; found $(node --version)." >&2
  exit 1
fi

install -d -m 0700 "${STATE_ROOT}" "${CONFIG_ROOT}"
install -d -m 0755 "${SERVICE_DIRECTORY}" "${STATE_ROOT}/work"

# Build only the fixed, allowlisted workloads. Generated runtimes remain
# untracked and can be refreshed by rerunning this installer after a git pull.
node "${REPOSITORY_ROOT}/workloads/scripts/build-portable.js"

if [[ "${PUBLIC_HOST}" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]] || [[ "${PUBLIC_HOST}" == *:* ]]; then
  subject_alt_name="IP:${PUBLIC_HOST}"
else
  subject_alt_name="DNS:${PUBLIC_HOST}"
fi
openssl req -x509 -newkey rsa:3072 -sha256 -nodes -days 30 \
  -keyout "${STATE_ROOT}/tls-key.pem" \
  -out "${STATE_ROOT}/tls-cert.pem" \
  -subj "/CN=${PUBLIC_HOST}" \
  -addext "subjectAltName=${subject_alt_name}"
chmod 0600 "${STATE_ROOT}/tls-key.pem"
chmod 0644 "${STATE_ROOT}/tls-cert.pem"

umask 077
env STATE_ROOT="${STATE_ROOT}" PUBLIC_HOST="${PUBLIC_HOST}" \
  node - "${FACULTY_IDS[@]}" <<'NODE'
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

const stateRoot = process.env.STATE_ROOT;
const facultyIds = process.argv.slice(2);
const credentials = facultyIds.map((id) => ({
  id,
  label: id,
  token: crypto.randomBytes(32).toString('hex'),
}));
fs.writeFileSync(
  path.join(stateRoot, 'api-credentials.json'),
  `${JSON.stringify({ schemaVersion: 1, credentials }, null, 2)}\n`,
  { mode: 0o600 },
);

const certificate = fs.readFileSync(path.join(stateRoot, 'tls-cert.pem'), 'utf8');
const connectionsDirectory = path.join(stateRoot, 'connections');
fs.mkdirSync(connectionsDirectory, { recursive: true, mode: 0o700 });
const filenames = new Set();
for (const [index, credential] of credentials.entries()) {
  const basename = credential.id.replace(/[^A-Za-z0-9._-]/g, '_');
  if (filenames.has(basename)) throw new Error(`Credential filename collision: ${basename}`);
  filenames.add(basename);
  const connection = {
    schemaVersion: 1,
    url: `https://${process.env.PUBLIC_HOST}:8443`,
    token: credential.token,
    certificate,
    label: `Hosted EduPerf · ${credential.label}`,
  };
  const serialized = `${JSON.stringify(connection, null, 2)}\n`;
  fs.writeFileSync(
    path.join(connectionsDirectory, `${basename}.json`),
    serialized,
    { mode: 0o600 },
  );
  if (index === 0) {
    fs.writeFileSync(path.join(stateRoot, 'connection.json'), serialized, { mode: 0o600 });
  }
}
NODE
chmod 0600 "${STATE_ROOT}/api-credentials.json" "${STATE_ROOT}/connections/"*.json

hpctoolkit_root="${EDUPERF_HPCTOOLKIT_ROOT:-}"
if [[ -z "${hpctoolkit_root}" ]] && [[ -s "${CONFIG_ROOT}/hpctoolkit-prefix" ]]; then
  hpctoolkit_root="$(<"${CONFIG_ROOT}/hpctoolkit-prefix")"
fi

cat > "${SERVICE_FILE}" <<EOF
[Unit]
Description=EduPerf fixed-workload hosted measurement worker
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
WorkingDirectory=${REPOSITORY_ROOT}
Environment=EDUPERF_WORKLOAD_DIR=${REPOSITORY_ROOT}/workloads
Environment=EDUPERF_WORK_DIR=${STATE_ROOT}/work
Environment=EDUPERF_API_CREDENTIALS_FILE=${STATE_ROOT}/api-credentials.json
Environment=EDUPERF_TLS_CERT=${STATE_ROOT}/tls-cert.pem
Environment=EDUPERF_TLS_KEY=${STATE_ROOT}/tls-key.pem
Environment=EDUPERF_HPCTOOLKIT_ROOT=${hpctoolkit_root}
Environment=EDUPERF_ENVIRONMENT_KIND=hosted
Environment=EDUPERF_NODE_TYPE=hosted-x86-64
Environment=EDUPERF_WORKER_LABEL=$(hostname -f)
Environment=EDUPERF_PORT=8443
ExecStart=${NODE_BIN} ${REPOSITORY_ROOT}/cloudlab-backend/server.js
Restart=on-failure
RestartSec=2
CPUAffinity=0
NoNewPrivileges=true
PrivateTmp=true

[Install]
WantedBy=default.target
EOF

systemctl --user daemon-reload
systemctl --user enable eduperf-backend.service
systemctl --user restart eduperf-backend.service

for attempt in {1..30}; do
  if curl --fail --silent --connect-timeout 2 --noproxy '*' \
    --cacert "${STATE_ROOT}/tls-cert.pem" \
    --resolve "${PUBLIC_HOST}:8443:127.0.0.1" \
    "https://${PUBLIC_HOST}:8443/v1/health" >/dev/null; then
    break
  fi
  if (( attempt == 30 )); then
    systemctl --user status eduperf-backend.service --no-pager >&2 || true
    exit 1
  fi
  sleep 1
done

echo "EduPerf is ready at https://${PUBLIC_HOST}:8443"
echo "Faculty connection files: ${STATE_ROOT}/connections"
