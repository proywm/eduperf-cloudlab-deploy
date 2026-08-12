#!/usr/bin/env bash
set -euo pipefail

readonly CONFIG_ROOT="${XDG_CONFIG_HOME:-${HOME}/.config}/eduperf"
readonly SETTINGS_FILE="${CONFIG_ROOT}/email.env"

if [[ ! -s "${CONFIG_ROOT}/allowed-emails.txt" ]]; then
  echo "Run hosted-node/install.sh before configuring email delivery." >&2
  exit 1
fi

sender="${EDUPERF_EMAIL_FROM:-}"
api_key="${EDUPERF_RESEND_API_KEY:-}"
if [[ -z "${sender}" ]]; then
  read -r -p "Verified sender (for example EduPerf <login@example.edu>): " sender
fi
if [[ -z "${api_key}" ]]; then
  read -r -s -p "Resend API key: " api_key
  printf '\n'
fi

if [[ ! "${sender}" =~ ^[A-Za-z0-9@._+\ \<\>-]{3,200}$ ]]; then
  echo "The sender contains unsupported characters." >&2
  exit 2
fi
if [[ ! "${sender}" =~ @ ]] || [[ ! "${api_key}" =~ ^re_[A-Za-z0-9_-]{16,}$ ]]; then
  echo "Enter a verified sender address and a valid Resend API key." >&2
  exit 2
fi

install -d -m 0700 "${CONFIG_ROOT}"
umask 077
temporary="$(mktemp "${CONFIG_ROOT}/email.env.XXXXXX")"
trap 'rm -f "${temporary}"' EXIT
printf 'EDUPERF_RESEND_API_KEY=%s\n' "${api_key}" > "${temporary}"
printf 'EDUPERF_EMAIL_FROM="%s"\n' "${sender}" >> "${temporary}"
mv "${temporary}" "${SETTINGS_FILE}"
trap - EXIT
chmod 0600 "${SETTINGS_FILE}"
systemctl --user restart eduperf-backend.service
systemctl --user is-active --quiet eduperf-backend.service
echo "EduPerf email delivery is configured and the backend has restarted."
