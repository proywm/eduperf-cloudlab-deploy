#!/usr/bin/env bash
set -euo pipefail

readonly REPOSITORY_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
readonly CONFIG_ROOT="${XDG_CONFIG_HOME:-${HOME}/.config}/eduperf"
readonly SERVICE_DROPIN="${XDG_CONFIG_HOME:-${HOME}/.config}/systemd/user/eduperf-backend.service.d"
readonly SPACK_ROOT="${XDG_DATA_HOME:-${HOME}/.local/share}/eduperf/spack"

if ! command -v g++ >/dev/null 2>&1; then
  echo "A C++ compiler is required to build the pinned HPCToolkit environment." >&2
  exit 1
fi
if ! systemctl --user show-environment >/dev/null 2>&1; then
  echo "The systemd user manager is unavailable." >&2
  exit 1
fi

install -d -m 0700 "${CONFIG_ROOT}"
EDUPERF_SPACK_ROOT="${SPACK_ROOT}" \
EDUPERF_HPCTOOLKIT_PREFIX_FILE="${CONFIG_ROOT}/hpctoolkit-prefix" \
EDUPERF_PYTHON_PREFIX_FILE="${CONFIG_ROOT}/hpctoolkit-python-prefix" \
SPACK_USER_CONFIG_PATH="${XDG_CONFIG_HOME:-${HOME}/.config}/eduperf/spack" \
SPACK_USER_CACHE_PATH="${XDG_CACHE_HOME:-${HOME}/.cache}/eduperf-spack" \
  "${REPOSITORY_ROOT}/cloudlab-backend/install-hpctoolkit.sh"

hpctoolkit_root="$(<"${CONFIG_ROOT}/hpctoolkit-prefix")"
profile_python="$(<"${CONFIG_ROOT}/hpctoolkit-python-prefix")/bin/python3.11"
test -x "${hpctoolkit_root}/bin/hpcrun"
test -x "${profile_python}"

install -d -m 0755 "${SERVICE_DROPIN}"
cat > "${SERVICE_DROPIN}/profiler.conf" <<EOF
[Service]
Environment=EDUPERF_HPCTOOLKIT_ROOT=${hpctoolkit_root}
Environment=EDUPERF_PROFILE_PYTHON=${profile_python}
EOF
systemctl --user daemon-reload
systemctl --user restart eduperf-backend.service
systemctl --user is-active --quiet eduperf-backend.service
echo "EduPerf HPCToolkit profiling, including Python calling contexts, is ready."
