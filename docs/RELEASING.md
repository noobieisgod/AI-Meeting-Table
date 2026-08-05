# Releasing

1. Choose a semantic version such as `1.1.0`.
2. Update the root CMake project version and Android version code. Packaging reads the same version value.
3. Build and test Windows and Android from clean directories.
4. Assemble a Windows Release archive and a manually signed Android App Bundle.
5. Inspect the Windows archive for Debug Runtime files, build metadata, credentials, databases, and signing material.
6. Generate SHA-256 checksums and include third-party notices.
7. Tag the verified commit as `v<version>` and publish platform-specific assets with release notes and supported-system requirements.

Release binaries and archives belong in GitHub Releases, not Git history.
