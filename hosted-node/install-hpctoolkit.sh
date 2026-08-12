#!/usr/bin/env bash
set -euo pipefail

# Spack v0.23.1 bundles the matching package recipes instead of resolving an
# independently moving spack-packages repository at installation time.
readonly SPACK_COMMIT="2bfcc69fa870d3c6919be87593f22647981b648a"
readonly HPCTOOLKIT_BASE_SPEC="hpctoolkit@2024.01.1~viewer~mpi+papi"
readonly DATA_ROOT="${XDG_DATA_HOME:-${HOME}/.local/share}/eduperf"
readonly CONFIG_ROOT="${XDG_CONFIG_HOME:-${HOME}/.config}/eduperf"
readonly EDUPERF_SPACK_ROOT="${DATA_ROOT}/spack"
readonly PREFIX_FILE="${CONFIG_ROOT}/hpctoolkit-prefix"

export SPACK_USER_CONFIG_PATH="${DATA_ROOT}/spack-v0.23-config"
export SPACK_USER_CACHE_PATH="${DATA_ROOT}/spack-v0.23-cache"

for command_name in git python3 g++; do
  if ! command -v "${command_name}" >/dev/null 2>&1; then
    echo "Required command is missing: ${command_name}" >&2
    exit 1
  fi
done

install -d -m 0755 "${DATA_ROOT}" "${SPACK_USER_CONFIG_PATH}" "${SPACK_USER_CACHE_PATH}"
install -d -m 0700 "${CONFIG_ROOT}"
if [[ ! -d "${EDUPERF_SPACK_ROOT}/.git" ]]; then
  git clone --filter=blob:none https://github.com/spack/spack.git "${EDUPERF_SPACK_ROOT}"
fi
git -C "${EDUPERF_SPACK_ROOT}" fetch --depth=1 origin "${SPACK_COMMIT}"
git -C "${EDUPERF_SPACK_ROOT}" checkout --detach "${SPACK_COMMIT}"

# shellcheck disable=SC1091
source "${EDUPERF_SPACK_ROOT}/share/spack/setup-env.sh"
spack compiler find
spack config add 'config:build_jobs:16'
# Some long-lived teaching hosts have a newer gcc binary without the matching
# g++ package. Spack otherwise selects that incomplete compiler because it has
# the highest version. Pin the concrete spec to the complete system g++ pair.
compiler_version="$(g++ -dumpfullversion -dumpversion)"
if [[ ! "${compiler_version}" =~ ^[0-9]+(\.[0-9]+){1,2}$ ]]; then
  echo "Could not determine the system g++ version." >&2
  exit 1
fi
hpctoolkit_spec="${HPCTOOLKIT_BASE_SPEC}%gcc@=${compiler_version} target=haswell"
spack install --fail-fast "${hpctoolkit_spec}"
hpctoolkit_prefix="$(spack location -i "${hpctoolkit_spec}")"
test -x "${hpctoolkit_prefix}/bin/hpcrun"
printf '%s\n' "${hpctoolkit_prefix}" > "${PREFIX_FILE}"
chmod 0600 "${PREFIX_FILE}"

echo "HPCToolkit is ready at ${hpctoolkit_prefix}"
echo "Rerun hosted-node/install.sh to attach it to the EduPerf service."
