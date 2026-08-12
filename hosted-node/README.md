# Hosted-node deployment

This alternative deploys EduPerf under an existing Linux user account without
root access. The host must already provide Node.js 22+, a C++ compiler, Eigen,
OpenSSL, curl, tar, and a persistent systemd user manager.

```sh
git clone https://github.com/proywm/eduperf-cloudlab-deploy.git
cd eduperf-cloudlab-deploy
./hosted-node/install.sh 141.215.12.243
```

The installer builds the fixed workload runtime, rotates the bearer token and
TLS key, installs `eduperf-backend.service` as a user service, and creates:

```text
~/.local/state/eduperf/connection.json
```

The instructor imports that file into the VS Code extension and distributes it
only to the intended class. Students do not discover or configure the IP
directly; the connection file supplies the URL, token, certificate, and label.

The runtime comparison works without HPCToolkit. To build the same pinned
profiling stack without root access, run:

```sh
./hosted-node/install-hpctoolkit.sh
./hosted-node/install.sh 141.215.12.243
```

The first command can take substantial time. It records the resulting prefix
in `~/.config/eduperf/hpctoolkit-prefix`; rerunning the main installer attaches
that prefix to the service. An existing installation can instead be supplied
through `EDUPERF_HPCTOOLKIT_ROOT`.

The firewall must allow instructor and student clients to reach TCP port 8443.
Treat `connection.json` as a password: the backend API exposes only fixed case
IDs and actions, but anyone possessing the file can submit measurement jobs.
