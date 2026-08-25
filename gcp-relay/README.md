# U.S.-hosted relay for SRL1

UM's network perimeter does not accept public inbound connections to SRL1.
This deployment gives the extension a stable public endpoint while preserving
SRL1 as the only measurement computer:

```text
student extension -- pinned TLS --> U.S. relay -- outbound SSH tunnel --> SRL1
```

The relay is an `e2-micro` VM with a regional Standard Tier address in Google
Cloud `us-central1`. It transports
encrypted TLS bytes and has no EduPerf code, workload, credential, student
answer, or measurement data. SRL1 initiates and maintains the tunnel, so no UM
inbound firewall exception is required. The VM accepts public TCP 8443 and
accepts SSH only from SRL1's public address. Its dedicated SSH key can create
only the one reverse listener and cannot open a shell.

## Deploy

Authenticate the instructor computer once, then run the deployment from this
repository:

```bash
gcloud auth login
./gcp-relay/deploy.sh \
  --project nsf-2006373-128866 \
  --region us-central1 \
  --zone us-central1-a \
  --srl-host SRL-1 \
  --extension-endpoint ../vscode-extension/resources/pilot-backend.json
```

The script reserves a stable U.S. public IPv4 address, creates the two narrow
firewall rules and relay VM, configures SRL1's persistent user service, rotates
the pinned TLS certificate to cover both the relay and SRL1 addresses, checks
the public health endpoint, and writes the extension's built-in endpoint.

After the endpoint file changes, package and publish/reinstall the extension.
Students do not import a connection file or configure the relay.

## Operational checks

On SRL1:

```bash
systemctl --user status eduperf-backend.service eduperf-us-relay.service
```

From a U.S. client outside UM:

```bash
curl --cacert tls-cert.pem https://RELAY_IP:8443/v1/health
```

The relay does not improve measurement capacity: SRL1 intentionally continues
to serialize runs on its pinned CPU. Stop the `eduperf-us-relay` VM when a pilot
is inactive to avoid cloud charges; retain the reserved address if the shipped
extension must keep the same endpoint.
