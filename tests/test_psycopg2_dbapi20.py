#!/usr/bin/env python
import dbapi20
import dbapi20_tpc
from test_connection import skip_if_tpc_disabled
from testutils import unittest, decorate_all_tests
import psycopg2

import tests

class Psycopg2Tests(dbapi20.DatabaseAPI20Test):
    driver = psycopg2
    connect_args = ()
    connect_kw_args = {'dsn': tests.dsn}

    lower_func = 'lower' # For stored procedure test

    def test_setoutputsize(self):
        # psycopg2's setoutputsize() is a no-op
        pass

    def test_nextset(self):
        # psycopg2 does not implement nextset()
        pass


class Psycopg2TPCTests(dbapi20_tpc.TwoPhaseCommitTests, unittest.TestCase):
    driver = psycopg2

    def connect(self):
        return psycopg2.connect(dsn=tests.dsn)

decorate_all_tests(Psycopg2TPCTests, skip_if_tpc_disabled)


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == '__main__':
    unittest.main()
