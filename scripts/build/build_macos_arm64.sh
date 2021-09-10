#!/bin/bash
#
# Script to generate arm64 wheels on an Apple Silicon host
#
# TODO: File issue on why cross-compiling is not working
set -euo pipefail

create-venv() {
    python3 -m venv "${1}"
    pip install -U pip wheel
}

tempdir=$(mktemp -d /tmp/psycopg2.$$)

echo "Everything will be created under ${tempdir}"

# create-venv "${tempdir}/venv_cibuildwheel" cibuildwheel
# source "${tempdir}/venv_cibuildwheel/bin/activate"
# pip install cibuildwheel
# export PACKAGE_NAME=psycopg2-binary
# # When you see "Repairing wheel" you will see this output when executing in an Apple Silicon
# # /opt/homebrew/Cellar/openssl@1.1/1.1.1l/lib/libcrypto.1.1.dylib
# # /opt/homebrew/Cellar/openssl@1.1/1.1.1l/lib/libssl.1.1.dylib
# # /opt/homebrew/Cellar/postgresql/13.4/lib/libpq.5.13.dylib
# # If you don't specify --output-dir you will be prompted for sudo
# CIBW_BUILD=cp38-macosx_arm64 cibuildwheel . --output-dir "${tempdir}/wheelhouse" --platform macos 2>&1
# # This package should be close to 2MB rather than 140KB
# du -h wheelhouse/*.whl
# deactivate

create-venv "${tempdir}/venv_delocate"
source "${tempdir}/venv_delocate/bin/activate"
pip install delocate
export PACKAGE_NAME=psycopg2-binary
# I'm making the steps close to what cibuildwheel does
pip wheel --use-feature=in-tree-build --wheel-dir "${tempdir}/built_wheel" --no-deps .
set -x
delocate-listdeps "${tempdir}/built_wheel/*.whl"
delocate-wheel --require-archs arm64 \
    -w "${tempdir}/repaired_wheel" \
    "${tempdir}/built_wheel/*.whl"
# This should be around 140KB
du -h "${tempdir}/built_wheel/*.whl" || exit 0
# This package should be close to 2MB rather than 140KB
du -h "${tempdir}/repaired_wheel/*"
deactivate

# Start the database for testing
# brew services start postgresql

# for i in $(seq 10 -1 0); do
#     eval pg_isready && break
#     if [ $i == 0 ]; then
#         echo "PostgreSQL service not ready, giving up"
#         exit 1
#     fi
#     echo "PostgreSQL service not ready, waiting a bit, attempts left: $i"
#     sleep 5
# done
# create-venv "${tempdir}/venv_test_package"
# source "${tempdir}/venv_test_package/bin/activate"
# export PSYCOPG2_TESTDB=postgres
# export PSYCOPG2_TEST_FAST=1
# # pip install psycopg2-binary --no-index -f "${tempdir}/wheelhouse"
# pip install psycopg2-binary --no-index -f wheelhouse
# python -c "import psycopg2; print(psycopg2.__version__)"
# python -c "import psycopg2; print(psycopg2.__libpq_version__)"
# python -c "import psycopg2; print(psycopg2.extensions.libpq_version())"
# python -c "import tests; tests.unittest.main(defaultTest='tests.test_suite')"
# deactivate
