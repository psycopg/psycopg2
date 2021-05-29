#!/bin/bash

set -euo pipefail
set -x

dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
prjdir="$( cd "${dir}/../.." && pwd )"

# Find psycopg version
version=$(grep -e ^PSYCOPG_VERSION setup.py | sed "s/.*'\(.*\)'/\1/")
# A gratuitous comment to fix broken vim syntax file: '")
distdir="${prjdir}/dist/psycopg2-$version"

# Replace the package name
if [[ "${PACKAGE_NAME:-}" ]]; then
    sed -i "s/^setup(name=\"psycopg2\"/setup(name=\"${PACKAGE_NAME}\"/" \
        "${prjdir}/setup.py"
fi

# Build the source package
python setup.py sdist -d "$distdir"

# install and test
pip install "${distdir}"/*.tar.gz

python -c "import tests; tests.unittest.main(defaultTest='tests.test_suite')"
