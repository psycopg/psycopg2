#!/bin/bash

# Create manylinux1 wheels for psycopg2
#
# Run this script with something like:
#
# docker run -t --rm -v `pwd`:/psycopg2 quay.io/pypa/manylinux1_x86_64 /psycopg2/scripts/build-manylinux.sh
# docker run -t --rm -v `pwd`:/psycopg2 quay.io/pypa/manylinux1_i686 linux32 /psycopg2/scripts/build-manylinux.sh
#
# (Note: -t is requrired for sudo)

set -e -x

# Install postgres packages for build and testing
# This doesn't work:
# rpm -Uvh "http://yum.postgresql.org/9.5/redhat/rhel-5-x86_64/pgdg-redhat95-9.5-3.noarch.rpm"
wget -O "/tmp/pgdg.rpm" "https://download.postgresql.org/pub/repos/yum/9.5/redhat/rhel-5-x86_64/pgdg-centos95-9.5-3.noarch.rpm"
rpm -Uv "/tmp/pgdg.rpm"
yum install -y postgresql95-devel postgresql95-server sudo

# Make pg_config available
export PGPATH=/usr/pgsql-9.5/bin/
export PATH="$PGPATH:$PATH"

# Find psycopg version
export VERSION=$(grep -e ^PSYCOPG_VERSION /psycopg2/setup.py | sed "s/.*'\(.*\)'/\1/")
export WHEELSDIR="/psycopg2/wheels/psycopg2-$VERSION"

# Create the wheel packages
for PYBIN in /opt/python/*/bin; do
    "${PYBIN}/pip" wheel /psycopg2/ -w "$WHEELSDIR"
done

# Bundle external shared libraries into the wheels
for WHL in "$WHEELSDIR"/*.whl; do
    auditwheel repair "$WHL" -w "$WHEELSDIR"
done

# Create a test cluster
/usr/bin/sudo -u postgres "$PGPATH/initdb" -D /var/lib/pgsql/9.5/data/
/usr/bin/sudo -u postgres "$PGPATH/pg_ctl" -D /var/lib/pgsql/9.5/data/ start
sleep 5     # wait server started
/usr/bin/sudo -u postgres "$PGPATH/createdb" psycopg2_test

export PSYCOPG2_TESTDB_USER=postgres

# Install packages and test
for PYBIN in /opt/python/*/bin; do
    "${PYBIN}/pip" install psycopg2 --no-index -f "$WHEELSDIR"
    "${PYBIN}/python" -c "from psycopg2 import tests; tests.unittest.main(defaultTest='tests.test_suite')"
done
