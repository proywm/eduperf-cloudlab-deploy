# EduPerf CloudLab Deployment

This public repository is the fixed-workload measurement backend for the
EduPerf VS Code extension. A CloudLab repository-based profile can clone it
anonymously, allocate one dedicated `m510`, and start the authenticated worker.

The repository intentionally contains no VS Code UI implementation, course
materials, student submissions, credentials, private results, or source-upload
endpoint. It contains only:

- `profile.py`: the one-node CloudLab profile;
- `cloudlab-backend/`: the authenticated, serialized API worker; and
- `workloads/`: the 100 allowlisted PerfBank adapters and the minimal runtime
  libraries required to build and execute them.

All 100 cases execute fresh behavioral and runtime comparisons. Five enhanced
cases also support live HPCToolkit event metrics and calling contexts. The API
accepts a shipped case ID and one of four fixed actions; it cannot execute an
uploaded program or arbitrary command.

## Create the CloudLab profile

1. Create a repository-based profile in CloudLab.
2. Use `https://github.com/proywm/eduperf-cloudlab-deploy.git` as the repository
   URL and select the top-level `profile.py`.
3. Instantiate it on the default `m510` node type.
4. Wait for the experiment to become **Ready**.
5. As the instructor, download `/local/eduperf/connection.json` from the node
   and import it with **EduPerf: Import Custom Backend Connection (Advanced)**
   in VS Code.

The initial Ubuntu launch installs a pinned HPCToolkit stack and can be slow.
Capture that configured node as a CloudLab project image, then use its URN as
the profile's `disk_image` value for routine classroom launches. Every boot
generates a fresh 256-bit bearer token and TLS certificate, even when the disk
image is reused.

See [cloudlab-backend/README.md](cloudlab-backend/README.md) for the service
protocol, isolation model, and operational details.

For a persistent machine that cannot use the root-level CloudLab bootstrap,
see [hosted-node/README.md](hosted-node/README.md). The hosted installer runs as
an ordinary user and provides the extension's allowlisted, passwordless email
sign-in service. The pilot endpoint is built into the VS Code extension, so
participants handle neither an IP address nor a JSON credential file.

## Local validation

On Ubuntu with a C++ compiler and Eigen installed:

```sh
npm run test:backend
npm run build:workloads
npm run test:workloads
npm run test:integration
npm run test:enhanced
bash -n cloudlab-backend/bootstrap.sh cloudlab-backend/install-hpctoolkit.sh \
  hosted-node/install.sh hosted-node/install-hpctoolkit.sh \
  hosted-node/configure-email.sh hosted-node/update-roster.sh
python3 -m py_compile profile.py
```

GitHub Actions rebuilds the pinned runtime and runs the same backend, 100-case,
and enhanced-harness checks on every push and pull request.
