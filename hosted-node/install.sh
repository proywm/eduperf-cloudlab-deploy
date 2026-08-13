#!/usr/bin/env bash
set -euo pipefail

PUBLIC_HOST="${1:-${EDUPERF_PUBLIC_HOST:-}}"
if (( $# > 0 )); then
  shift
fi
ALLOWED_EMAILS=()
if [[ "${1:-}" == "--allowlist-file" ]]; then
  if (( $# != 2 )) || [[ ! -r "${2:-}" ]]; then
    echo "--allowlist-file requires one readable roster file." >&2
    exit 2
  fi
  while IFS= read -r roster_line || [[ -n "${roster_line}" ]]; do
    roster_email="${roster_line%%#*}"
    roster_email="${roster_email//[[:space:]]/}"
    if [[ -n "${roster_email}" ]]; then
      ALLOWED_EMAILS+=("${roster_email}")
    fi
  done < "$2"
else
  ALLOWED_EMAILS=("$@")
fi
readonly PUBLIC_HOST
readonly ALLOWED_EMAILS
readonly REPOSITORY_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
readonly STATE_ROOT="${XDG_STATE_HOME:-${HOME}/.local/state}/eduperf"
readonly CONFIG_ROOT="${XDG_CONFIG_HOME:-${HOME}/.config}/eduperf"
readonly SERVICE_DIRECTORY="${XDG_CONFIG_HOME:-${HOME}/.config}/systemd/user"
readonly SERVICE_FILE="${SERVICE_DIRECTORY}/eduperf-backend.service"
readonly NODE_BIN="$(command -v node || true)"

if [[ -z "${PUBLIC_HOST}" ]] || (( ${#ALLOWED_EMAILS[@]} == 0 )); then
  echo "Usage: $0 PUBLIC_IP_OR_DNS_NAME ALLOWED_EMAIL [ALLOWED_EMAIL ...]" >&2
  echo "   or: $0 PUBLIC_IP_OR_DNS_NAME --allowlist-file ROSTER.txt" >&2
  exit 2
fi
if [[ ! "${PUBLIC_HOST}" =~ ^[A-Za-z0-9.:_-]+$ ]]; then
  echo "The public host contains unsupported characters." >&2
  exit 2
fi
for allowed_email in "${ALLOWED_EMAILS[@]}"; do
  if [[ ! "${allowed_email}" =~ ^[^[:space:]@]+@[^[:space:]@]+\.[^[:space:]@]+$ ]]; then
    echo "Invalid allowed email: ${allowed_email}" >&2
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
install -d -m 0700 "${STATE_ROOT}/auth-outbox"

# Build only the fixed, allowlisted workloads. Generated runtimes remain
# untracked and can be refreshed by rerunning this installer after a git pull.
node "${REPOSITORY_ROOT}/workloads/scripts/build-portable.js"

public_host_is_ip=false
if [[ "${PUBLIC_HOST}" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]] || [[ "${PUBLIC_HOST}" == *:* ]]; then
  public_host_is_ip=true
  subject_alt_name="IP:${PUBLIC_HOST}"
else
  subject_alt_name="DNS:${PUBLIC_HOST}"
fi
certificate_valid=false
if [[ -s "${STATE_ROOT}/tls-key.pem" ]] && [[ -s "${STATE_ROOT}/tls-cert.pem" ]] \
    && openssl x509 -in "${STATE_ROOT}/tls-cert.pem" -checkend 2592000 -noout >/dev/null; then
  if [[ "${public_host_is_ip}" == true ]] \
      && openssl x509 -in "${STATE_ROOT}/tls-cert.pem" -checkip "${PUBLIC_HOST}" -noout >/dev/null; then
    certificate_valid=true
  elif [[ "${public_host_is_ip}" == false ]] \
      && openssl x509 -in "${STATE_ROOT}/tls-cert.pem" -checkhost "${PUBLIC_HOST}" -noout >/dev/null; then
    certificate_valid=true
  fi
fi
if [[ "${certificate_valid}" == false ]]; then
  openssl req -x509 -newkey rsa:3072 -sha256 -nodes -days 365 \
    -keyout "${STATE_ROOT}/tls-key.pem" \
    -out "${STATE_ROOT}/tls-cert.pem" \
    -subj "/CN=${PUBLIC_HOST}" \
    -addext "subjectAltName=${subject_alt_name}"
fi
chmod 0600 "${STATE_ROOT}/tls-key.pem"
chmod 0644 "${STATE_ROOT}/tls-cert.pem"

umask 077
printf '%s\n' "${ALLOWED_EMAILS[@],,}" > "${CONFIG_ROOT}/allowed-emails.txt"
if [[ ! -s "${CONFIG_ROOT}/auth-secret" ]]; then
  openssl rand -base64 48 > "${CONFIG_ROOT}/auth-secret"
fi
if [[ ! -e "${CONFIG_ROOT}/email.env" ]]; then
  {
    printf '# The test outbox proves auth locally but does not deliver email.\n'
    printf 'EDUPERF_EMAIL_OUTBOX=%s\n' "${STATE_ROOT}/auth-outbox"
    printf '# For real delivery, add both lines below. Resend then takes precedence.\n'
    printf '# EDUPERF_RESEND_API_KEY=re_replace_me\n'
    printf '# EDUPERF_EMAIL_FROM="EduPerf <login@your-verified-domain.example>"\n'
  } > "${CONFIG_ROOT}/email.env"
fi
chmod 0600 "${CONFIG_ROOT}/allowed-emails.txt" "${CONFIG_ROOT}/auth-secret" "${CONFIG_ROOT}/email.env"

hpctoolkit_root="${EDUPERF_HPCTOOLKIT_ROOT:-}"
profile_python="${EDUPERF_PROFILE_PYTHON:-}"
if [[ -z "${hpctoolkit_root}" ]] && [[ -s "${CONFIG_ROOT}/hpctoolkit-prefix" ]]; then
  hpctoolkit_root="$(<"${CONFIG_ROOT}/hpctoolkit-prefix")"
fi
if [[ -z "${profile_python}" ]] && [[ -s "${CONFIG_ROOT}/hpctoolkit-python-prefix" ]]; then
  profile_python="$(<"${CONFIG_ROOT}/hpctoolkit-python-prefix")/bin/python3.11"
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
Environment=EDUPERF_ALLOWED_EMAILS_FILE=${CONFIG_ROOT}/allowed-emails.txt
Environment=EDUPERF_AUTH_SECRET_FILE=${CONFIG_ROOT}/auth-secret
EnvironmentFile=-${CONFIG_ROOT}/email.env
Environment=EDUPERF_TLS_CERT=${STATE_ROOT}/tls-cert.pem
Environment=EDUPERF_TLS_KEY=${STATE_ROOT}/tls-key.pem
Environment=EDUPERF_HPCTOOLKIT_ROOT=${hpctoolkit_root}
Environment=EDUPERF_PROFILE_PYTHON=${profile_python}
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
echo "Allowed email addresses: ${CONFIG_ROOT}/allowed-emails.txt"
echo "Email provider settings: ${CONFIG_ROOT}/email.env"
