#!/bin/bash

# Run the tests in all the databases
# The script is designed for a Trusty environment.

set -e

run_test () {
    version=$1
    port=$2
    dbname=psycopg2_test

    printf "\n\nRunning tests against PostgreSQL $version\n\n"
    export PSYCOPG2_TESTDB=$dbname
    export PSYCOPG2_TESTDB_PORT=$port
    export PSYCOPG2_TESTDB_USER=travis
    export PSYCOPG2_TEST_REPL_DSN=
    unset PSYCOPG2_TEST_GREEN
    python -c "from psycopg2 import tests; tests.unittest.main(defaultTest='tests.test_suite')"

    printf "\n\nRunning tests against PostgreSQL $version (green mode)\n\n"
    export PSYCOPG2_TEST_GREEN=1
    python -c "from psycopg2 import tests; tests.unittest.main(defaultTest='tests.test_suite')"
}

run_test 9.6 54396
run_test 9.5 54395
run_test 9.4 54394
run_test 9.3 54393
run_test 9.2 54392
