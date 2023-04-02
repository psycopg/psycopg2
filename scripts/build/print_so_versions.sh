#!/bin/bash

# Take a .so file as input and print the Debian packages and versions of the
# libraries it links.

set -euo pipefail
# set -x

source /etc/os-release

sofile="$1"

case "$ID" in
    alpine)
        depfiles=$( (ldd "$sofile" 2>/dev/null || true) | grep '=>' | sed 's/.*=> \(.*\) (.*)/\1/')
        (for depfile in $depfiles; do
             echo "$(basename "$depfile") => $(apk info --who-owns "${depfile}" | awk '{print $(NF)}')"
        done) | sort | uniq
        ;;

    debian)
        depfiles=$(ldd "$sofile" | grep '=>' | sed 's/.*=> \(.*\) (.*)/\1/')
        (for depfile in $depfiles; do
            pkgname=$(dpkg -S "${depfile}" | sed 's/\(\): .*/\1/')
            dpkg -l "${pkgname}" | grep '^ii' | awk '{print $2 " => " $3}'
        done) | sort | uniq
        ;;

    centos)
        echo "TODO!"
        ;;

    *)
        echo "$0: unexpected Linux distribution: '$ID'" >&2
        exit 1
        ;;
esac
