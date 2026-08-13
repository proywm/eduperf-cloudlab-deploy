#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_DIRECTORY="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
"${SCRIPT_DIRECTORY}/install-hpctoolkit.sh"
echo "EduPerf profiling, including Python calling contexts, is ready."
