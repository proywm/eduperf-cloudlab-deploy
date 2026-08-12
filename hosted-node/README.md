# Hosted-node deployment

This alternative deploys EduPerf under an existing Linux user account without
root access. The host must already provide Node.js 22+, a C++ compiler, Eigen,
OpenSSL, curl, tar, and a persistent systemd user manager.

```sh
git clone https://github.com/proywm/eduperf-cloudlab-deploy.git
cd eduperf-cloudlab-deploy
./hosted-node/install.sh 141.215.12.243 \
  probirr@umich.edu sjiao2@ncsu.edu jit623@lehigh.edu
```

The installer builds the fixed workload runtime, installs
`eduperf-backend.service` as a user service, and writes the exact email
allowlist to:

```text
~/.config/eduperf/allowed-emails.txt
```

The extension already knows the pilot endpoint and pinned HTTPS certificate.
The student selects **Sign in to Measurement Backend**, enters an allowlisted
email address, and enters the six-digit code received by email. The resulting
seven-day session is kept in VS Code SecretStorage. Students do not handle an
IP address, JSON connection file, password, token, terminal, or Output channel.

The first installation uses a protected on-node outbox so the authentication
flow can be tested before an email provider is connected. For real delivery,
verify a Resend sending domain and run the interactive setup once:

```sh
./hosted-node/configure-email.sh
```

The script asks for the verified sender and privately prompts for the API key;
nothing is placed on the command line or in JSON. Equivalently, an administrator
can write the following settings to `~/.config/eduperf/email.env`:

```text
EDUPERF_RESEND_API_KEY=re_replace_me
EDUPERF_EMAIL_FROM="EduPerf <login@your-verified-domain.example>"
```

Then restart the service:

```sh
systemctl --user restart eduperf-backend.service
```

The API key remains only in the node user's mode-0600 configuration file; it is
never included in the repository or extension. Resend delivery takes
precedence over the test outbox when both settings are present. The extension
checks the backend's public authentication status before offering sign-in, so
it reports a clear instructor-configuration message while real delivery is
disabled.

For a class, export one email address per line (blank lines and `#` comments are
ignored) and redeploy with a roster file:

```sh
./hosted-node/install.sh 141.215.12.243 --allowlist-file course-roster.txt
```

Removing an address from the allowlist immediately invalidates that person's
existing signed session. Codes expire after ten minutes, are single-use, allow
five guesses, and requests are rate-limited per address and source. Sessions
expire after seven days. The server stores no password and does not retain an
email-provider credential in its process source.

The source-address safety limit allows 500 code requests per hour so a class
sharing one campus NAT address can sign in together; the stricter one-minute
and five-per-hour limits still apply to each email address.

Measurements remain serialized on the pinned CPU. Each signed-in user may have
at most three jobs queued or running, while the shared waiting queue is capped
at 250 jobs. This prevents one participant from monopolizing the worker while
leaving room for a large class to submit together.

The runtime comparison works without HPCToolkit. To build the same pinned
profiling stack without root access, run:

```sh
./hosted-node/install-hpctoolkit.sh
./hosted-node/install.sh 141.215.12.243 --allowlist-file course-roster.txt
```

The first command can take substantial time. It records the resulting prefix
in `~/.config/eduperf/hpctoolkit-prefix`; rerunning the main installer attaches
that prefix to the service. An existing installation can instead be supplied
through `EDUPERF_HPCTOOLKIT_ROOT`.

The firewall must allow instructor and student clients to reach TCP port 8443.
The backend API exposes only fixed case IDs and allowlisted actions, serializes
measurements for repeatability, and prevents one signed-in user from reading
another user's job result.
