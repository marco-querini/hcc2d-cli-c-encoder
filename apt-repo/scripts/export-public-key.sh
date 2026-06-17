#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)
. "$REPO_ROOT/apt-repo/repo.conf"

if [ -z "${GNUPGHOME:-}" ] && [ -d "$REPO_ROOT/.secrets/apt-gpg" ]; then
    GNUPGHOME="$REPO_ROOT/.secrets/apt-gpg"
    export GNUPGHOME
fi

if [ -z "${HOME:-}" ] && [ -d "$REPO_ROOT/.secrets/apt-home" ]; then
    HOME="$REPO_ROOT/.secrets/apt-home"
    export HOME
fi

if [ -z "${APT_REPO_GPG_KEY:-}" ]; then
    echo "Error: set APT_REPO_GPG_KEY to the signing key fingerprint or long key ID." >&2
    exit 2
fi

mkdir -p "$(dirname "$REPO_ROOT/$APT_REPO_KEYRING_OUTPUT")"
gpg --batch --yes --export "$APT_REPO_GPG_KEY" > "$REPO_ROOT/$APT_REPO_KEYRING_OUTPUT"

echo "Wrote $APT_REPO_KEYRING_OUTPUT"
