#!/usr/bin/env python

import os
import unittest

dbname = os.environ.get('PSYCOPG2_TESTDB', 'psycopg2_test')
dbhost = os.environ.get('PSYCOPG2_TESTDB_HOST', None)
dbport = os.environ.get('PSYCOPG2_TESTDB_PORT', None)
dbuser = os.environ.get('PSYCOPG2_TESTDB_USER', None)

# Check if we want to test psycopg's green path.
green = os.environ.get('PSYCOPG2_TEST_GREEN', None)
if green:
    if green == '1':
        from psycopg2.extras import wait_select as wait_callback
    elif green == 'eventlet':
        from eventlet.support.psycopg2_patcher import eventlet_wait_callback \
            as wait_callback
    else:
        raise ValueError("please set 'PSYCOPG2_TEST_GREEN' to a valid value")

    import psycopg2.extensions
    psycopg2.extensions.set_wait_callback(wait_callback)

# Construct a DSN to connect to the test database:
dsn = 'dbname=%s' % dbname
if dbhost is not None:
    dsn += ' host=%s' % dbhost
if dbport is not None:
    dsn += ' port=%s' % dbport
if dbuser is not None:
    dsn += ' user=%s' % dbuser

import bugX000
import extras_dictcursor
import test_dates
import test_psycopg2_dbapi20
import test_quote
import test_connection
import test_cursor
import test_transaction
import types_basic
import types_extras
import test_lobject
import test_copy
import test_notify
import test_async
import test_green

def test_suite():
    suite = unittest.TestSuite()
    suite.addTest(bugX000.test_suite())
    suite.addTest(extras_dictcursor.test_suite())
    suite.addTest(test_dates.test_suite())
    suite.addTest(test_psycopg2_dbapi20.test_suite())
    suite.addTest(test_quote.test_suite())
    suite.addTest(test_connection.test_suite())
    suite.addTest(test_cursor.test_suite())
    suite.addTest(test_transaction.test_suite())
    suite.addTest(types_basic.test_suite())
    suite.addTest(types_extras.test_suite())

    if not green:
        suite.addTest(test_lobject.test_suite())
        suite.addTest(test_copy.test_suite())
    else:
        import warnings
        warnings.warn("copy/lobjects not implemented in green mode: skipping tests")

    suite.addTest(test_notify.test_suite())
    suite.addTest(test_async.test_suite())
    suite.addTest(test_green.test_suite())
    return suite

if __name__ == '__main__':
    unittest.main(defaultTest='test_suite')
