#!/bin/bash

# Build a modern version of libpq and depending libs from source on Centos 5, Rocky, Alpine or macOS

set -euo pipefail

# Last release: https://www.postgresql.org/ftp/source/
# IMPORTANT! Change the cache key in packages.yml when upgrading libraries
postgres_version="${LIBPQ_VERSION}"

# last release: https://www.openssl.org/source/
openssl_version="${OPENSSL_VERSION}"

# Latest release: https://kerberos.org/dist/
krb5_version="1.21.3"

# Latest release: https://openldap.org/software/download/
ldap_version="2.6.9"

# Latest release: https://github.com/cyrusimap/cyrus-sasl/releases
sasl_version="2.1.28"

export LIBPQ_BUILD_PREFIX=${LIBPQ_BUILD_PREFIX:-/tmp/libpq.build}

case "$(uname)" in
    Darwin)
        ID=macos
        library_suffix=dylib
        ;;

    Linux)
        source /etc/os-release
        library_suffix=so
        ;;

    *)
        echo "$0: unexpected Operating system: '$(uname)'" >&2
        exit 1
        ;;
esac

# Install packages required for test and wheels build, regardless of whether
# we will build the libpq or not.
case "$ID" in
    alpine)
        apk add --no-cache krb5-libs
        ;;
esac

if [[ -f "${LIBPQ_BUILD_PREFIX}/lib/libpq.${library_suffix}" ]]; then
    echo "libpq already available: build skipped" >&2
    exit 0
fi

# Install packages required to build the libpq.
case "$ID" in
    centos|rocky)
        yum update -y
        yum install -y flex krb5-devel pam-devel perl perl-IPC-Cmd perl-Time-Piece zlib-devel
        ;;

    alpine)
        apk upgrade
        apk add --no-cache flex krb5-dev linux-pam-dev openldap-dev \
            openssl-dev zlib-dev
        ;;

    macos)
        brew install automake m4 libtool
        # If available, libpq seemingly insists on linking against homebrew's
        # openssl no matter what so remove it. Since homebrew's curl depends on
        # it, force use of system curl.
        brew uninstall --force --ignore-dependencies openssl gettext curl
        if [ -z "${MACOSX_ARCHITECTURE:-}" ]; then
            MACOSX_ARCHITECTURE="$(uname -m)"
        fi
        # Set the deployment target to be <= to that of the oldest supported Python version.
        # e.g. https://www.python.org/downloads/release/python-380/
        if [ "$MACOSX_ARCHITECTURE" == "x86_64" ]; then
            export MACOSX_DEPLOYMENT_TARGET=10.9
        else
            export MACOSX_DEPLOYMENT_TARGET=11.0
        fi
        ;;

    *)
        echo "$0: unexpected Linux distribution: '$ID'" >&2
        exit 1
        ;;
esac


if [ "$ID" == "macos" ]; then
    make_configure_standard_flags=( \
        --prefix=${LIBPQ_BUILD_PREFIX} \
        "CPPFLAGS=-I${LIBPQ_BUILD_PREFIX}/include/ -arch $MACOSX_ARCHITECTURE" \
        "LDFLAGS=-L${LIBPQ_BUILD_PREFIX}/lib -arch $MACOSX_ARCHITECTURE" \
    )
else
    make_configure_standard_flags=( \
        --prefix=${LIBPQ_BUILD_PREFIX} \
        CPPFLAGS=-I${LIBPQ_BUILD_PREFIX}/include/ \
        "LDFLAGS=-L${LIBPQ_BUILD_PREFIX}/lib -L${LIBPQ_BUILD_PREFIX}/lib64" \
    )
fi

if [ "$ID" == "centos" ] || [ "$ID" == "rocky" ] || [ "$ID" == "macos" ]; then
  if [[ ! -f "${LIBPQ_BUILD_PREFIX}/openssl.cnf" ]]; then

    # Build openssl if needed
    openssl_tag="openssl-${openssl_version}"
    openssl_dir="openssl-${openssl_tag}"
    if [ ! -d "${openssl_dir}" ]; then
        curl -fsSL \
            https://github.com/openssl/openssl/archive/${openssl_tag}.tar.gz \
            | tar xzf -

        pushd "${openssl_dir}"

        options=(--prefix=${LIBPQ_BUILD_PREFIX} --openssldir=${LIBPQ_BUILD_PREFIX} \
            zlib -fPIC shared)
        if [ -z "${MACOSX_ARCHITECTURE:-}" ]; then
            ./config $options
        else
            ./configure "darwin64-$MACOSX_ARCHITECTURE-cc" $options
        fi

        make -s depend
        make -s
    else
        pushd "${openssl_dir}"
    fi

    # Install openssl
    make install_sw
    popd

  fi
fi


if [ "$ID" == "macos" ]; then

    # Build kerberos if needed
    krb5_dir="krb5-${krb5_version}/src"
    if [ ! -d "${krb5_dir}" ]; then
        curl -fsSL "https://kerberos.org/dist/krb5/${krb5_version%.*}/krb5-${krb5_version}.tar.gz" \
            | tar xzf -

        pushd "${krb5_dir}"
        ./configure "${make_configure_standard_flags[@]}"
        make -s
    else
        pushd "${krb5_dir}"
    fi

    make install
    popd

fi


if [ "$ID" == "centos" ] || [ "$ID" == "rocky" ] || [ "$ID" == "macos" ]; then
  if [[ ! -f "${LIBPQ_BUILD_PREFIX}/lib/libsasl2.${library_suffix}" ]]; then

    # Build libsasl2 if needed
    # The system package (cyrus-sasl-devel) causes an amazing error on i686:
    # "unsupported version 0 of Verneed record"
    # https://github.com/pypa/manylinux/issues/376
    sasl_tag="cyrus-sasl-${sasl_version}"
    sasl_dir="cyrus-sasl-${sasl_tag}"
    if [ ! -d "${sasl_dir}" ]; then
        curl -fsSL \
            https://github.com/cyrusimap/cyrus-sasl/archive/${sasl_tag}.tar.gz \
            | tar xzf -

        pushd "${sasl_dir}"

        if [ "$ID" == "rocky" ]; then
            # Fix missing time.h include in multiple files for newer GCC versions
            sed -i.bak '/#include "saslint.h"/a\
#include <time.h>' lib/saslutil.c
            sed -i.bak '/#include "plugin_common.h"/a\
#include <time.h>' plugins/cram.c
        fi

        autoreconf -i
        ./configure "${make_configure_standard_flags[@]}" --disable-macos-framework
        make -s
    else
        pushd "${sasl_dir}"
    fi

    # Install libsasl2
    # requires missing nroff to build
    touch saslauthd/saslauthd.8
    make install
    popd

  fi
fi


if [ "$ID" == "centos" ] || [ "$ID" == "rocky" ] || [ "$ID" == "macos" ]; then
  if [[ ! -f "${LIBPQ_BUILD_PREFIX}/lib/libldap.${library_suffix}" ]]; then

    # Build openldap if needed
    ldap_tag="${ldap_version}"
    ldap_dir="openldap-${ldap_tag}"
    if [ ! -d "${ldap_dir}" ]; then
        curl -fsSL \
            https://www.openldap.org/software/download/OpenLDAP/openldap-release/openldap-${ldap_tag}.tgz \
            | tar xzf -

        pushd "${ldap_dir}"

        ./configure "${make_configure_standard_flags[@]}" --enable-backends=no --enable-null

        make -s depend
        make -s -C libraries/liblutil/
        make -s -C libraries/liblber/
        make -s -C libraries/libldap/
    else
        pushd "${ldap_dir}"
    fi

    # Install openldap
    make -C libraries/liblber/ install
    make -C libraries/libldap/ install
    make -C include/ install
    chmod +x ${LIBPQ_BUILD_PREFIX}/lib/{libldap,liblber}*.${library_suffix}*
    popd

  fi
fi


# Build libpq if needed
postgres_tag="REL_${postgres_version//./_}"
postgres_dir="postgres-${postgres_tag}"
if [ ! -d "${postgres_dir}" ]; then
    curl -fsSL \
        https://github.com/postgres/postgres/archive/${postgres_tag}.tar.gz \
        | tar xzf -

    pushd "${postgres_dir}"

    if [ "$ID" != "macos" ]; then
        # Match the default unix socket dir default with what defined on Ubuntu and
        # Red Hat, which seems the most common location
        sed -i 's|#define DEFAULT_PGSOCKET_DIR .*'\
'|#define DEFAULT_PGSOCKET_DIR "/var/run/postgresql"|' \
            src/include/pg_config_manual.h
    fi

    export LD_LIBRARY_PATH="${LIBPQ_BUILD_PREFIX}/lib:${LIBPQ_BUILD_PREFIX}/lib64"

    ./configure "${make_configure_standard_flags[@]}" --sysconfdir=/etc/postgresql-common \
        --with-gssapi --with-openssl --with-pam --with-ldap \
        --without-readline --without-icu
    make -s -C src/interfaces/libpq
    make -s -C src/bin/pg_config
    make -s -C src/include
else
    pushd "${postgres_dir}"
fi

# Install libpq
make -C src/interfaces/libpq install
make -C src/bin/pg_config install
make -C src/include install
popd

find ${LIBPQ_BUILD_PREFIX} -name \*.${library_suffix}.\* -type f -exec strip --strip-unneeded {} \;
