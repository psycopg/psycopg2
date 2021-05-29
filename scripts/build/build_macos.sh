#!/bin/bash

# Create macOS wheels for psycopg2
#
# Following instructions from https://github.com/MacPython/wiki/wiki/Spinning-wheels
# Cargoculting pieces of implementation from https://github.com/matthew-brett/multibuild

set -euo pipefail
set -x

dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
prjdir="$( cd "${dir}/../.." && pwd )"

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
version=$(grep -e ^PSYCOPG_VERSION "${prjdir}/setup.py" | gsed "s/.*'\(.*\)'/\1/")
# A gratuitous comment to fix broken vim syntax file: '")
distdir="${prjdir}/dist/psycopg2-$version"
mkdir -p "$distdir"

# Install required python packages
pip install -U pip wheel delocate

# Replace the package name
if [[ "${PACKAGE_NAME:-}" ]]; then
    gsed -i "s/^setup(name=\"psycopg2\"/setup(name=\"${PACKAGE_NAME}\"/" \
        "${prjdir}/setup.py"
fi

# Build the wheels
wheeldir="${prjdir}/wheels"
pip wheel -w ${wheeldir} .
delocate-listdeps ${wheeldir}/*.whl

# Check where is the libpq. I'm gonna kill it for testing
if [[ -z "${LIBPQ:-}" ]]; then
    export LIBPQ=$(delocate-listdeps ${wheeldir}/*.whl | grep libpq)
fi

delocate-wheel ${wheeldir}/*.whl
# https://github.com/MacPython/wiki/wiki/Spinning-wheels#question-will-pip-give-me-a-broken-wheel
delocate-addplat --rm-orig -x 10_9 -x 10_10 ${wheeldir}/*.whl
cp ${wheeldir}/*.whl ${distdir}

# kill the libpq to make sure tests don't depend on it
mv "$LIBPQ" "${LIBPQ}-bye"

# Install and test the built wheel
pip install ${PACKAGE_NAME:-psycopg2} --no-index -f "$distdir"

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
