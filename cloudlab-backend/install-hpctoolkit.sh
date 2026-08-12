#!/usr/bin/env bash
set -euo pipefail

# Spack v0.23.1 bundles its matching package recipes, avoiding an unpinned
# spack-packages checkout. Captured CloudLab images retain this one-time build.
readonly SPACK_COMMIT="2bfcc69fa870d3c6919be87593f22647981b648a"
readonly HPCTOOLKIT_BASE_SPEC="hpctoolkit@2024.01.1~viewer~mpi+papi"
readonly EDUPERF_SPACK_ROOT="/opt/eduperf-spack"
readonly PREFIX_FILE="/opt/eduperf/hpctoolkit-prefix"

export SPACK_USER_CONFIG_PATH="/opt/eduperf-spack-v0.23-config"
export SPACK_USER_CACHE_PATH="/opt/eduperf-spack-v0.23-cache"

if [[ -s "${PREFIX_FILE}" ]] && [[ -x "$(<"${PREFIX_FILE}")/bin/hpcrun" ]]; then
  exit 0
fi

install -d -m 0755 "${SPACK_USER_CONFIG_PATH}" "${SPACK_USER_CACHE_PATH}"

if [[ ! -d "${EDUPERF_SPACK_ROOT}/.git" ]]; then
  git clone --filter=blob:none https://github.com/spack/spack.git "${EDUPERF_SPACK_ROOT}"
fi
git -C "${EDUPERF_SPACK_ROOT}" fetch --depth=1 origin "${SPACK_COMMIT}"
git -C "${EDUPERF_SPACK_ROOT}" checkout --detach "${SPACK_COMMIT}"

# shellcheck disable=SC1091
source "${EDUPERF_SPACK_ROOT}/share/spack/setup-env.sh"
spack compiler find
compiler_version="$(g++ -dumpfullversion -dumpversion)"
if [[ ! "${compiler_version}" =~ ^[0-9]+(\.[0-9]+){1,2}$ ]]; then
  echo "Could not determine the system g++ version." >&2
  exit 1
fi
hpctoolkit_spec="${HPCTOOLKIT_BASE_SPEC}%gcc@=${compiler_version} target=haswell"
spack install --fail-fast "${hpctoolkit_spec}"
hpctoolkit_prefix="$(spack location -i "${hpctoolkit_spec}")"
test -x "${hpctoolkit_prefix}/bin/hpcrun"
install -d -m 0755 "$(dirname "${PREFIX_FILE}")"
printf '%s\n' "${hpctoolkit_prefix}" > "${PREFIX_FILE}"
chmod 0644 "${PREFIX_FILE}"
