#!/usr/bin/env python

# test_pool.py - unit test for psycopg2 classes
#
# psycopg2 is free software: you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# In addition, as a special exception, the copyright holders give
# permission to link this program with the OpenSSL library (or with
# modified versions of OpenSSL that use the same license as OpenSSL),
# and distribute linked combinations including the two.
#
# You must obey the GNU Lesser General Public License in all respects for
# all of the code used other than OpenSSL.
#
# psycopg2 is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.

import unittest

import psycopg2
import psycopg2.extensions as _ext
import psycopg2.pool

from .testconfig import dsn, dbname
from .testutils import ConnectingTestCase, skip_before_postgres


class PoolTests(ConnectingTestCase):

    def test_get_conn(self):
        """Test the call to getconn. Should just return an open connection."""
        pool = psycopg2.pool.SimpleConnectionPool(0, 1, dsn, idle_timeout=30)
        conn = pool.getconn()

        # Make sure we got an open connection
        self.assertFalse(conn.closed)

        # Try again. We should get an error, since we only allowed one connection.
        self.assertRaises(psycopg2.pool.PoolError, pool.getconn)

        # Put the connection back, the return time should be set
        pool.putconn(conn)
        self.assertIn(id(conn), pool._return_times)

        # Get the connection back
        new_conn = pool.getconn()
        self.assertIs(new_conn, conn)

    def test_prune(self):
        """Test the prune function to make sure it closes conenctions and removes them from the pool"""
        pool = psycopg2.pool.SimpleConnectionPool(0, 3, dsn, idle_timeout=30)

        # Get a connection that we use, so it can't be pruned.
        sticky_conn = pool.getconn()
        self.assertFalse(sticky_conn in pool._pool)
        self.assertTrue(sticky_conn in pool._used.values())
        self.assertFalse(sticky_conn.closed)
        self.assertFalse(id(sticky_conn) in pool._return_times)

        # Create a second connection that is put back into the pool, available to be pruned
        conn = pool.getconn()

        # Create a third connection that is put back into the pool, but won't be expired
        new_conn = pool.getconn()

        # Put the connections back in the pool
        pool.putconn(conn)
        pool.putconn(new_conn)

        # Verify that everything is in the expected state
        self.assertTrue(conn in pool._pool)
        self.assertFalse(conn in pool._used.values())
        self.assertFalse(conn.closed)
        self.assertTrue(id(conn) in pool._return_times)

        self.assertTrue(new_conn in pool._pool)
        self.assertFalse(new_conn in pool._used.values())
        self.assertFalse(new_conn.closed)
        self.assertTrue(id(new_conn) in pool._return_times)

        self.assertNotEqual(conn, sticky_conn)
        self.assertNotEqual(new_conn, conn)

        # Make the connections expire a minute ago (but not new_con)
        pool._return_times[id(conn)] -= 60
        pool._return_times[id(sticky_conn)] = pool._return_times[id(conn)]

        # Prune connections
        pool._prune()

        # Make sure the unused expired connection is gone and closed,
        # but the used connection isn't
        self.assertFalse(conn in pool._pool)
        self.assertTrue(conn.closed)
        self.assertFalse(id(conn) in pool._return_times)

        self.assertFalse(sticky_conn.closed)
        self.assertTrue(id(sticky_conn) in pool._return_times)

        # The un-expired connection should still exist and be open
        self.assertFalse(new_conn.closed)
        self.assertTrue(id(new_conn) in pool._return_times)

    def test_prune_below_min(self):
        pool = psycopg2.pool.SimpleConnectionPool(1, 1, dsn, idle_timeout=30)
        conn = pool.getconn()
        self.assertFalse(conn in pool._pool)

        # Nothing in pool, since we only allow one connection here
        self.assertEqual(len(pool._pool), 0)

        pool.putconn(conn, close=True)

        # This connection should *not* be in the pool, since we said close
        self.assertFalse(conn in pool._pool)

        # But we should still have *a* connection available
        self.assertEqual(len(pool._pool), 1)

    def test_putconn_normal(self):
        pool = psycopg2.pool.SimpleConnectionPool(0, 1, dsn, idle_timeout=30)
        conn = pool.getconn()
        self.assertFalse(conn in pool._pool)

        pool.putconn(conn)
        self.assertTrue(conn in pool._pool)

    def test_putconn_closecon(self):
        pool = psycopg2.pool.SimpleConnectionPool(0, 1, dsn, idle_timeout=30)
        conn = pool.getconn()
        self.assertFalse(conn in pool._pool)

        pool.putconn(conn, close=True)
        self.assertFalse(conn in pool._pool)
        self.assertFalse(id(conn) in pool._return_times)

    def test_getconn_expired(self):
        pool = psycopg2.pool.SimpleConnectionPool(0, 1, dsn, idle_timeout=30)
        conn = pool.getconn()

        # Expire the connection
        pool.putconn(conn)
        pool._return_times[id(conn)] -= 60

        # Connection should be discarded
        new_conn = pool.getconn()
        self.assertIsNot(new_conn, conn)
        self.assertFalse(conn in pool._pool)
        self.assertFalse(id(conn) in pool._return_times)
        self.assertTrue(conn.closed)

    def test_putconn_unkeyed(self):
        pool = psycopg2.pool.SimpleConnectionPool(0, 1, dsn, idle_timeout=30)

        # Test put with missing key
        conn = pool.getconn()
        del pool._rused[id(conn)]
        self.assertRaises(psycopg2.pool.PoolError, pool.putconn, conn)

    def test_putconn_errorState(self):
        pool = psycopg2.pool.SimpleConnectionPool(0, 1, dsn, idle_timeout=30)
        conn = pool.getconn()

        # Get connection into transaction state
        cursor = conn.cursor()
        try:
            cursor.execute("INSERT INTO nonexistent (id) VALUES (1)")
        except psycopg2.ProgrammingError:
            pass

        self.assertNotEqual(conn.get_transaction_status(),
                            _ext.TRANSACTION_STATUS_IDLE)

        pool.putconn(conn)

        # Make sure we got back into the pool and are now showing idle
        self.assertEqual(conn.get_transaction_status(),
                         _ext.TRANSACTION_STATUS_IDLE)
        self.assertTrue(conn in pool._pool)

    def test_putconn_closed(self):
        pool = psycopg2.pool.SimpleConnectionPool(0, 1, dsn, idle_timeout=30)
        conn = pool.getconn()

        # The connection should be open and shouldn't have a return time
        self.assertFalse(conn.closed)
        self.assertFalse(id(conn) in pool._return_times)

        conn.close()

        # Now should be closed
        self.assertTrue(conn.closed)

        pool.putconn(conn)

        # The connection should have been discarded
        self.assertFalse(conn in pool._pool)
        self.assertFalse(id(conn) in pool._return_times)

    @skip_before_postgres(8, 2)
    def test_caching(self):
        pool = psycopg2.pool.SimpleConnectionPool(0, 10, dsn, idle_timeout=30)

        # Get a connection to use to check the number of connections
        check_conn = pool.getconn()
        check_conn.autocommit = True  # Being in a transaction hides the other connections.
        # Get a cursor to run check queries with
        check_cursor = check_conn.cursor()

        SQL = "SELECT numbackends from pg_stat_database WHERE datname=%s"
        check_cursor.execute(SQL, (dbname, ))

        # Not trying to test anything yet, so hopefully this always works :)
        starting_conns = check_cursor.fetchone()[0]

        # Get a couple more connections
        conn2 = pool.getconn()
        conn3 = pool.getconn()

        self.assertNotEqual(conn2, conn3)

        # Verify that we have the expected number of connections to the DB server now
        check_cursor.execute(SQL, (dbname, ))
        total_cons = check_cursor.fetchone()[0]

        self.assertEqual(total_cons, starting_conns + 2)

        # Put the connections back in the pool and verify they don't close
        pool.putconn(conn2)
        pool.putconn(conn3)

        check_cursor.execute(SQL, (dbname, ))
        total_cons_after_put = check_cursor.fetchone()[0]

        self.assertEqual(total_cons, total_cons_after_put)

        # Get another connection and verify we don't create a new one
        conn4 = pool.getconn()

        # conn4 should be either conn2 or conn3 (we don't care which)
        self.assertTrue(conn4 in (conn2, conn3))

        check_cursor.execute(SQL, (dbname, ))
        total_cons_after_get = check_cursor.fetchone()[0]

        self.assertEqual(total_cons_after_get, total_cons)

    def test_closeall(self):
        pool = psycopg2.pool.SimpleConnectionPool(0, 10, dsn, idle_timeout=30)
        conn1 = pool.getconn()
        conn2 = pool.getconn()
        pool.putconn(conn2)

        self.assertEqual(len(pool._pool), 1)  # 1 in use, 1 put back
        self.assertEqual(len(pool._used), 1)  # and we have one used connection

        # Both connections should be open at this point
        self.assertFalse(conn1.closed)
        self.assertFalse(conn2.closed)

        pool.closeall()

        # Make sure both connections are now closed
        self.assertTrue(conn1.closed)
        self.assertTrue(conn2.closed)

        # Apparently the closeall command doesn't actually empty _used or _pool,
        # it just blindly closes the connections. Fixit?
        # self.assertEqual(len(pool._used), 0)
        # self.assertEqual(len(pool._pool), 0)

        # To maintain consistancy with existing code, closeall doesn't mess with the _return_times dict either
        # self.assertEqual(len(pool._return_times), 0)

        # We should get an error if we try to put conn1 back now
        self.assertRaises(psycopg2.pool.PoolError, pool.putconn, conn1)


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)


if __name__ == "__main__":
    unittest.main()
