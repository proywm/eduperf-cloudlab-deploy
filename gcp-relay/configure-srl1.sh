#!/usr/bin/env bash
set -euo pipefail

relay_host="${1:-}"
origin_public_host="${2:-141.215.12.243}"
readonly relay_host origin_public_host
readonly config_root="${XDG_CONFIG_HOME:-${HOME}/.config}/eduperf"
readonly state_root="${XDG_STATE_HOME:-${HOME}/.local/state}/eduperf"
readonly service_root="${XDG_CONFIG_HOME:-${HOME}/.config}/systemd/user"
readonly relay_key="${config_root}/relay_ed25519"
readonly known_hosts="${config_root}/relay_known_hosts"
readonly relay_service="${service_root}/eduperf-us-relay.service"

if [[ -z "${relay_host}" ]] || [[ ! "${relay_host}" =~ ^[A-Za-z0-9.-]+$ ]]; then
  echo "Usage: $0 RELAY_IP_OR_DNS_NAME [ORIGIN_PUBLIC_IP_OR_DNS_NAME]" >&2
  exit 2
fi
if [[ -z "${origin_public_host}" ]] || [[ ! "${origin_public_host}" =~ ^[A-Za-z0-9.:-]+$ ]]; then
  echo "The origin public host is invalid." >&2
  exit 2
fi
for command_name in curl openssl ssh ssh-keygen ssh-keyscan systemctl; do
  if ! command -v "${command_name}" >/dev/null 2>&1; then
    echo "Required command is missing: ${command_name}" >&2
    exit 1
  fi
done
if [[ ! -s "${state_root}/tls-key.pem" ]] || [[ ! -s "${state_root}/tls-cert.pem" ]]; then
  echo "Install the EduPerf hosted backend before configuring its relay." >&2
  exit 1
fi

install -d -m 0700 "${config_root}"
install -d -m 0755 "${service_root}"
if [[ ! -s "${relay_key}" ]]; then
  ssh-keygen -q -t ed25519 -N '' -C 'eduperf-srl1-us-relay' -f "${relay_key}"
fi
chmod 0600 "${relay_key}"
chmod 0644 "${relay_key}.pub"

temporary_known_hosts="$(mktemp)"
temporary_key="$(mktemp)"
temporary_cert="$(mktemp)"
temporary_service="$(mktemp)"
cleanup() {
  rm -f -- "${temporary_known_hosts}" "${temporary_key}" "${temporary_cert}" "${temporary_service}"
}
trap cleanup EXIT

for attempt in {1..30}; do
  if ssh-keyscan -T 3 -H "${relay_host}" > "${temporary_known_hosts}" 2>/dev/null \
      && [[ -s "${temporary_known_hosts}" ]]; then
    break
  fi
  if (( attempt == 30 )); then
    echo "The U.S. relay did not make SSH available within 90 seconds." >&2
    exit 1
  fi
  sleep 3
done
install -m 0600 "${temporary_known_hosts}" "${known_hosts}"

host_san() {
  if [[ "$1" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]] || [[ "$1" == *:* ]]; then
    printf 'IP:%s' "$1"
  else
    printf 'DNS:%s' "$1"
  fi
}

certificate_matches() {
  local host="$1"
  if [[ "$host" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]] || [[ "$host" == *:* ]]; then
    openssl x509 -in "${state_root}/tls-cert.pem" -checkip "$host" -noout >/dev/null 2>&1
  else
    openssl x509 -in "${state_root}/tls-cert.pem" -checkhost "$host" -noout >/dev/null 2>&1
  fi
}

if ! openssl x509 -in "${state_root}/tls-cert.pem" -checkend 2592000 -noout >/dev/null 2>&1 \
    || ! certificate_matches "${relay_host}" \
    || ! certificate_matches "${origin_public_host}"; then
  openssl req -x509 -newkey rsa:3072 -sha256 -nodes -days 365 \
    -keyout "${temporary_key}" \
    -out "${temporary_cert}" \
    -subj "/CN=${relay_host}" \
    -addext "subjectAltName=$(host_san "${relay_host}"),$(host_san "${origin_public_host}")" \
    >/dev/null 2>&1
  install -m 0600 "${temporary_key}" "${state_root}/tls-key.pem"
  install -m 0644 "${temporary_cert}" "${state_root}/tls-cert.pem"
fi

{
  printf '[Unit]\n'
  printf 'Description=EduPerf outbound tunnel to the U.S. classroom relay\n'
  printf 'After=network-online.target eduperf-backend.service\n'
  printf 'Wants=network-online.target\n'
  printf 'Requires=eduperf-backend.service\n\n'
  printf '[Service]\n'
  printf 'Type=simple\n'
  printf 'ExecStart=/usr/bin/ssh -NT -i %s -o IdentitiesOnly=yes -o BatchMode=yes -o ExitOnForwardFailure=yes -o ServerAliveInterval=20 -o ServerAliveCountMax=3 -o StrictHostKeyChecking=yes -o UserKnownHostsFile=%s -R 0.0.0.0:8443:127.0.0.1:8443 eduperf-tunnel@%s\n' \
    "${relay_key}" "${known_hosts}" "${relay_host}"
  printf 'Restart=always\n'
  printf 'RestartSec=3\n'
  printf 'NoNewPrivileges=true\n'
  printf 'PrivateTmp=true\n\n'
  printf '[Install]\n'
  printf 'WantedBy=default.target\n'
} > "${temporary_service}"
install -m 0644 "${temporary_service}" "${relay_service}"

systemctl --user daemon-reload
systemctl --user restart eduperf-backend.service
systemctl --user enable --now eduperf-us-relay.service

for attempt in {1..30}; do
  if curl --fail --silent --connect-timeout 3 --noproxy '*' \
      --max-time 5 \
      --cacert "${state_root}/tls-cert.pem" \
      "https://${relay_host}:8443/v1/health" >/dev/null; then
    echo "EduPerf is reachable through the U.S. relay at https://${relay_host}:8443"
    exit 0
  fi
  if (( attempt == 30 )); then
    systemctl --user status eduperf-us-relay.service --no-pager >&2 || true
    exit 1
  fi
  sleep 2
done
