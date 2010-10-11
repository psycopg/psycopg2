#!/usr/bin/env python

import unittest
from operator import attrgetter

import psycopg2
import psycopg2.extensions
import tests

class ConnectionTests(unittest.TestCase):

    def connect(self):
        return psycopg2.connect(tests.dsn)

    def test_closed_attribute(self):
        conn = self.connect()
        self.assertEqual(conn.closed, False)
        conn.close()
        self.assertEqual(conn.closed, True)

    def test_cursor_closed_attribute(self):
        conn = self.connect()
        curs = conn.cursor()
        self.assertEqual(curs.closed, False)
        curs.close()
        self.assertEqual(curs.closed, True)

        # Closing the connection closes the cursor:
        curs = conn.cursor()
        conn.close()
        self.assertEqual(curs.closed, True)

    def test_reset(self):
        conn = self.connect()
        # switch isolation level, then reset
        level = conn.isolation_level
        conn.set_isolation_level(0)
        self.assertEqual(conn.isolation_level, 0)
        conn.reset()
        # now the isolation level should be equal to saved one
        self.assertEqual(conn.isolation_level, level)

    def test_notices(self):
        conn = self.connect()
        cur = conn.cursor()
        cur.execute("create temp table chatty (id serial primary key);")
        self.assertEqual("CREATE TABLE", cur.statusmessage)
        self.assert_(conn.notices)
        conn.close()

    def test_server_version(self):
        conn = self.connect()
        self.assert_(conn.server_version)

    def test_protocol_version(self):
        conn = self.connect()
        self.assert_(conn.protocol_version in (2,3), conn.protocol_version)

    def test_isolation_level(self):
        conn = self.connect()
        self.assertEqual(
            conn.isolation_level,
            psycopg2.extensions.ISOLATION_LEVEL_READ_COMMITTED)

    def test_encoding(self):
        conn = self.connect()
        self.assert_(conn.encoding in psycopg2.extensions.encodings)


class ConnectionTwoPhaseTests(unittest.TestCase):
    def setUp(self):
        self.clear_test_xacts()

    def tearDown(self):
        self.clear_test_xacts()

    def clear_test_xacts(self):
        """Rollback all the prepared transaction in the testing db."""
        cnn = self.connect()
        cnn.set_isolation_level(0)
        cur = cnn.cursor()
        cur.execute(
            "select gid from pg_prepared_xacts where database = %s",
            (tests.dbname,))
        gids = [ r[0] for r in cur ]
        for gid in gids:
            cur.execute("rollback prepared %s;", (gid,))
        cnn.close()

    def connect(self):
        return psycopg2.connect(tests.dsn)

    def test_status_after_recover(self):
        cnn = self.connect()
        self.assertEqual(psycopg2.extensions.STATUS_READY, cnn.status)
        xns = cnn.tpc_recover()
        self.assertEqual(psycopg2.extensions.STATUS_READY, cnn.status)

        cur = cnn.cursor()
        cur.execute("select 1")
        self.assertEqual(psycopg2.extensions.STATUS_BEGIN, cnn.status)
        xns = cnn.tpc_recover()
        self.assertEqual(psycopg2.extensions.STATUS_BEGIN, cnn.status)

    def test_recovered_xids(self):
        # insert a few test xns
        cnn = self.connect()
        cnn.set_isolation_level(0)
        cur = cnn.cursor()
        cur.execute("begin; prepare transaction '1-foo';")
        cur.execute("begin; prepare transaction '2-bar';")

        # read the values to return
        cur.execute("""
            select gid, prepared, owner, database
            from pg_prepared_xacts
            where database = %s;""",
            (tests.dbname,))
        okvals = cur.fetchall()
        okvals.sort()

        cnn = self.connect()
        xids = cnn.tpc_recover()
        xids = [ xid for xid in xids if xid.database == tests.dbname ]
        xids.sort(key=attrgetter('gtrid'))

        # check the values returned
        self.assertEqual(len(okvals), len(xids))
        for (xid, (gid, prepared, owner, database)) in zip (xids, okvals):
            self.assertEqual(xid.gtrid, gid)
            self.assertEqual(xid.prepared, prepared)
            self.assertEqual(xid.owner, owner)
            self.assertEqual(xid.database, database)


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()
