#!/bin/bash

# Build psycopg2-binary wheel packages for Apple M1 (cpNNN-macosx_arm64)
#
# This script is designed to run on a local machine: it will clone the repos
# remotely and execute the `build_macos_arm64.sh` script remotely, then will
# download the built packages. A tag to build must be specified.
#
# In order to run the script, the `m1` host must be specified in
# `~/.ssh/config`; for instance:
#
#   Host m1
#     User m1
#     HostName 1.2.3.4

set -euo pipefail
# set -x

tag=${1:-}

if [[ ! "${tag}" ]]; then
    echo "Usage: $0 TAG" >&2
    exit 2
fi

rdir=psycobuild

# Clone the repos
ssh m1 rm -rf "${rdir}"
ssh m1 git clone https://github.com/psycopg/psycopg2.git --branch ${tag} "${rdir}"

# Allow sudoing without password, to allow brew to install
ssh -t m1 bash -c \
    'test -f /etc/sudoers.d/m1 || echo "m1 ALL=(ALL) NOPASSWD:ALL" | sudo tee /etc/sudoers.d/m1'

# Build the wheel packages
ssh m1 "${rdir}/scripts/build/build_macos_arm64.sh"

# Transfer the packages locally
scp -r "m1:${rdir}/wheelhouse" .
