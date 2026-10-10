# HCC2D Encoder APT Repository

This directory contains the infrastructure for publishing the
`hcc2d-encoder` Debian package in a signed APT repository for Debian and
Ubuntu users.

It is separate from the Debian packaging in `debian/`:

- `debian/` builds the package
- `apt-repo/` publishes the package for `apt update`, `apt install`,
  `apt upgrade`, and `apt remove`

## Supported distributions

The default configuration includes:

- Debian `bookworm`
- Debian `trixie`
- Ubuntu `jammy`
- Ubuntu `noble`

Edit `apt-repo/repo.conf` or override its environment variables to change the
distribution and architecture matrix. One repository component is supported;
it is `main` by default.

## Repository layout

Published output is generated under:

```text
apt-repo/public/
  dists/<codename>/
  pool/<codename>/main/h/hcc2d-encoder/
```

This layout is suitable for static hosting over HTTPS.

## Signing key

The Encoder and Streamer use the same public HCC2D archive key:

```text
4DCDC5549D1955166EE3E83A567381E2B0354085
```

Only the public key `apt-repo/public/hcc2d-archive-keyring.gpg` is versioned.
Private GnuPG material must remain outside the repository. When it is stored
in a non-default keyring, provide its location only for the publishing command:

```bash
GNUPGHOME=/path/to/private/apt-gpg \
  apt-repo/scripts/publish.sh --distribution noble \
  --deb ../hcc2d-encoder_0.9.0-2_amd64.deb
```

The publishing script reads private-key material only through GnuPG and never
copies it into the repository.

## Prerequisites

- `gpg` and `gpgv`
- `apt-ftparchive` from `apt-utils`
- `dpkg-deb`, `gzip`, and `sha256sum`
- a `.deb` built with `dpkg-buildpackage`

## Build the package

From the repository root:

```bash
dpkg-buildpackage -us -uc -b
```

## Publish metadata

Publish for Ubuntu 24.04 (`noble`):

```bash
apt-repo/scripts/publish.sh \
  --distribution noble \
  --deb ../hcc2d-encoder_0.9.0-2_amd64.deb
```

Generate unsigned metadata for a dry run:

```bash
apt-repo/scripts/publish.sh \
  --distribution noble \
  --deb ../hcc2d-encoder_0.9.0-2_amd64.deb \
  --skip-signing
```

Validate the generated repository:

```bash
apt-repo/scripts/check.sh --distribution noble
```

The publication script validates every package, preserves prior revisions,
prepares package indexes and signatures in a staging directory, and only then
installs the completed metadata. It refuses to replace an existing package
filename with different content. The check script verifies compressed indexes,
referenced package sizes and SHA-256 hashes, Release checksums, and signatures.

Generated pools, distribution metadata, and temporary files are ignored by
Git. Deploy them separately to the HTTPS package host.

## Official repository and installation

The official HCC2D Encoder repository is hosted at:

```text
https://hcc2d.com/apt-encoder/
```

Debian/Ubuntu users can install it with:

```bash
curl -fsSL https://hcc2d.com/apt-encoder/hcc2d-archive-keyring.gpg \
  | sudo tee /usr/share/keyrings/hcc2d-archive-keyring.gpg >/dev/null

echo "deb [arch=amd64 signed-by=/usr/share/keyrings/hcc2d-archive-keyring.gpg] https://hcc2d.com/apt-encoder noble main" \
  | sudo tee /etc/apt/sources.list.d/hcc2d-encoder.list >/dev/null

sudo apt update
sudo apt install hcc2d-encoder
```

Replace `noble` with the user distribution codename as needed.

## Export the public key

If the public key needs to be regenerated from the signing keyring:

```bash
GNUPGHOME=/path/to/private/apt-gpg \
  apt-repo/scripts/export-public-key.sh
```

Verify the exported fingerprint before committing it:

```bash
gpg --show-keys --with-fingerprint \
  apt-repo/public/hcc2d-archive-keyring.gpg
```

Never add a private key, `private-keys-v1.d`, revocation certificate, or a
complete private GnuPG home to this public repository.

## Professional deployment recommendations

- Use a stable HTTPS URL for the repository
- Publish over HTTPS only
- Keep the signing key offline if possible; use a dedicated CI key only if
  you understand the risk
- Build separately for each target distribution
- Keep old package versions available for rollback
- Automate publish steps in CI after a tagged release

## Official Debian inclusion

This repository is for direct user distribution under your control.
It does not replace the separate Debian official inclusion workflow.
