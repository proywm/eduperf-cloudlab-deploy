# Security

Please report vulnerabilities privately through GitHub's security advisory
interface for this repository. Do not open a public issue containing a token,
certificate, CloudLab hostname, or connection file.

The service deliberately exposes no source-upload or general command endpoint.
Credentials and self-signed TLS material are generated under `/local/eduperf`
on each experiment boot and must never be committed.
