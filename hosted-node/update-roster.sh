#!/usr/bin/env bash
set -euo pipefail

if (( $# != 1 )) || [[ ! -r "$1" ]]; then
  echo "Usage: $0 COURSE_ROSTER.txt" >&2
  exit 2
fi

readonly ROSTER_FILE="$1"
readonly CONFIG_ROOT="${XDG_CONFIG_HOME:-${HOME}/.config}/eduperf"
readonly ALLOWLIST_FILE="${CONFIG_ROOT}/allowed-emails.txt"

if [[ ! -e "${ALLOWLIST_FILE}" ]]; then
  echo "Run hosted-node/install.sh before updating the roster." >&2
  exit 1
fi

declare -A seen=()
emails=()
while IFS= read -r roster_line || [[ -n "${roster_line}" ]]; do
  email="${roster_line%%#*}"
  email="${email//[[:space:]]/}"
  email="${email,,}"
  [[ -z "${email}" ]] && continue
  if [[ ! "${email}" =~ ^[^[:space:]@]+@[^[:space:]@]+\.[^[:space:]@]+$ ]]; then
    echo "Invalid roster email: ${email}" >&2
    exit 2
  fi
  if [[ -z "${seen[${email}]:-}" ]]; then
    seen[${email}]=1
    emails+=("${email}")
  fi
done < "${ROSTER_FILE}"

if (( ${#emails[@]} == 0 )); then
  echo "The roster must contain at least one email address." >&2
  exit 2
fi

umask 077
temporary="$(mktemp "${CONFIG_ROOT}/allowed-emails.txt.XXXXXX")"
trap 'rm -f "${temporary}"' EXIT
printf '%s\n' "${emails[@]}" > "${temporary}"
chmod 0600 "${temporary}"
mv "${temporary}" "${ALLOWLIST_FILE}"
trap - EXIT

systemctl --user restart eduperf-backend.service
systemctl --user is-active --quiet eduperf-backend.service
echo "EduPerf roster updated: ${#emails[@]} allowed email address(es)."
