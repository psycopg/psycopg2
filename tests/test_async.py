#!/usr/bin/env python
import unittest

import psycopg2
from psycopg2 import extensions

import select
import StringIO

import sys
if sys.version_info < (3,):
    import tests
else:
    import py3tests as tests


class AsyncTests(unittest.TestCase):

    def setUp(self):
        self.sync_conn = psycopg2.connect(tests.dsn)
        self.conn = psycopg2.connect(tests.dsn, async=True)

        state = psycopg2.extensions.POLL_WRITE
        while state != psycopg2.extensions.POLL_OK:
            if state == psycopg2.extensions.POLL_WRITE:
                select.select([], [self.conn.fileno()], [])
            elif state == psycopg2.extensions.POLL_READ:
                select.select([self.conn.fileno()], [], [])
            state = self.conn.poll()

        curs = self.conn.cursor()
        curs.execute('''
            CREATE TEMPORARY TABLE table1 (
              id int PRIMARY KEY
            )''')
        self.conn.commit()

    def tearDown(self):
        self.sync_conn.close()
        self.conn.close()

    def wait_for_query(self, cur):
        state = cur.poll()
        while state != psycopg2.extensions.POLL_OK:
            if state == psycopg2.extensions.POLL_READ:
                select.select([cur.fileno()], [], [])
            elif state == psycopg2.extensions.POLL_WRITE:
                select.select([], [cur.fileno()], [])
            state = cur.poll()

    def test_wrong_execution_type(self):
        cur = self.conn.cursor()
        sync_cur = self.sync_conn.cursor()

        self.assertRaises(psycopg2.ProgrammingError, cur.execute,
                          "select 'a'", async=False)
        self.assertRaises(psycopg2.ProgrammingError, sync_cur.execute,
                          "select 'a'", async=True)

        # but this should work anyway
        sync_cur.execute("select 'a'", async=False)
        cur.execute("select 'a'", async=True)

    def test_async_select(self):
        cur = self.conn.cursor()
        self.assertFalse(self.conn.executing())
        cur.execute("select 'a'")
        self.assertTrue(self.conn.executing())

        self.wait_for_query(cur)

        self.assertFalse(self.conn.executing())
        self.assertEquals(cur.fetchone()[0], "a")

    def test_async_callproc(self):
        cur = self.conn.cursor()
        try:
            cur.callproc("pg_sleep", (0.1, ), True)
        except psycopg2.ProgrammingError:
            # PG <8.1 did not have pg_sleep
            return
        self.assertTrue(self.conn.executing())

        self.wait_for_query(cur)
        self.assertFalse(self.conn.executing())
        self.assertEquals(cur.fetchall()[0][0], '')

    def test_async_after_async(self):
        cur = self.conn.cursor()
        cur2 = self.conn.cursor()

        cur.execute("insert into table1 values (1)")

        # an async execute after an async one blocks and waits for completion
        cur.execute("select * from table1")
        self.wait_for_query(cur)

        self.assertEquals(cur.fetchall()[0][0], 1)

        cur.execute("delete from table1")
        self.wait_for_query(cur)

        cur.execute("select * from table1")
        self.wait_for_query(cur)

        self.assertEquals(cur.fetchone(), None)

    def test_fetch_after_async(self):
        cur = self.conn.cursor()
        cur.execute("select 'a'")

        # a fetch after an asynchronous query blocks and waits for completion
        self.assertEquals(cur.fetchall()[0][0], "a")

    def test_rollback_while_async(self):
        cur = self.conn.cursor()

        cur.execute("select 'a'")

        # a rollback blocks and should leave the connection in a workable state
        self.conn.rollback()
        self.assertFalse(self.conn.executing())

        # try a sync cursor first
        sync_cur = self.sync_conn.cursor()
        sync_cur.execute("select 'b'")
        self.assertEquals(sync_cur.fetchone()[0], "b")

        # now try the async cursor
        cur.execute("select 'c'")
        self.wait_for_query(cur)
        self.assertEquals(cur.fetchmany()[0][0], "c")

    def test_commit_while_async(self):
        cur = self.conn.cursor()

        cur.execute("insert into table1 values (1)")

        # a commit blocks
        self.conn.commit()
        self.assertFalse(self.conn.executing())

        cur.execute("select * from table1")
        self.wait_for_query(cur)
        self.assertEquals(cur.fetchall()[0][0], 1)

        cur.execute("delete from table1")
        self.conn.commit()

        cur.execute("select * from table1")
        self.wait_for_query(cur)
        self.assertEquals(cur.fetchone(), None)

    def test_set_parameters_while_async(self):
        prev_encoding = self.conn.encoding
        cur = self.conn.cursor()

        cur.execute("select 'c'")
        self.assertTrue(self.conn.executing())

        # getting transaction status works
        self.assertEquals(self.conn.get_transaction_status(),
                          extensions.TRANSACTION_STATUS_ACTIVE)
        self.assertTrue(self.conn.executing())

        # this issues a ROLLBACK internally
        self.conn.set_client_encoding("LATIN1")

        self.assertFalse(self.conn.executing())
        self.assertEquals(self.conn.encoding, "LATIN1")

        self.conn.set_client_encoding(prev_encoding)

    def test_reset_while_async(self):
        prev_encoding = self.conn.encoding
        # pick something different than the current encoding
        new_encoding = (prev_encoding == "LATIN1") and "UTF8" or "LATIN1"

        self.conn.set_client_encoding(new_encoding)

        cur = self.conn.cursor()
        cur.execute("select 'c'")
        self.assertTrue(self.conn.executing())

        self.conn.reset()
        self.assertFalse(self.conn.executing())
        self.assertEquals(self.conn.encoding, prev_encoding)

    def test_async_iter(self):
        cur = self.conn.cursor()

        cur.execute("insert into table1 values (1), (2), (3)")
        self.wait_for_query(cur)
        cur.execute("select id from table1 order by id")

        # iteration just blocks
        self.assertEquals(list(cur), [(1, ), (2, ), (3, )])
        self.assertFalse(self.conn.executing())

    def test_copy_while_async(self):
        cur = self.conn.cursor()
        cur.execute("select 'a'")

        # copy just blocks
        cur.copy_from(StringIO.StringIO("1\n3\n5\n\\.\n"), "table1")

        cur.execute("select * from table1 order by id")
        self.assertEquals(cur.fetchall(), [(1, ), (3, ), (5, )])

    def test_async_executemany(self):
        cur = self.conn.cursor()
        self.assertRaises(
            psycopg2.ProgrammingError,
            cur.executemany, "insert into table1 values (%s)", [1, 2, 3])

    def test_async_scroll(self):
        cur = self.conn.cursor()
        cur.execute("insert into table1 values (1), (2), (3)")
        self.wait_for_query(cur)
        cur.execute("select id from table1 order by id")

        # scroll blocks, but should work
        cur.scroll(1)
        self.assertFalse(self.conn.executing())
        self.assertEquals(cur.fetchall(), [(2, ), (3, )])

        cur = self.conn.cursor()
        cur.execute("select id from table1 order by id")

        cur2 = self.conn.cursor()
        self.assertRaises(psycopg2.ProgrammingError, cur2.scroll, 1)

        self.assertRaises(psycopg2.ProgrammingError, cur.scroll, 4)

        cur = self.conn.cursor()
        cur.execute("select id from table1 order by id")
        cur.scroll(2)
        cur.scroll(-1)
        self.assertEquals(cur.fetchall(), [(2, ), (3, )])

    def test_async_dont_read_all(self):
        cur = self.conn.cursor()
        cur.execute("select 'a'; select 'b'")

        # fetch the result
        self.wait_for_query(cur)

        # it should be the result of the second query
        self.assertEquals(cur.fetchone()[0][0], "b")

def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)
