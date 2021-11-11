#!/bin/bash

# Create manylinux_2_24 wheels for psycopg2
#
# Look at the .github/workflows/packages.yml file for hints about how to use it.

set -euo pipefail
set -x

dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
prjdir="$( cd "${dir}/../.." && pwd )"

# Build all the available versions, or just the ones specified in PYVERS
if [ ! "${PYVERS:-}" ]; then
    PYVERS="$(ls /opt/python/)"
fi

# Find psycopg version
version=$(grep -e ^PSYCOPG_VERSION "${prjdir}/setup.py" | sed "s/.*'\(.*\)'/\1/")
# A gratuitous comment to fix broken vim syntax file: '")
distdir="${prjdir}/dist/psycopg2-$version"

# Replace the package name
if [[ "${PACKAGE_NAME:-}" ]]; then
    sed -i "s/^setup(name=\"psycopg2\"/setup(name=\"${PACKAGE_NAME}\"/" \
        "${prjdir}/setup.py"
fi

# Install prerequisite libraries
curl -k -s https://www.postgresql.org/media/keys/ACCC4CF8.asc | apt-key add -
echo "deb http://apt.postgresql.org/pub/repos/apt stretch-pgdg main" \
    > /etc/apt/sources.list.d/pgdg.list
apt-get -y update
apt-get install -y libpq-dev

# Create the wheel packages
for pyver in $PYVERS; do
    pybin="/opt/python/${pyver}/bin"
    "${pybin}/pip" wheel "${prjdir}" -w "${prjdir}/dist/"
done

# Bundle external shared libraries into the wheels
for whl in "${prjdir}"/dist/*.whl; do
    "${dir}/strip_wheel.sh" "$whl"
    auditwheel repair "$whl" -w "$distdir"
done

# Make sure the libpq is not in the system
for f in $(find /usr/lib /usr/lib64 -name libpq\*) ; do
    mkdir -pv "/libpqbak/$(dirname $f)"
    mv -v "$f" "/libpqbak/$(dirname $f)"
done

# Install packages and test
cd "${prjdir}"
for pyver in $PYVERS; do
    pybin="/opt/python/${pyver}/bin"
    "${pybin}/pip" install ${PACKAGE_NAME:-psycopg2} --no-index -f "$distdir"

    # Print psycopg and libpq versions
    "${pybin}/python" -c "import psycopg2; print(psycopg2.__version__)"
    "${pybin}/python" -c "import psycopg2; print(psycopg2.__libpq_version__)"
    "${pybin}/python" -c "import psycopg2; print(psycopg2.extensions.libpq_version())"

    # Fail if we are not using the expected libpq library
    if [[ "${WANT_LIBPQ:-}" ]]; then
        "${pybin}/python" -c "import psycopg2, sys; sys.exit(${WANT_LIBPQ} != psycopg2.extensions.libpq_version())"
    fi

    "${pybin}/python" -c "import tests; tests.unittest.main(defaultTest='tests.test_suite')"
done

# Restore the libpq packages
for f in $(cd /libpqbak/ && find . -not -type d); do
    mv -v "/libpqbak/$f" "/$f"
done
