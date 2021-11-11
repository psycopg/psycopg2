#!/bin/bash

# Strip symbols inplace from the libraries in a zip archive.
#
# Stripping symbols is beneficial (reduction of 30% of the final package, >
# %90% of the installed libraries. However just running `auditwheel repair
# --strip` breaks some of the libraries included from the system, which fail at
# import with errors such as "ELF load command address/offset not properly
# aligned".
#
# System libraries are already pretty stripped. _psycopg2.so goes around
# 1.6M -> 300K (python 3.8, x86_64)
#
# This script is designed to run on a wheel archive before auditwheel.

set -euo pipefail
set -x

wheel=$(realpath "$1")
shift

# python or python3?
if which python > /dev/null; then
    py=python
else
    py=python3
fi

tmpdir=$(mktemp -d)
trap "rm -r ${tmpdir}" EXIT

cd "${tmpdir}"
$py -m zipfile -e "${wheel}" .

find . -name *.so -ls -exec strip "$@" {} \;
# Display the size after strip
find . -name *.so -ls

$py -m zipfile -c "${wheel}" *

cd -
