# HCC2D APT Repository

This directory contains the infrastructure for publishing a signed APT
repository for Debian and Ubuntu users.

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

Edit `apt-repo/repo.conf` if you want a different matrix.

## Repository layout

Published output is generated under:

```text
apt-repo/public/
  dists/<codename>/
  pool/<codename>/main/h/hcc2d-encoder/
```

This layout is suitable for static hosting over HTTPS.

## Prerequisites

- `gpg`
- `apt-ftparchive` from `apt-utils`
- built `.deb` artifacts from `dpkg-buildpackage`

## 1. Create or choose a signing key

Use a dedicated repository signing key.

Example:

```bash
gpg --full-generate-key
gpg --list-secret-keys --keyid-format=long
```

Then set the key ID or fingerprint:

```bash
export APT_REPO_GPG_KEY=YOURKEYID
```

You can also persist it in `apt-repo/repo.conf`.

## 2. Export the public key

```bash
apt-repo/scripts/export-public-key.sh
```

This writes:

```text
apt-repo/public/hcc2d-archive-keyring.gpg
```

Host that file alongside the repository so users can install it.

## 3. Publish a package

Example for Ubuntu 24.04 (`noble`):

```bash
apt-repo/scripts/publish.sh \
  --distribution noble \
  --deb ../hcc2d-encoder_0.9.0-1_amd64.deb
```

Example for Debian 12 (`bookworm`):

```bash
apt-repo/scripts/publish.sh \
  --distribution bookworm \
  --deb ../hcc2d-encoder_0.9.0-1_amd64.deb
```

For dry runs without signatures:

```bash
apt-repo/scripts/publish.sh \
  --distribution noble \
  --deb ../hcc2d-encoder_0.9.0-1_amd64.deb \
  --skip-signing
```

## 4. Validate metadata

```bash
apt-repo/scripts/check.sh --distribution noble
```

## 5. Host the repository

Serve `apt-repo/public/` over HTTPS on a stable URL, for example:

```text
https://packages.hcc2d.com/apt/
```

## User installation flow

If the repository is hosted at `https://packages.hcc2d.com/apt`, Debian/Ubuntu
users can install it with:

```bash
curl -fsSL https://packages.hcc2d.com/apt/hcc2d-archive-keyring.gpg \
  | sudo tee /usr/share/keyrings/hcc2d-archive-keyring.gpg >/dev/null

echo "deb [signed-by=/usr/share/keyrings/hcc2d-archive-keyring.gpg] https://packages.hcc2d.com/apt noble main" \
  | sudo tee /etc/apt/sources.list.d/hcc2d.list >/dev/null

sudo apt update
sudo apt install hcc2d-encoder
```

Replace `noble` with the user distribution codename as needed.

## Professional deployment recommendations

- Use a dedicated domain or subdomain such as `packages.hcc2d.com`
- Publish over HTTPS only
- Keep the signing key offline if possible; use a dedicated CI key only if
  you understand the risk
- Build separately for each target distribution
- Keep old package versions available for rollback
- Automate publish steps in CI after a tagged release

## Official Debian inclusion

This repository is for direct user distribution under your control.
It does not replace the separate Debian official inclusion workflow.
