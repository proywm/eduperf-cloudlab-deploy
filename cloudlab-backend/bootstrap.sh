#!/usr/bin/env bash
set -euo pipefail

readonly EDUPERF_ROOT="/opt/eduperf"
readonly EDUPERF_STATE="/local/eduperf"
readonly REPOSITORY="/local/repository"
readonly NODE_VERSION="22.23.2"
readonly NODE_ARCHIVE="node-v${NODE_VERSION}-linux-x64.tar.xz"
readonly NODE_URL="https://nodejs.org/download/release/v${NODE_VERSION}"

if [[ "${EUID}" -ne 0 ]]; then
  echo "EduPerf bootstrap must run as root." >&2
  exit 1
fi

install -d -m 0755 "${EDUPERF_ROOT}" "${EDUPERF_STATE}" "${EDUPERF_STATE}/work"
exec > >(tee -a "${EDUPERF_STATE}/bootstrap.log") 2>&1

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
  binutils build-essential bzip2 ca-certificates cmake curl file gfortran git \
  libboost-all-dev libdw-dev libeigen3-dev libelf-dev libiberty-dev liblzma-dev libssl-dev \
  libunwind-dev libxerces-c-dev libzstd-dev ninja-build openssl patch pkg-config \
  python3 python3-pip python3-venv tar unzip xz-utils zlib1g-dev

temporary_directory="$(mktemp -d)"
trap 'rm -rf -- "${temporary_directory}"' EXIT
if [[ ! -x "/opt/node-v${NODE_VERSION}/bin/node" ]]; then
  curl --fail --location --proto '=https' --tlsv1.2 \
    "${NODE_URL}/${NODE_ARCHIVE}" -o "${temporary_directory}/${NODE_ARCHIVE}"
  curl --fail --location --proto '=https' --tlsv1.2 \
    "${NODE_URL}/SHASUMS256.txt" -o "${temporary_directory}/SHASUMS256.txt"
  (
    cd "${temporary_directory}"
    grep " ${NODE_ARCHIVE}$" SHASUMS256.txt | sha256sum --check --strict -
  )
  tar -xJf "${temporary_directory}/${NODE_ARCHIVE}" -C /opt
  ln -sfn "/opt/node-v${NODE_VERSION}-linux-x64" "/opt/node-v${NODE_VERSION}"
fi
export PATH="/opt/node-v${NODE_VERSION}/bin:${PATH}"

# A repository-based profile is cloned into /local/repository. Preserve the
# large portable runtime baked into a captured image, then rebuild the fixed
# C++ adapters below so changed lesson sources cannot use stale executables.
if [[ -d "${EDUPERF_ROOT}/workloads/portable" ]]; then
  cp -a "${EDUPERF_ROOT}/workloads/portable" "${temporary_directory}/portable"
fi
rm -rf -- "${EDUPERF_ROOT}/workloads" "${EDUPERF_ROOT}/cloudlab-backend"
cp -a "${REPOSITORY}/workloads" "${EDUPERF_ROOT}/workloads"
cp -a "${REPOSITORY}/cloudlab-backend" "${EDUPERF_ROOT}/cloudlab-backend"
if [[ -d "${temporary_directory}/portable" ]]; then
  rm -rf -- "${EDUPERF_ROOT}/workloads/portable"
  cp -a "${temporary_directory}/portable" "${EDUPERF_ROOT}/workloads/portable"
fi

# Build the 100 fixed adapters and portable Python. A captured image reuses its
# 100 MB Python runtime and recompiles only the 34 small fixed C++ drivers.
node "${EDUPERF_ROOT}/workloads/scripts/build-portable.js"

install_hpctoolkit="true"
if command -v geni-get >/dev/null 2>&1; then
  install_hpctoolkit="$(geni-get 'param install_hpctoolkit' 2>/dev/null || echo true)"
fi
case "${install_hpctoolkit,,}" in
  true|1|yes)
    "${EDUPERF_ROOT}/cloudlab-backend/install-hpctoolkit.sh"
    ;;
esac

umask 077
openssl rand -hex 32 > "${EDUPERF_STATE}/api-token"
chmod 0600 "${EDUPERF_STATE}/api-token"

fqdn="$(hostname -f)"
short_hostname="$(hostname -s)"
openssl req -x509 -newkey rsa:3072 -sha256 -nodes -days 30 \
  -keyout "${EDUPERF_STATE}/tls-key.pem" \
  -out "${EDUPERF_STATE}/tls-cert.pem" \
  -subj "/CN=${fqdn}" \
  -addext "subjectAltName=DNS:${fqdn},DNS:${short_hostname}"
chmod 0600 "${EDUPERF_STATE}/tls-key.pem"
chmod 0644 "${EDUPERF_STATE}/tls-cert.pem"

hpctoolkit_root=""
profile_python=""
if [[ -s "${EDUPERF_ROOT}/hpctoolkit-prefix" ]]; then
  hpctoolkit_root="$(<"${EDUPERF_ROOT}/hpctoolkit-prefix")"
fi
if [[ -s "${EDUPERF_ROOT}/hpctoolkit-python-prefix" ]]; then
  profile_python="$(<"${EDUPERF_ROOT}/hpctoolkit-python-prefix")/bin/python3.11"
fi

cat > /etc/systemd/system/eduperf-backend.service <<EOF
[Unit]
Description=EduPerf deterministic CloudLab measurement worker
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=root
WorkingDirectory=${EDUPERF_ROOT}
Environment=PATH=/opt/node-v${NODE_VERSION}/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
Environment=EDUPERF_WORKLOAD_DIR=${EDUPERF_ROOT}/workloads
Environment=EDUPERF_WORK_DIR=${EDUPERF_STATE}/work
Environment=EDUPERF_API_TOKEN_FILE=${EDUPERF_STATE}/api-token
Environment=EDUPERF_TLS_CERT=${EDUPERF_STATE}/tls-cert.pem
Environment=EDUPERF_TLS_KEY=${EDUPERF_STATE}/tls-key.pem
Environment=EDUPERF_HPCTOOLKIT_ROOT=${hpctoolkit_root}
Environment=EDUPERF_PROFILE_PYTHON=${profile_python}
Environment=EDUPERF_ENVIRONMENT_KIND=cloudlab
Environment=EDUPERF_NODE_TYPE=m510
Environment=EDUPERF_WORKER_LABEL=${fqdn}
Environment=EDUPERF_BACKEND_REVISION=$(git -C "${EDUPERF_ROOT}" rev-parse HEAD)
Environment=EDUPERF_PORT=8443
ExecStart=/opt/node-v${NODE_VERSION}/bin/node ${EDUPERF_ROOT}/cloudlab-backend/server.js
Restart=on-failure
RestartSec=2
CPUAffinity=0
Nice=-10
NoNewPrivileges=true
PrivateTmp=true
ProtectHome=true
ProtectSystem=strict
ReadWritePaths=${EDUPERF_STATE}

[Install]
WantedBy=multi-user.target
EOF

cat > /etc/sysctl.d/90-eduperf-perf.conf <<'EOF'
# CloudLab allocates the entire physical node to this experiment. Permit the
# root-owned, allowlisted worker to collect hardware performance counters.
kernel.perf_event_paranoid=1
kernel.kptr_restrict=0
EOF
sysctl --system

systemctl daemon-reload
systemctl enable --now eduperf-backend.service

# This is the single import artifact for the instructor. The private token and
# self-signed certificate stay outside the git checkout and are not captured
# into student-facing lesson data.
python3 - "${fqdn}" "${EDUPERF_STATE}" <<'PY'
import json
import pathlib
import sys

fqdn = sys.argv[1]
state = pathlib.Path(sys.argv[2])
connection = {
    "schemaVersion": 1,
    "url": f"https://{fqdn}:8443",
    "token": (state / "api-token").read_text(encoding="utf-8").strip(),
    "certificate": (state / "tls-cert.pem").read_text(encoding="utf-8"),
    "label": f"CloudLab m510 · {fqdn}",
}
(state / "connection.json").write_text(
    json.dumps(connection, indent=2) + "\n", encoding="utf-8"
)
PY
chmod 0600 "${EDUPERF_STATE}/connection.json"

curl --fail --silent --cacert "${EDUPERF_STATE}/tls-cert.pem" \
  "https://${fqdn}:8443/v1/health" >/dev/null
echo "EduPerf backend is ready at https://${fqdn}:8443"
