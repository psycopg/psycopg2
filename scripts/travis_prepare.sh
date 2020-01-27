#!/bin/bash

set -e -x

# Prepare the test databases in Travis CI.
#
# The script should be run with sudo.
# The script is not idempotent: it assumes the machine in a clean state
# and is designed for a sudo-enabled Trusty environment.
#
# The variables TEST_PAST, TEST_FUTURE, DONT_TEST_PRESENT can be used to test
# against unsupported Postgres versions and skip tests with supported ones.
#
# The variables can be set in the travis configuration
# (https://travis-ci.org/psycopg/psycopg2/settings)

set_param () {
    # Set a parameter in a postgresql.conf file
    param=$1
    value=$2

    sed -i "s/^\s*#\?\s*$param.*/$param = $value/" "$DATADIR/postgresql.conf"
}

create () {
    export VERSION=$1
    export PACKAGE=${2:-$VERSION}

    # Version as number: 9.6 -> 906; 11 -> 1100
    export VERNUM=$(echo $VERSION \
        | sed 's/\([0-9]\+\)\(\.\([0-9]\+\)\)\?/100 * \1 + 0\3/' | bc)

    # Port number: 9.6 -> 50906
    export PORT=$(( 50000 + $VERNUM ))

    export DATADIR="/var/lib/postgresql/$PACKAGE/psycopg"
    export PGDIR="/usr/lib/postgresql/$PACKAGE"
    export PGBIN="$PGDIR/bin"

    # install postgres versions not available on the image
    if [[ ! -d "${PGDIR}" ]]; then
        if (( "$VERNUM" >= 904 )); then
            # A versiou supported by postgres
            if [[ ! "${apt_updated:-}" ]]; then
                apt_updated="yeah"
                sudo apt-get update -y
            fi
            sudo apt-get install -y \
                postgresql-server-dev-${VERSION} postgresql-${VERSION}
        else
            # A dinosaur
            wget -O - \
                https://upload.psycopg.org/postgresql/postgresql-${PACKAGE}-$(lsb_release -cs).tar.bz2 \
                | sudo tar xjf - -C /usr/lib/postgresql
        fi
    fi

    sudo -u postgres "$PGBIN/initdb" -D "$DATADIR"

    set_param port "$PORT"
    if (( "$VERNUM" >= 800 )); then
        set_param listen_addresses "'*'"
    else
        set_param tcpip_socket true
    fi

    # for two-phase commit testing
    if (( "$VERNUM" >= 801 )); then set_param max_prepared_transactions 10; fi

    # for replication testing
    if (( "$VERNUM" >= 900 )); then set_param max_wal_senders 5; fi
    if (( "$VERNUM" >= 904 )); then set_param max_replication_slots 5; fi

    if (( "$VERNUM" >= 904 )); then
        set_param wal_level logical
    elif (( "$VERNUM" >= 900 )); then
        set_param wal_level hot_standby
    fi

    if (( "$VERNUM" >= 900 )); then
        echo "host replication travis 0.0.0.0/0 trust" >> "$DATADIR/pg_hba.conf"
    fi

    # start the server, wait for start
    sudo -u postgres "$PGBIN/pg_ctl" -w -l /dev/null -D "$DATADIR" start

    # create the test database
    DBNAME=psycopg2_test
    CONNINFO="user=postgres host=localhost port=$PORT dbname=template1"

    if (( "$VERNUM" >= 901 )); then
        psql -c "create user travis createdb createrole replication" "$CONNINFO"
    elif (( "$VERNUM" >= 801 )); then
        psql -c "create user travis createdb createrole" "$CONNINFO"
    else
        psql -c "create user travis createdb createuser" "$CONNINFO"
    fi

    psql -c "create database $DBNAME with owner travis" "$CONNINFO"

    # configure global objects on the test database
    CONNINFO="user=postgres host=localhost port=$PORT dbname=$DBNAME"

    if (( "$VERNUM" >= 901 )); then
        psql -c "create extension hstore" "$CONNINFO"
    elif (( "$VERNUM" >= 803 )); then
        psql -f "$PGDIR/share/contrib/hstore.sql" "$CONNINFO"
    fi

    if (( "$VERNUM" == 901 )); then
        psql -c "create extension json" "$CONNINFO"
    fi
}

# Would give a permission denied error in the travis build dir
cd /

# Postgres versions supported by Travis CI
if (( ! "$DONT_TEST_PRESENT" )); then
    create 12
    create 11
    create 10
    create 9.6
    create 9.5
    create 9.4
fi

# Unsupported postgres versions that we still support
# Images built by https://github.com/psycopg/psycopg2-wheels/tree/build-dinosaurs
if (( "$TEST_PAST" )); then
    create 7.4
    create 8.0
    create 8.1
    create 8.2
    create 8.3
    create 8.4
    create 9.0
    create 9.1
    create 9.2
    create 9.3
fi

# Postgres built from master
if (( "$TEST_FUTURE" )); then
    create 13 13-master
fi
