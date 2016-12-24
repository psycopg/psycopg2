#!/bin/bash

# Run the tests in all the databases
# The script is designed for a Trusty environment.

set -e

run_test () {
    version=$1
    port=$2
    pyver=$(python -c "import sys; print(''.join(map(str,sys.version_info[:2])))")
    dbname=psycopg_test_$pyver

    # Create a database for each python version to allow tests to run in parallel
    psql -c "create database $dbname" \
        "user=postgres port=$port dbname=postgres"

    psql -c "grant create on database $dbname to travis" \
        "user=postgres port=$port dbname=postgres"

    psql -c "create extension hstore" \
        "user=postgres port=$port dbname=$dbname"

    printf "\n\nRunning tests against PostgreSQL $version\n\n"
    export PSYCOPG2_TESTDB=$dbname
    export PSYCOPG2_TESTDB_PORT=$port
    export PSYCOPG2_TESTDB_USER=travis
    make check

    printf "\n\nRunning tests against PostgreSQL $version (green mode)\n\n"
    export PSYCOPG2_TEST_GREEN=1
    make check
}

run_test 9.6 54396
run_test 9.5 54395
run_test 9.4 54394
run_test 9.3 54393
run_test 9.2 54392
