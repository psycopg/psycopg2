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
# set -x

source /etc/os-release
dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

wheel=$(realpath "$1")
shift

tmpdir=$(mktemp -d)
trap "rm -r ${tmpdir}" EXIT

cd "${tmpdir}"
python -m zipfile -e "${wheel}" .

echo "
Libs before:"
# Busybox doesn't have "find -ls"
find . -name \*.so | xargs ls -l

# On Debian, print the package versions libraries come from
echo "
Dependencies versions of '_psycopg.so' library:"
"${dir}/print_so_versions.sh" "$(find . -name \*_psycopg\*.so)"

find . -name \*.so -exec strip "$@" {} \;

echo "
Libs after:"
find . -name \*.so | xargs ls -l

python -m zipfile -c ${wheel} *

cd -
