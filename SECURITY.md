# Security Policy

## Supported versions

Security fixes target the latest published release and the current `main` branch.

## Reporting a vulnerability

Do not open a public issue for a vulnerability, exposed credential, or sensitive user data. Use GitHub's private vulnerability reporting for this repository. If that is unavailable, email `minghorn.tmp@gmail.com` with a concise description, affected version, reproduction steps, and potential impact.

Do not include real API keys, private prompts, or user attachments in a report. You should receive an acknowledgement within seven days.

## Credential handling

AI Meeting Table stores credentials locally and sends them only to the selected provider for authentication. Repository changes must preserve log redaction, avoid response-body logging, and keep signing material and credentials outside version control.
