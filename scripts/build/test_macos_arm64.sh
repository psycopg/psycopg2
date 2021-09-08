#!/bin/bash

# Test macos arm64 wheel packages from Github actions.
# It only makes sense to run this script from an Apple Silicon device
#
# From Github's Actions tab, choose the "Build packages" run and
# look for artifacts at the bottom. Download "packages_macos_arm64"
# and call this script with the path to it
set -euo pipefail
set -x

url=