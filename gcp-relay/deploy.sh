#!/usr/bin/env bash
set -euo pipefail

readonly script_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project="${EDUPERF_GCP_PROJECT:-$(gcloud config get-value project 2>/dev/null || true)}"
region="${EDUPERF_GCP_REGION:-us-central1}"
zone="${EDUPERF_GCP_ZONE:-us-central1-a}"
srl_host="${EDUPERF_SRL_SSH_HOST:-SRL-1}"
origin_public_host="${EDUPERF_ORIGIN_PUBLIC_HOST:-141.215.12.243}"
srl_egress_ipv4="${EDUPERF_SRL_EGRESS_IPV4:-141.215.12.243}"
extension_endpoint="${EDUPERF_EXTENSION_ENDPOINT:-}"
readonly instance_name="eduperf-us-relay"
readonly address_name="eduperf-us-relay-ip"
readonly network_tag="eduperf-us-relay"
readonly network_name="eduperf-us-relay-net"

usage() {
  printf 'Usage: %s [--project ID] [--region US_REGION] [--zone US_ZONE] [--srl-host SSH_ALIAS] [--origin-public-host HOST] [--srl-egress-ipv4 IP] [--extension-endpoint PATH]\n' "$0"
}
while (( $# > 0 )); do
  case "$1" in
    --project) project="${2:-}"; shift 2 ;;
    --region) region="${2:-}"; shift 2 ;;
    --zone) zone="${2:-}"; shift 2 ;;
    --srl-host) srl_host="${2:-}"; shift 2 ;;
    --origin-public-host) origin_public_host="${2:-}"; shift 2 ;;
    --srl-egress-ipv4) srl_egress_ipv4="${2:-}"; shift 2 ;;
    --extension-endpoint) extension_endpoint="${2:-}"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
  esac
done
readonly subnet_name="eduperf-us-relay-${region}"

if [[ -z "${project}" ]] || [[ -z "${region}" ]] || [[ -z "${zone}" ]]; then
  echo "A Google Cloud project, U.S. region, and zone are required." >&2
  exit 2
fi
if [[ ! "${region}" =~ ^us- ]] || [[ ! "${zone}" =~ ^us- ]]; then
  echo "EduPerf policy requires a U.S. Google Cloud region and zone." >&2
  exit 2
fi
if [[ "${zone}" != "${region}-"* ]]; then
  echo "The zone must belong to the selected U.S. region." >&2
  exit 2
fi
if [[ ! "${srl_egress_ipv4}" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]]; then
  echo "SRL1's public egress address must be an IPv4 address." >&2
  exit 2
fi
for command_name in gcloud node scp ssh; do
  if ! command -v "${command_name}" >/dev/null 2>&1; then
    echo "Required command is missing: ${command_name}" >&2
    exit 1
  fi
done

active_account="$(gcloud auth list --filter=status:ACTIVE --format='value(account)' 2>/dev/null || true)"
if [[ -z "${active_account}" ]] \
    || ! gcloud projects describe "${project}" --format='value(projectId)' >/dev/null 2>&1; then
  echo "Google Cloud authentication is expired. Run: gcloud auth login" >&2
  exit 3
fi

ssh "${srl_host}" 'install -d -m 0700 "${XDG_CONFIG_HOME:-${HOME}/.config}/eduperf"; key="${XDG_CONFIG_HOME:-${HOME}/.config}/eduperf/relay_ed25519"; if [[ ! -s "${key}" ]]; then ssh-keygen -q -t ed25519 -N "" -C eduperf-srl1-us-relay -f "${key}"; fi'
tunnel_public_key="$(ssh "${srl_host}" 'key="${XDG_CONFIG_HOME:-${HOME}/.config}/eduperf/relay_ed25519.pub"; sed -n "1p" "${key}"')"
if [[ ! "${tunnel_public_key}" =~ ^ssh-ed25519[[:space:]] ]]; then
  echo "SRL1 did not provide a valid dedicated relay public key." >&2
  exit 1
fi

if ! gcloud compute addresses describe "${address_name}" --project "${project}" --region "${region}" >/dev/null 2>&1; then
  gcloud compute addresses create "${address_name}" --project "${project}" --region "${region}" --network-tier STANDARD
fi
relay_ip="$(gcloud compute addresses describe "${address_name}" --project "${project}" --region "${region}" --format='value(address)')"
if [[ ! "${relay_ip}" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]]; then
  echo "Google Cloud did not assign a relay IPv4 address." >&2
  exit 1
fi

if ! gcloud compute networks describe "${network_name}" --project "${project}" >/dev/null 2>&1; then
  gcloud compute networks create "${network_name}" --project "${project}" --subnet-mode custom
fi
if ! gcloud compute networks subnets describe "${subnet_name}" --project "${project}" --region "${region}" >/dev/null 2>&1; then
  gcloud compute networks subnets create "${subnet_name}" --project "${project}" \
    --network "${network_name}" --region "${region}" --range 10.73.0.0/28
fi

ensure_firewall() {
  local name="$1" allow="$2" sources="$3"
  if gcloud compute firewall-rules describe "${name}" --project "${project}" >/dev/null 2>&1; then
    gcloud compute firewall-rules update "${name}" --project "${project}" \
      --allow "${allow}" --source-ranges "${sources}" --target-tags "${network_tag}" >/dev/null
  else
    gcloud compute firewall-rules create "${name}" --project "${project}" \
      --direction INGRESS --action ALLOW --rules "${allow}" \
      --source-ranges "${sources}" --target-tags "${network_tag}" --network "${network_name}"
  fi
}
ensure_firewall eduperf-us-relay-https tcp:8443 0.0.0.0/0
ensure_firewall eduperf-us-relay-ssh tcp:22 "${srl_egress_ipv4}/32"

instance_exists=false
if gcloud compute instances describe "${instance_name}" --project "${project}" --zone "${zone}" >/dev/null 2>&1; then
  instance_exists=true
fi

metadata_key_file="$(mktemp)"
cleanup() { rm -f -- "${metadata_key_file}"; }
trap cleanup EXIT
printf '%s\n' "${tunnel_public_key}" > "${metadata_key_file}"

if [[ "${instance_exists}" == false ]]; then
  gcloud compute instances create "${instance_name}" \
    --project "${project}" --zone "${zone}" \
    --machine-type e2-micro \
    --network-interface "subnet=${subnet_name},network-tier=STANDARD,address=${relay_ip}" \
    --tags "${network_tag}" \
    --image-family ubuntu-2404-lts-amd64 --image-project ubuntu-os-cloud \
    --boot-disk-size 10GB --boot-disk-type pd-standard \
    --metadata block-project-ssh-keys=true \
    --metadata-from-file "startup-script=${script_root}/startup.sh,eduperf-tunnel-public-key=${metadata_key_file}" \
    --no-service-account --no-scopes \
    --shielded-secure-boot --shielded-vtpm --shielded-integrity-monitoring
else
  existing_ip="$(gcloud compute instances describe "${instance_name}" --project "${project}" --zone "${zone}" --format='value(networkInterfaces[0].accessConfigs[0].natIP)')"
  if [[ "${existing_ip}" != "${relay_ip}" ]]; then
    echo "Existing ${instance_name} does not use the reserved relay address ${relay_ip}." >&2
    exit 1
  fi
  gcloud compute instances add-metadata "${instance_name}" --project "${project}" --zone "${zone}" \
    --metadata block-project-ssh-keys=true \
    --metadata-from-file "startup-script=${script_root}/startup.sh,eduperf-tunnel-public-key=${metadata_key_file}"
  gcloud compute instances reset "${instance_name}" --project "${project}" --zone "${zone}"
fi

remote_script="/tmp/eduperf-configure-us-relay-$RANDOM.sh"
scp "${script_root}/configure-srl1.sh" "${srl_host}:${remote_script}"
ssh "${srl_host}" "status=0; bash '${remote_script}' '${relay_ip}' '${origin_public_host}' || status=\$?; rm -f -- '${remote_script}'; exit \${status}"

certificate_file="$(mktemp)"
trap 'rm -f -- "${metadata_key_file}" "${certificate_file}"' EXIT
remote_certificate="$(ssh "${srl_host}" 'printf "%s/eduperf/tls-cert.pem" "${XDG_STATE_HOME:-${HOME}/.local/state}"')"
scp "${srl_host}:${remote_certificate}" "${certificate_file}"

endpoint_output="${extension_endpoint:-${PWD}/eduperf-us-endpoint.json}"
node "${script_root}/write-endpoint.js" "${relay_ip}" "${certificate_file}" "${endpoint_output}"
curl --fail --silent --show-error --cacert "${certificate_file}" "https://${relay_ip}:8443/v1/health" >/dev/null

printf '\nEduPerf is publicly reachable through a U.S.-hosted relay.\n'
printf 'Endpoint: https://%s:8443\n' "${relay_ip}"
printf 'Extension configuration: %s\n' "${endpoint_output}"
