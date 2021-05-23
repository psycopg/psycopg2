#!/bin/bash

# Create macOS wheels for psycopg2
#
# Following instructions from https://github.com/MacPython/wiki/wiki/Spinning-wheels
# Cargoculting pieces of implementation from https://github.com/matthew-brett/multibuild

set -euo pipefail
set -x

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PRJDIR="$( cd "${DIR}/../.." && pwd )"

brew install gnu-sed postgresql@13

# Start the database for testing
brew services start postgresql

for i in $(seq 10 -1 0); do
  eval pg_isready && break
  if [ $i == 0 ]; then
      echo "PostgreSQL service not ready, giving up"
      exit 1
  fi
  echo "PostgreSQL service not ready, waiting a bit, attempts left: $i"
  sleep 5
done

# Find psycopg version
VERSION=$(grep -e ^PSYCOPG_VERSION "${PRJDIR}/setup.py" | gsed "s/.*'\(.*\)'/\1/")
# A gratuitous comment to fix broken vim syntax file: '")
DISTDIR="${PRJDIR}/dist/psycopg2-$VERSION"
mkdir -p "$DISTDIR"

# Install required python packages
pip install -U pip wheel delocate

# Allow to find the libraries needed.
export LDFLAGS="-L/usr/local/opt/openssl@1.1/lib"
export CPPFLAGS="-I/usr/local/opt/openssl@1.1/include"

# Replace the package name
gsed -i "s/^setup(name=\"psycopg2\"/setup(name=\"${PACKAGE_NAME}\"/" \
    "${PRJDIR}/setup.py"

# Build the wheels
WHEELDIR="${PRJDIR}/wheels"
pip wheel -w ${WHEELDIR} .
delocate-listdeps ${WHEELDIR}/*.whl

# Check where is the libpq. I'm gonna kill it for testing
if [[ -z "${LIBPQ:-}" ]]; then
    export LIBPQ=$(delocate-listdeps ${WHEELDIR}/*.whl | grep libpq)
fi

delocate-wheel ${WHEELDIR}/*.whl
# https://github.com/MacPython/wiki/wiki/Spinning-wheels#question-will-pip-give-me-a-broken-wheel
delocate-addplat --rm-orig -x 10_9 -x 10_10 ${WHEELDIR}/*.whl
cp ${WHEELDIR}/*.whl ${DISTDIR}

# kill the libpq to make sure tests don't depend on it
mv "$LIBPQ" "${LIBPQ}-bye"

# Install and test the built wheel
pip install ${PACKAGE_NAME} --no-index -f "$DISTDIR"

# Print psycopg and libpq versions
python -c "import psycopg2; print(psycopg2.__version__)"
python -c "import psycopg2; print(psycopg2.__libpq_version__)"
python -c "import psycopg2; print(psycopg2.extensions.libpq_version())"

# fail if we are not using the expected libpq library
# Disabled as we just use what's available on the system on macOS
# if [[ "${WANT_LIBPQ:-}" ]]; then
#     python -c "import psycopg2, sys; sys.exit(${WANT_LIBPQ} != psycopg2.extensions.libpq_version())"
# fi

python -c "import tests; tests.unittest.main(defaultTest='tests.test_suite')"

# just because I'm a boy scout
mv "${LIBPQ}-bye" "$LIBPQ"
