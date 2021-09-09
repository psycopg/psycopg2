#!/bin/bash

# Test macos arm64 wheel packages from Github actions.
# It only makes sense to run this script from an Apple Silicon device
#
# From Github's Actions tab, choose the "Build packages" run and
# look for artifacts at the bottom. Download "packages_macos_arm64"
# and call this script with the path to it
set -euo pipefail
set -x

tempdir=$(mktemp -d /tmp/psycopg2.$$)
venv="$tempdir/venv"
unzip "$1" -d "$tempdir"

# XXX: It would be ideal to run this script against various Python versions
python3 -m venv "$venv"
source "$venv/bin/activate"
pip install psycopg2-binary --no-index -f dist
python -c "import psycopg2; print(psycopg2.__version__)"
python -c "import psycopg2; print(psycopg2.__libpq_version__)"
python -c "import psycopg2; print(psycopg2.extensions.libpq_version())"
python -c "import tests; tests.unittest.main(defaultTest='tests.test_suite')"
