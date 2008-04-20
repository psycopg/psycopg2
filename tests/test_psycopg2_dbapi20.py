#!/usr/bin/env python
import dbapi20
import unittest
import psycopg2
import popen2

import tests

class Psycopg2TestCase(dbapi20.DatabaseAPI20Test):
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


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == '__main__':
    unittest.main()
