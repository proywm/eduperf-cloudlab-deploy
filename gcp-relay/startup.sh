#!/usr/bin/env bash
set -euo pipefail

# This VM transports TLS bytes only. The EduPerf API, workloads, credentials,
# and measurements remain on SRL1; the relay cannot decrypt student traffic.
readonly METADATA_ROOT="http://metadata.google.internal/computeMetadata/v1"
readonly METADATA_HEADER="Metadata-Flavor: Google"
readonly RELAY_USER="eduperf-tunnel"
readonly AUTHORIZED_KEYS="/home/${RELAY_USER}/.ssh/authorized_keys"

tunnel_public_key="$({
  curl --fail --silent --show-error \
    --header "${METADATA_HEADER}" \
    "${METADATA_ROOT}/instance/attributes/eduperf-tunnel-public-key"
} 2>/dev/null || true)"

if [[ ! "${tunnel_public_key}" =~ ^(ssh-ed25519|ecdsa-sha2-nistp256|ssh-rsa)[[:space:]][A-Za-z0-9+/=]+([[:space:]].*)?$ ]]; then
  echo "Missing or invalid eduperf-tunnel-public-key instance metadata." >&2
  exit 1
fi

if ! id "${RELAY_USER}" >/dev/null 2>&1; then
  useradd --create-home --shell /bin/bash "${RELAY_USER}"
fi
# Keep the account unlocked so OpenSSH permits public-key authentication, but
# remove its password. The Match block below accepts public keys only, and the
# key itself rejects every session command while allowing one reverse listener.
passwd --delete "${RELAY_USER}" >/dev/null
install -d -o "${RELAY_USER}" -g "${RELAY_USER}" -m 0700 "/home/${RELAY_USER}/.ssh"

key_options='command="/usr/sbin/nologin",restrict,port-forwarding,permitlisten="0.0.0.0:8443"'
temporary_keys="$(mktemp)"
trap 'rm -f -- "${temporary_keys}"' EXIT
printf '%s %s\n' "${key_options}" "${tunnel_public_key}" > "${temporary_keys}"
install -o "${RELAY_USER}" -g "${RELAY_USER}" -m 0600 "${temporary_keys}" "${AUTHORIZED_KEYS}"

temporary_sshd="$(mktemp)"
trap 'rm -f -- "${temporary_keys}" "${temporary_sshd}"' EXIT
{
  printf 'Match User %s\n' "${RELAY_USER}"
  printf '    PasswordAuthentication no\n'
  printf '    KbdInteractiveAuthentication no\n'
  printf '    AuthenticationMethods publickey\n'
  printf '    AllowTcpForwarding remote\n'
  printf '    GatewayPorts clientspecified\n'
  printf '    PermitListen 0.0.0.0:8443\n'
  printf '    PermitOpen none\n'
  printf '    X11Forwarding no\n'
  printf '    AllowAgentForwarding no\n'
  printf '    PermitTTY no\n'
  printf 'Match all\n'
} > "${temporary_sshd}"
install -o root -g root -m 0644 "${temporary_sshd}" /etc/ssh/sshd_config.d/90-eduperf-relay.conf

/usr/sbin/sshd -t
systemctl restart ssh.service 2>/dev/null || systemctl restart sshd.service

# Remove every service not needed by the byte-forwarding gateway from the
# public network surface. Google Cloud firewall rules provide the outer gate.
systemctl disable --now apache2.service nginx.service 2>/dev/null || true

echo "EduPerf U.S. relay is ready for SRL1's outbound tunnel."
