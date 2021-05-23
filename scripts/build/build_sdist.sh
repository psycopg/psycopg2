#!/bin/bash

set -euo pipefail
set -x

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PRJDIR="$( cd "${DIR}/../.." && pwd )"

# Find psycopg version
export VERSION=$(grep -e ^PSYCOPG_VERSION setup.py | sed "s/.*'\(.*\)'/\1/")
# A gratuitous comment to fix broken vim syntax file: '")
export DISTDIR="${PRJDIR}/dist/psycopg2-$VERSION"

# Replace the package name
if [[ "${PACKAGE_NAME:-}" ]]; then
    sed -i "s/^setup(name=\"psycopg2\"/setup(name=\"${PACKAGE_NAME}\"/" \
        setup.py
fi

# Build the source package
python setup.py sdist -d "$DISTDIR"

# install and test
pip install "${DISTDIR}"/*.tar.gz

python -c "import tests; tests.unittest.main(defaultTest='tests.test_suite')"
