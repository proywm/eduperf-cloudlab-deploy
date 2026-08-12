#!/usr/bin/env bash
set -euo pipefail

readonly PUBLIC_HOST="${1:-${EDUPERF_PUBLIC_HOST:-}}"
readonly REPOSITORY_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
readonly STATE_ROOT="${XDG_STATE_HOME:-${HOME}/.local/state}/eduperf"
readonly CONFIG_ROOT="${XDG_CONFIG_HOME:-${HOME}/.config}/eduperf"
readonly SERVICE_DIRECTORY="${XDG_CONFIG_HOME:-${HOME}/.config}/systemd/user"
readonly SERVICE_FILE="${SERVICE_DIRECTORY}/eduperf-backend.service"
readonly NODE_BIN="$(command -v node || true)"

if [[ -z "${PUBLIC_HOST}" ]]; then
  echo "Usage: $0 PUBLIC_IP_OR_DNS_NAME" >&2
  exit 2
fi
if [[ ! "${PUBLIC_HOST}" =~ ^[A-Za-z0-9.:_-]+$ ]]; then
  echo "The public host contains unsupported characters." >&2
  exit 2
fi

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

umask 077
openssl rand -hex 32 > "${STATE_ROOT}/api-token"

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
chmod 0600 "${STATE_ROOT}/api-token" "${STATE_ROOT}/tls-key.pem"
chmod 0644 "${STATE_ROOT}/tls-cert.pem"

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
Environment=EDUPERF_API_TOKEN_FILE=${STATE_ROOT}/api-token
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

env PUBLIC_HOST="${PUBLIC_HOST}" STATE_ROOT="${STATE_ROOT}" node <<'NODE'
const fs = require('node:fs');
const path = require('node:path');

const stateRoot = process.env.STATE_ROOT;
const connection = {
  schemaVersion: 1,
  url: `https://${process.env.PUBLIC_HOST}:8443`,
  token: fs.readFileSync(path.join(stateRoot, 'api-token'), 'utf8').trim(),
  certificate: fs.readFileSync(path.join(stateRoot, 'tls-cert.pem'), 'utf8'),
  label: `Hosted EduPerf · ${process.env.PUBLIC_HOST}`,
};
fs.writeFileSync(
  path.join(stateRoot, 'connection.json'),
  `${JSON.stringify(connection, null, 2)}\n`,
  { mode: 0o600 },
);
NODE

echo "EduPerf is ready at https://${PUBLIC_HOST}:8443"
echo "Instructor connection file: ${STATE_ROOT}/connection.json"
