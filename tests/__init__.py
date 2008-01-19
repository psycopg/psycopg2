#!/usr/bin/env python
import os
import unittest

dbname = os.environ.get('PSYCOPG2_TESTDB', 'psycopg2_test')

import bugX000
import extras_dictcursor
import test_dates
import test_psycopg2_dbapi20
import test_quote
import test_connection
import test_transaction
import types_basic

def test_suite():
    suite = unittest.TestSuite()
    suite.addTest(bugX000.test_suite())
    suite.addTest(extras_dictcursor.test_suite())
    suite.addTest(test_dates.test_suite())
    suite.addTest(test_psycopg2_dbapi20.test_suite())
    suite.addTest(test_quote.test_suite())
    suite.addTest(test_connection.test_suite())
    suite.addTest(test_transaction.test_suite())
    suite.addTest(types_basic.test_suite())
    return suite

if __name__ == '__main__':
    unittest.main(defaultTest='test_suite')
