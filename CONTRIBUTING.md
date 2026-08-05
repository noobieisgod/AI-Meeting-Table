# Contributing

## Development workflow

1. Create a focused branch from `main`.
2. Keep shared behavior in `src/` and platform UI or integration code under `apps/windows/` or `apps/android/`.
3. Do not add generated builds, packages, credentials, signing files, or local databases.
4. Build the affected platform and run the complete host test suite.
5. Describe behavior changes, validation performed, and any compatibility impact in the pull request.

## Style

- Follow the existing C++ and Qt style.
- Prefer Qt or standard-library facilities over new dependencies.
- Keep platform conditionals at integration boundaries.
- Add the smallest regression test that demonstrates a non-trivial fix.

## Reporting problems

Use GitHub Issues for reproducible bugs and feature requests. Use the private process in [SECURITY.md](SECURITY.md) for vulnerabilities or exposed credentials.
