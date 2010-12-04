#!/usr/bin/env python

import time
import threading
from testutils import unittest, skip_if_no_pg_sleep

import tests
import psycopg2
import psycopg2.extensions
from psycopg2 import extras


class CancelTests(unittest.TestCase):

    def setUp(self):
        self.conn = psycopg2.connect(tests.dsn)
        cur = self.conn.cursor()
        cur.execute('''
            CREATE TEMPORARY TABLE table1 (
              id int PRIMARY KEY
            )''')
        self.conn.commit()

    def tearDown(self):
        self.conn.close()

    def test_empty_cancel(self):
        self.conn.cancel()

    @skip_if_no_pg_sleep('conn')
    def test_cancel(self):
        errors = []

        def neverending(conn):
            cur = conn.cursor()
            try:
                self.assertRaises(psycopg2.extensions.QueryCanceledError,
                                  cur.execute, "select pg_sleep(60)")
            # make sure the connection still works
                conn.rollback()
                cur.execute("select 1")
                self.assertEqual(cur.fetchall(), [(1, )])
            except Exception, e:
                errors.append(e)
                raise

        def canceller(conn):
            cur = conn.cursor()
            try:
                conn.cancel()
            except Exception, e:
                errors.append(e)
                raise

        thread1 = threading.Thread(target=neverending, args=(self.conn, ))
        # wait a bit to make sure that the other thread is already in
        # pg_sleep -- ugly and racy, but the chances are ridiculously low
        thread2 = threading.Timer(0.3, canceller, args=(self.conn, ))
        thread1.start()
        thread2.start()
        thread1.join()
        thread2.join()

        self.assertEqual(errors, [])

    @skip_if_no_pg_sleep('conn')
    def test_async_cancel(self):
        async_conn = psycopg2.connect(tests.dsn, async=True)
        self.assertRaises(psycopg2.OperationalError, async_conn.cancel)
        extras.wait_select(async_conn)
        cur = async_conn.cursor()
        cur.execute("select pg_sleep(10000)")
        self.assertTrue(async_conn.isexecuting())
        async_conn.cancel()
        self.assertRaises(psycopg2.extensions.QueryCanceledError,
                          extras.wait_select, async_conn)
        cur.execute("select 1")
        extras.wait_select(async_conn)
        self.assertEqual(cur.fetchall(), [(1, )])

    def test_async_connection_cancel(self):
        async_conn = psycopg2.connect(tests.dsn, async=True)
        async_conn.close()
        self.assertTrue(async_conn.closed)


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()
