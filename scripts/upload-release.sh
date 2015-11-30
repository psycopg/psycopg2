#!/bin/bash
# Script to create a psycopg release
#
# You must create a release tag before running the script, e.g. 2_5_4.
# The script will check out in a clear environment, build the sdist package,
# unpack and test it, then upload on PyPI and on the psycopg website.

set -e

REPO_URL=git@github.com:psycopg/psycopg2.git

VER=$(grep ^PSYCOPG_VERSION setup.py | cut -d "'" -f 2)

# avoid releasing a testing version
echo "$VER" | grep -qE '^[0-9]+\.[0-9]+(\.[0-9]+)?$' \
    || (echo "bad release: $VER" >&2 && exit 1)

# Check out in a clean environment
rm -rf rel
mkdir rel
cd rel
git clone $REPO_URL psycopg
cd psycopg
TAG=${VER//./_}
git checkout -b $TAG $TAG
make sdist

# Test the sdist just created
cd dist
tar xzvf psycopg2-$VER.tar.gz
cd psycopg2-$VER
make
make check
cd ../../

read -p "if you are not fine with the above stop me now..."

# upload to pypi and to the website

python setup.py sdist --formats=gztar upload -s

DASHVER=${VER//./-}
DASHVER=${DASHVER:0:3}

# Requires ssh configuration for 'psycoweb'
scp dist/psycopg2-${VER}.tar.gz psycoweb:tarballs/PSYCOPG-${DASHVER}/
ssh psycoweb ln -sfv PSYCOPG-${DASHVER}/psycopg2-${VER}.tar.gz \
    tarballs/psycopg2-latest.tar.gz

scp dist/psycopg2-${VER}.tar.gz.asc psycoweb:tarballs/PSYCOPG-${DASHVER}/
ssh psycoweb ln -sfv PSYCOPG-${DASHVER}/psycopg2-${VER}.tar.gz.asc \
    tarballs/psycopg2-latest.tar.gz.asc

echo "great, now write release notes and an email!"
