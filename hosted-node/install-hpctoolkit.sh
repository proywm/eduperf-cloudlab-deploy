#!/usr/bin/env bash
set -euo pipefail

readonly SPACK_COMMIT="570fa283b5787581c6ea4c50ddac7e10c8daa814"
readonly HPCTOOLKIT_SPEC="hpctoolkit@2024.01.1~viewer~mpi+papi target=haswell"
readonly DATA_ROOT="${XDG_DATA_HOME:-${HOME}/.local/share}/eduperf"
readonly CONFIG_ROOT="${XDG_CONFIG_HOME:-${HOME}/.config}/eduperf"
readonly SPACK_ROOT="${DATA_ROOT}/spack"
readonly PREFIX_FILE="${CONFIG_ROOT}/hpctoolkit-prefix"

for command_name in git python3; do
  if ! command -v "${command_name}" >/dev/null 2>&1; then
    echo "Required command is missing: ${command_name}" >&2
    exit 1
  fi
done

install -d -m 0755 "${DATA_ROOT}"
install -d -m 0700 "${CONFIG_ROOT}"
if [[ ! -d "${SPACK_ROOT}/.git" ]]; then
  git clone --filter=blob:none https://github.com/spack/spack.git "${SPACK_ROOT}"
fi
git -C "${SPACK_ROOT}" fetch --depth=1 origin "${SPACK_COMMIT}"
git -C "${SPACK_ROOT}" checkout --detach "${SPACK_COMMIT}"

# shellcheck disable=SC1091
source "${SPACK_ROOT}/share/spack/setup-env.sh"
spack compiler find
spack config add 'config:build_jobs:16'
spack install --fail-fast "${HPCTOOLKIT_SPEC}"
hpctoolkit_prefix="$(spack location -i "${HPCTOOLKIT_SPEC}")"
test -x "${hpctoolkit_prefix}/bin/hpcrun"
printf '%s\n' "${hpctoolkit_prefix}" > "${PREFIX_FILE}"
chmod 0600 "${PREFIX_FILE}"

echo "HPCToolkit is ready at ${hpctoolkit_prefix}"
echo "Rerun hosted-node/install.sh to attach it to the EduPerf service."
