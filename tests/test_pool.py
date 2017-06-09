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

from datetime import datetime, timedelta
import psycopg2
import psycopg2.pool as psycopg_pool
import psycopg2.extensions as _ext

from testutils import (unittest, ConnectingTestCase, skip_before_postgres,
    skip_if_no_namedtuple, skip_if_no_getrefcount, slow, skip_if_no_superuser,
    skip_if_windows)

from testconfig import dsn, dbname

class PoolTests(ConnectingTestCase):
    def test_caching_pool_get_conn(self):
        """Test the call to getconn. Should just return an open connection."""
        lifetime = 30
        pool = psycopg_pool.CachingConnectionPool(0, 1, lifetime, dsn)
        conn = pool.getconn()
        expected_expires = datetime.now() + timedelta(seconds = lifetime)

        #Verify we have one entry in the expiration table
        self.assertEqual(len(pool._expirations), 1)
        actual_expires = pool._expirations[id(conn)]

        # there may be some slight variation between when we created the connection
        # and our "expected" expiration.
        # Should be negligable, however
        self.assertAlmostEqual(expected_expires, actual_expires, delta = timedelta(seconds = 1))

        #make sure we got an open connection
        self.assertFalse(conn.closed)

        #Try again. We should get an error, since we only allowed one connection
        self.assertRaises(psycopg2.pool.PoolError, pool.getconn)

        # Put the connection back, then get it again. The expiration time should increment
        # If this test is consistantly failing, we may need to add a "sleep" to force
        # some real time between connections, but as long as the precision of
        # datetime is high enough, this should work. All we care is that new_expires
        # is greater than the original expiration time
        pool.putconn(conn)
        conn = pool.getconn()
        new_expires = pool._expirations[id(conn)]
        self.assertGreater(new_expires, actual_expires)

    def test_caching_pool_prune(self):
        """Test the prune function to make sure it closes conenctions and removes them from the pool"""
        pool = psycopg_pool.CachingConnectionPool(0, 3, 30, dsn)

        # Get a connection that we use, so it can't be pruned.
        sticky_conn = pool.getconn()
        self.assertFalse(sticky_conn in pool._pool)
        self.assertTrue(sticky_conn in pool._used.values())
        self.assertFalse(sticky_conn.closed)
        self.assertTrue(id(sticky_conn) in pool._expirations)

        # create a second connection that is put back into the pool, available to be pruned.
        conn = pool.getconn()

        # create a third connection that is put back into the pool, but won't be expired
        new_conn = pool.getconn()

        # Put the connections back in the pool.
        pool.putconn(conn)
        pool.putconn(new_conn)

        # Verify that everything is in the expected state
        self.assertTrue(conn in pool._pool)
        self.assertFalse(conn in pool._used.values())
        self.assertFalse(conn.closed)
        self.assertTrue(id(conn) in pool._expirations)

        self.assertTrue(new_conn in pool._pool)
        self.assertFalse(new_conn in pool._used.values())
        self.assertFalse(new_conn.closed)
        self.assertTrue(id(new_conn) in pool._expirations)

        self.assertNotEqual(conn, sticky_conn)
        self.assertNotEqual(new_conn, conn)

        #Make the connections expire a minute ago (but not new_con)
        old_expire = datetime.now() - timedelta(minutes = 1)

        pool._expirations[id(conn)] = old_expire
        pool._expirations[id(sticky_conn)] = old_expire

        #prune connections
        pool._prune()

        # make sure the unused expired connection is gone and closed,
        # but the used connection isn't
        self.assertFalse(conn in pool._pool)
        self.assertTrue(conn.closed)
        self.assertFalse(id(conn) in pool._expirations)

        self.assertFalse(sticky_conn.closed)
        self.assertTrue(id(sticky_conn) in pool._expirations)

        # The un-expired connection should still exist and be open
        self.assertFalse(new_conn.closed)
        self.assertTrue(id(new_conn) in pool._expirations)

    def test_caching_pool_prune_missing_connection(self):
        pool = psycopg_pool.CachingConnectionPool(0, 1, 30, dsn)
        conn = pool.getconn(key = "test")

        self.assertTrue("test" in pool._used)

        #connection got lost somehow.
        del pool._used["test"]

        #expire this connection
        old_expire = datetime.now() - timedelta(minutes = 1)

        pool._expirations[id(conn)] = old_expire

        # and prune
        pool._prune()

    def test_caching_pool_prune_below_min(self):
        pool = psycopg_pool.CachingConnectionPool(1, 1, 30, dsn)
        conn = pool.getconn()
        self.assertFalse(conn in pool._pool)

        #nothing in pool, since we only allow one connection here
        self.assertEqual(len(pool._pool), 0)

        pool.putconn(conn, close= True)

        # This connection should *not* be in the pool, since we said close
        self.assertFalse(conn in pool._pool)

        # But we should still have *a* connection available
        self.assertEqual(len(pool._pool), 1)


    def test_caching_pool_putconn_normal(self):
        pool = psycopg_pool.CachingConnectionPool(0, 1, 30, dsn)
        conn = pool.getconn()
        self.assertFalse(conn in pool._pool)

        pool.putconn(conn)
        self.assertTrue(conn in pool._pool)

    def test_caching_pool_putconn_closecon(self):
        pool = psycopg_pool.CachingConnectionPool(0, 1, 30, dsn)
        conn = pool.getconn()
        self.assertFalse(conn in pool._pool)

        pool.putconn(conn, close = True)
        self.assertFalse(conn in pool._pool)
        self.assertFalse(id(conn) in pool._expirations)

    def test_caching_pool_putconn_closecon_noexp(self):
        pool = psycopg_pool.CachingConnectionPool(0, 1, 30, dsn)
        conn = pool.getconn()
        self.assertFalse(conn in pool._pool)

        # Something went haywire with the prune, and the expiration information
        # for this connection got lost.
        del pool._expirations[id(conn)]
        self.assertFalse(id(conn) in pool._expirations)

        # Should still work without error
        pool.putconn(conn, close = True)
        self.assertFalse(conn in pool._pool)

    def test_caching_pool_putconn_expired(self):
        pool = psycopg_pool.CachingConnectionPool(0, 1, 30, dsn)
        conn = pool.getconn()

        #expire the connection
        pool._expirations[id(conn)] = datetime.now() - timedelta(minutes = 1)
        pool.putconn(conn)

        #connection should be discarded
        self.assertFalse(conn in pool._pool)
        self.assertFalse(id(conn) in pool._expirations)
        self.assertTrue(conn.closed)

    def test_caching_pool_putconn_unkeyed(self):
        pool = psycopg_pool.CachingConnectionPool(0, 1, 30, dsn)

        #Test put with empty key
        conn = pool.getconn()
        self.assertRaises(psycopg_pool.PoolError, pool.putconn, conn, '')

    def test_caching_pool_putconn_errorState(self):
        pool = psycopg_pool.CachingConnectionPool(0, 1, 30, dsn)
        conn = pool.getconn()

        #Get connection into transaction state
        cursor = conn.cursor()
        try:
            cursor.execute("INSERT INTO test (id) VALUES (1)")  #table does not exist, error
        except:
            pass

        self.assertNotEqual(conn.get_transaction_status(),
                            _ext.TRANSACTION_STATUS_IDLE)

        pool.putconn(conn)

        # make sure we got back into the pool and are now showing idle
        self.assertEqual(conn.get_transaction_status(),
                         _ext.TRANSACTION_STATUS_IDLE)
        self.assertTrue(conn in pool._pool)

    def test_caching_pool_putconn_closed(self):
        pool = psycopg_pool.CachingConnectionPool(0, 1, 30, dsn)
        conn = pool.getconn()

        #Open connection with expiration
        self.assertFalse(conn.closed)
        self.assertTrue(id(conn) in pool._expirations)

        conn.close()

        # Now should be closed, but still have expiration entry
        self.assertTrue(conn.closed)
        self.assertTrue(id(conn) in pool._expirations)

        pool.putconn(conn)

        # we should not have an expiration any more
        self.assertFalse(id(conn) in pool._expirations)

        # and the connection should have been discarded
        self.assertFalse(conn in pool._pool)

    def test_caching_pool_putconn_closed_noexp(self):
        pool = psycopg_pool.CachingConnectionPool(0, 1, 30, dsn)
        conn = pool.getconn()

        #Open connection with expiration
        self.assertFalse(conn.closed)
        self.assertTrue(id(conn) in pool._expirations)

        conn.close()

        # Now should be closed, but still have expiration entry
        self.assertTrue(conn.closed)
        self.assertTrue(id(conn) in pool._expirations)

        # Delete the expiration entry to simulate confusion
        del pool._expirations[id(conn)]

        # we should not have an expiration any more
        self.assertFalse(id(conn) in pool._expirations)

        pool.putconn(conn)

        # and the connection should have been discarded, without error
        self.assertFalse(conn in pool._pool)

    def test_caching_pool_caching(self):
        pool = psycopg_pool.CachingConnectionPool(0, 10, 30, dsn)

        # Get a connection to use to check the number of connections
        check_conn = pool.getconn()
        check_conn.autocommit = True  # Being in a transaction hides the other connections.
        # Get a cursor to run check queries with
        check_cursor = check_conn.cursor()

        SQL = "SELECT numbackends from pg_stat_database WHERE datname=%s"
        check_cursor.execute(SQL, (dbname, ))

        #not trying to test anything yet, so hopefully this always works :)
        starting_conns = check_cursor.fetchone()[0]

        #Get a couple more connections
        conn2 = pool.getconn()
        conn3 = pool.getconn()

        self.assertEqual(len(pool._expirations), 3)

        self.assertNotEqual(conn2, conn3)

        # Verify that we have the expected number of connections to the DB server now
        check_cursor.execute(SQL, (dbname, ))
        total_cons = check_cursor.fetchone()[0]

        self.assertEqual(total_cons, starting_conns + 2)

        #Put the connections back in the pool and verify they don't close
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

    def test_caching_pool_closeall(self):
        pool = psycopg_pool.CachingConnectionPool(0, 10, 30, dsn)
        conn1 = pool.getconn()
        conn2 = pool.getconn()
        pool.putconn(conn2)

        self.assertEqual(len(pool._pool), 1)  #1 in use, 1 put back
        self.assertEqual(len(pool._expirations), 2)  # We have two expirations for two connections
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

        # To maintain consistancy with existing code, closeall doesn't mess with the _expirations dict either
        # self.assertEqual(len(pool._expirations), 0)

        #We should get an error if we try to put conn1 back now
        self.assertRaises(psycopg2.pool.PoolError, pool.putconn, conn1)
