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
        self.make_test_table()
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

    def make_test_table(self):
        cnn = self.connect()
        cur = cnn.cursor()
        cur.execute("DROP TABLE IF EXISTS test_tpc;")
        cur.execute("CREATE TABLE test_tpc (data text);")
        cnn.commit()
        cnn.close()

    def count_xacts(self):
        """Return the number of prepared xacts currently in the test db."""
        cnn = self.connect()
        cur = cnn.cursor()
        cur.execute("""
            select count(*) from pg_prepared_xacts
            where database = %s;""",
            (tests.dbname,))
        rv = cur.fetchone()[0]
        cnn.close()
        return rv

    def count_test_records(self):
        """Return the number of records in the test table."""
        cnn = self.connect()
        cur = cnn.cursor()
        cur.execute("select count(*) from test_tpc;")
        rv = cur.fetchone()[0]
        cnn.close()
        return rv

    def connect(self):
        return psycopg2.connect(tests.dsn)

    def test_tpc_commit(self):
        cnn = self.connect()
        xid = cnn.xid(1, "gtrid", "bqual")
        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_READY)

        cnn.tpc_begin(xid)
        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_BEGIN)

        cur = cnn.cursor()
        cur.execute("insert into test_tpc values ('test_tpc_commit');")
        self.assertEqual(0, self.count_xacts())
        self.assertEqual(0, self.count_test_records())

        cnn.tpc_prepare()
        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_PREPARED)
        self.assertEqual(1, self.count_xacts())
        self.assertEqual(0, self.count_test_records())

        cnn.tpc_commit()
        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_READY)
        self.assertEqual(0, self.count_xacts())
        self.assertEqual(1, self.count_test_records())

    def test_tpc_commit_one_phase(self):
        cnn = self.connect()
        xid = cnn.xid(1, "gtrid", "bqual")
        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_READY)

        cnn.tpc_begin(xid)
        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_BEGIN)

        cur = cnn.cursor()
        cur.execute("insert into test_tpc values ('test_tpc_commit_1p');")
        self.assertEqual(0, self.count_xacts())
        self.assertEqual(0, self.count_test_records())

        cnn.tpc_commit()
        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_READY)
        self.assertEqual(0, self.count_xacts())
        self.assertEqual(1, self.count_test_records())

    def test_tpc_commit_recovered(self):
        cnn = self.connect()
        xid = cnn.xid(1, "gtrid", "bqual")
        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_READY)

        cnn.tpc_begin(xid)
        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_BEGIN)

        cur = cnn.cursor()
        cur.execute("insert into test_tpc values ('test_tpc_commit_rec');")
        self.assertEqual(0, self.count_xacts())
        self.assertEqual(0, self.count_test_records())

        cnn.tpc_prepare()
        cnn.close()
        self.assertEqual(1, self.count_xacts())
        self.assertEqual(0, self.count_test_records())

        cnn = self.connect()
        xid = cnn.xid(1, "gtrid", "bqual")
        cnn.tpc_commit(xid)

        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_READY)
        self.assertEqual(0, self.count_xacts())
        self.assertEqual(1, self.count_test_records())

    def test_tpc_rollback(self):
        cnn = self.connect()
        xid = cnn.xid(1, "gtrid", "bqual")
        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_READY)

        cnn.tpc_begin(xid)
        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_BEGIN)

        cur = cnn.cursor()
        cur.execute("insert into test_tpc values ('test_tpc_rollback');")
        self.assertEqual(0, self.count_xacts())
        self.assertEqual(0, self.count_test_records())

        cnn.tpc_prepare()
        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_PREPARED)
        self.assertEqual(1, self.count_xacts())
        self.assertEqual(0, self.count_test_records())

        cnn.tpc_rollback()
        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_READY)
        self.assertEqual(0, self.count_xacts())
        self.assertEqual(0, self.count_test_records())

    def test_tpc_rollback_one_phase(self):
        cnn = self.connect()
        xid = cnn.xid(1, "gtrid", "bqual")
        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_READY)

        cnn.tpc_begin(xid)
        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_BEGIN)

        cur = cnn.cursor()
        cur.execute("insert into test_tpc values ('test_tpc_rollback_1p');")
        self.assertEqual(0, self.count_xacts())
        self.assertEqual(0, self.count_test_records())

        cnn.tpc_rollback()
        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_READY)
        self.assertEqual(0, self.count_xacts())
        self.assertEqual(0, self.count_test_records())

    def test_tpc_rollback_recovered(self):
        cnn = self.connect()
        xid = cnn.xid(1, "gtrid", "bqual")
        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_READY)

        cnn.tpc_begin(xid)
        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_BEGIN)

        cur = cnn.cursor()
        cur.execute("insert into test_tpc values ('test_tpc_commit_rec');")
        self.assertEqual(0, self.count_xacts())
        self.assertEqual(0, self.count_test_records())

        cnn.tpc_prepare()
        cnn.close()
        self.assertEqual(1, self.count_xacts())
        self.assertEqual(0, self.count_test_records())

        cnn = self.connect()
        xid = cnn.xid(1, "gtrid", "bqual")
        cnn.tpc_rollback(xid)

        self.assertEqual(cnn.status, psycopg2.extensions.STATUS_READY)
        self.assertEqual(0, self.count_xacts())
        self.assertEqual(0, self.count_test_records())

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

    def test_xid_encoding(self):
        cnn = self.connect()
        xid = cnn.xid(42, "gtrid", "bqual")
        cnn.tpc_begin(xid)
        cnn.tpc_prepare()

        cnn = self.connect()
        cur = cnn.cursor()
        cur.execute("select gid from pg_prepared_xacts where database = %s;",
            (tests.dbname,))
        self.assertEqual('42_Z3RyaWQ=_YnF1YWw=', cur.fetchone()[0])

    def test_xid_roundtrip(self):
        for fid, gtrid, bqual in [
            (0, "", ""),
            (42, "gtrid", "bqual"),
            (0x7fffffff, "x" * 64, "y" * 64),
        ]:
            cnn = self.connect()
            xid = cnn.xid(fid, gtrid, bqual)
            cnn.tpc_begin(xid)
            cnn.tpc_prepare()
            cnn.close()

            cnn = self.connect()
            xids = [ xid for xid in cnn.tpc_recover()
                if xid.database == tests.dbname ]
            self.assertEqual(1, len(xids))
            xid = xids[0]
            self.assertEqual(xid.format_id, fid)
            self.assertEqual(xid.gtrid, gtrid)
            self.assertEqual(xid.bqual, bqual)

            cnn.tpc_rollback(xid)

    def test_unparsed_roundtrip(self):
        for tid in [
            '',
            'hello, world!',
            'x' * 199,  # PostgreSQL's limit in transaction id length
        ]:
            cnn = self.connect()
            cnn.tpc_begin(tid)
            cnn.tpc_prepare()
            cnn.close()

            cnn = self.connect()
            xids = [ xid for xid in cnn.tpc_recover()
                if xid.database == tests.dbname ]
            self.assertEqual(1, len(xids))
            xid = xids[0]
            self.assertEqual(xid.format_id, None)
            self.assertEqual(xid.gtrid, tid)
            self.assertEqual(xid.bqual, None)

            cnn.tpc_rollback(xid)


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()
