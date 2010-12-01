#!/usr/bin/env python

import unittest
import psycopg2
import psycopg2.extensions
import psycopg2.extras
import tests

class ConnectionStub(object):
    """A `connection` wrapper allowing analysis of the `poll()` calls."""
    def __init__(self, conn):
        self.conn = conn
        self.polls = []

    def fileno(self):
        return self.conn.fileno()

    def poll(self):
        rv = self.conn.poll()
        self.polls.append(rv)
        return rv

class GreenTests(unittest.TestCase):
    def setUp(self):
        self._cb = psycopg2.extensions.get_wait_callback()
        psycopg2.extensions.set_wait_callback(psycopg2.extras.wait_select)
        self.conn = psycopg2.connect(tests.dsn)

    def tearDown(self):
        self.conn.close()
        psycopg2.extensions.set_wait_callback(self._cb)

    def set_stub_wait_callback(self, conn):
        stub = ConnectionStub(conn)
        psycopg2.extensions.set_wait_callback(
            lambda conn: psycopg2.extras.wait_select(stub))
        return stub

    def test_flush_on_write(self):
        # a very large query requires a flush loop to be sent to the backend
        conn = self.conn
        stub = self.set_stub_wait_callback(conn)
        curs = conn.cursor()
        for mb in 1, 5, 10, 20, 50:
            size = mb * 1024 * 1024
            del stub.polls[:]
            curs.execute("select %s;", ('x' * size,))
            self.assertEqual(size, len(curs.fetchone()[0]))
            if stub.polls.count(psycopg2.extensions.POLL_WRITE) > 1:
                return

        # This is more a testing glitch than an error: it happens
        # on high load on linux: probably because the kernel has more
        # buffers ready. A warning may be useful during development,
        # but an error is bad during regression testing.
        import warnings
        warnings.warn("sending a large query didn't trigger block on write.")

    def test_error_in_callback(self):
        conn = self.conn
        curs = conn.cursor()
        curs.execute("select 1")  # have a BEGIN
        curs.fetchone()

        # now try to do something that will fail in the callback
        psycopg2.extensions.set_wait_callback(lambda conn: 1/0)
        self.assertRaises(ZeroDivisionError, curs.execute, "select 2")

        # check that the connection is left in an usable state
        psycopg2.extensions.set_wait_callback(psycopg2.extras.wait_select)
        conn.rollback()
        curs.execute("select 2")
        self.assertEqual(2, curs.fetchone()[0])


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()
