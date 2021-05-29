#!/bin/bash

# Build a modern version of libpq and depending libs from source on Centos 5

set -euo pipefail
set -x

openssl_version="1.1.1k"
ldap_version="2.4.59"
sasl_version="2.1.27"
postgres_version="13.3"

yum install -y zlib-devel krb5-devel pam-devel


# Build openssl if needed
openssl_tag="OpenSSL_${openssl_version//./_}"
openssl_dir="openssl-${openssl_tag}"
if [ ! -d "${openssl_dir}" ]; then curl -sL \
        https://github.com/openssl/openssl/archive/${openssl_tag}.tar.gz \
        | tar xzf -

    cd "${openssl_dir}"

    ./config --prefix=/usr/local/ --openssldir=/usr/local/ \
        zlib -fPIC shared
    make depend
    make
else
    cd "${openssl_dir}"
fi

# Install openssl
make install_sw
cd ..


# Build libsasl2 if needed
# The system package (cyrus-sasl-devel) causes an amazing error on i686:
# "unsupported version 0 of Verneed record"
# https://github.com/pypa/manylinux/issues/376
sasl_tag="cyrus-sasl-${sasl_version}"
sasl_dir="cyrus-sasl-${sasl_tag}"
if [ ! -d "${sasl_dir}" ]; then
    curl -sL \
        https://github.com/cyrusimap/cyrus-sasl/archive/${sasl_tag}.tar.gz \
        | tar xzf -

    cd "${sasl_dir}"

    autoreconf -i
    ./configure
    make
else
    cd "${sasl_dir}"
fi

# Install libsasl2
# requires missing nroff to build
touch saslauthd/saslauthd.8
make install
cd ..


# Build openldap if needed
ldap_tag="${ldap_version}"
ldap_dir="openldap-${ldap_tag}"
if [ ! -d "${ldap_dir}" ]; then
    curl -sL \
        https://www.openldap.org/software/download/OpenLDAP/openldap-release/openldap-${ldap_tag}.tgz \
        | tar xzf -

    cd "${ldap_dir}"

    ./configure --enable-backends=no --enable-null
    make depend
    make -C libraries/liblutil/
    make -C libraries/liblber/
    make -C libraries/libldap/
    make -C libraries/libldap_r/
else
    cd "${ldap_dir}"
fi

# Install openldap
make -C libraries/liblber/ install
make -C libraries/libldap/ install
make -C libraries/libldap_r/ install
make -C include/ install
chmod +x /usr/local/lib/{libldap,liblber}*.so*
cd ..


# Build libpq if needed
postgres_tag="REL_${postgres_version//./_}"
postgres_dir="postgres-${postgres_tag}"
if [ ! -d "${postgres_dir}" ]; then
    curl -sL \
        https://github.com/postgres/postgres/archive/${postgres_tag}.tar.gz \
        | tar xzf -

    cd "${postgres_dir}"

    # Match the default unix socket dir default with what defined on Ubuntu and
    # Red Hat, which seems the most common location
    sed -i 's|#define DEFAULT_PGSOCKET_DIR .*'\
'|#define DEFAULT_PGSOCKET_DIR "/var/run/postgresql"|' \
        src/include/pg_config_manual.h

    # Without this, libpq ./configure fails on i686
    if [[ "$(uname -m)" == "i686" ]]; then
        export LD_LIBRARY_PATH=/usr/local/lib
    fi

    ./configure --prefix=/usr/local --without-readline \
        --with-gssapi --with-openssl --with-pam --with-ldap
    make -C src/interfaces/libpq
    make -C src/bin/pg_config
    make -C src/include
else
    cd "${postgres_dir}"
fi

# Install libpq
make -C src/interfaces/libpq install
make -C src/bin/pg_config install
make -C src/include install
cd ..

find /usr/local/ -name \*.so.\* -type f -exec strip --strip-unneeded {} \;
