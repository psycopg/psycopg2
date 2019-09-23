"""Connection pooling for psycopg2

This module implements thread-safe (and not) connection pools.
"""
# psycopg/pool.py - pooling code for psycopg
#
# Copyright (C) 2003-2019 Federico Di Gregorio  <fog@debian.org>
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

try:
    from time import process_time
except ImportError:
    # For Python < 3.3
    from time import clock as process_time

import psycopg2
from psycopg2 import extensions as _ext


class PoolError(psycopg2.Error):
    pass


class AbstractConnectionPool(object):
    """Generic key-based pooling code."""

    def __init__(self, minconn, maxconn, *args, **kwargs):
        """Initialize the connection pool.

        New 'minconn' connections are created immediately calling 'connfunc'
        with given parameters. The connection pool will support a maximum of
        about 'maxconn' connections.
        """
        self.minconn = int(minconn)
        self.maxconn = int(maxconn)
        self.idle_timeout = kwargs.pop('idle_timeout', 0)
        self.closed = False

        self._args = args
        self._kwargs = kwargs

        self._pool = []
        self._used = {}
        self._rused = {}    # id(conn) -> key map
        self._keys = 0
        self._return_times = {}

        for i in range(self.minconn):
            self._connect()

    def _connect(self, key=None):
        """Create a new connection and assign it to 'key' if not None."""
        conn = psycopg2.connect(*self._args, **self._kwargs)
        if key is not None:
            self._used[key] = conn
            self._rused[id(conn)] = key
        else:
            self._return_times[id(conn)] = process_time()
            self._pool.append(conn)
        return conn

    def _getkey(self):
        """Return a new unique key."""
        self._keys += 1
        return self._keys

    def _getconn(self, key=None):
        """Get a free connection and assign it to 'key' if not None."""
        if self.closed:
            raise PoolError("connection pool is closed")
        if key is None:
            key = self._getkey()

        if key in self._used:
            return self._used[key]

        while True:
            try:
                conn = self._pool.pop()
            except IndexError:
                if len(self._used) >= self.maxconn:
                    raise PoolError("connection pool exhausted")
                conn = self._connect(key)
            else:
                idle_since = self._return_times.pop(id(conn), 0)
                close = (
                    conn.info.transaction_status != _ext.TRANSACTION_STATUS_IDLE or
                    self.idle_timeout and idle_since < (process_time() - self.idle_timeout)
                )
                if close:
                    conn.close()
                    continue
            break
        self._used[key] = conn
        self._rused[id(conn)] = key
        return conn

    def _putconn(self, conn, key=None, close=False):
        """Put away a connection."""
        if self.closed:
            raise PoolError("connection pool is closed")

        if key is None:
            key = self._rused.get(id(conn))
            if key is None:
                raise PoolError("trying to put unkeyed connection")

        if close or self.idle_timeout == 0 and len(self._pool) >= self.minconn:
            conn.close()
        else:
            # Return the connection into a consistent state before putting
            # it back into the pool
            if not conn.closed:
                status = conn.info.transaction_status
                if status == _ext.TRANSACTION_STATUS_UNKNOWN:
                    # server connection lost
                    conn.close()
                elif status != _ext.TRANSACTION_STATUS_IDLE:
                    # connection in error or in transaction
                    conn.rollback()
                    self._return_times[id(conn)] = process_time()
                    self._pool.append(conn)
                else:
                    # regular idle connection
                    self._return_times[id(conn)] = process_time()
                    self._pool.append(conn)
            # If the connection is closed, we just discard it.

        # here we check for the presence of key because it can happen that a
        # thread tries to put back a connection after a call to close
        if not self.closed or key in self._used:
            del self._used[key]
            del self._rused[id(conn)]

        # Open new connections if we've dropped below minconn.
        while (len(self._pool) + len(self._used)) < self.minconn:
            self._connect()

    def _closeall(self):
        """Close all connections.

        Note that this can lead to some code fail badly when trying to use
        an already closed connection. If you call .closeall() make sure
        your code can deal with it.
        """
        if self.closed:
            raise PoolError("connection pool is closed")
        for conn in self._pool + list(self._used.values()):
            try:
                conn.close()
            except Exception:
                pass
        self.closed = True

    def _prune(self):
        """Drop all expired connections from the pool."""
        if self.idle_timeout is None:
            return
        threshold = process_time() - self.idle_timeout
        for conn in list(self._pool):
            if self._return_times.get(id(conn), 0) < threshold:
                try:
                    self._pool.remove(conn)
                except ValueError:
                    continue
                self._return_times.pop(id(conn), None)
                conn.close()


class SimpleConnectionPool(AbstractConnectionPool):
    """A connection pool that can't be shared across different threads."""

    getconn = AbstractConnectionPool._getconn
    putconn = AbstractConnectionPool._putconn
    closeall = AbstractConnectionPool._closeall
    prune = AbstractConnectionPool._prune


class ThreadedConnectionPool(AbstractConnectionPool):
    """A connection pool that works with the threading module."""

    def __init__(self, minconn, maxconn, *args, **kwargs):
        """Initialize the threading lock."""
        import threading
        AbstractConnectionPool.__init__(
            self, minconn, maxconn, *args, **kwargs)
        self._lock = threading.Lock()

    def getconn(self, key=None):
        """Get a free connection and assign it to 'key' if not None."""
        self._lock.acquire()
        try:
            return self._getconn(key)
        finally:
            self._lock.release()

    def putconn(self, conn=None, key=None, close=False):
        """Put away an unused connection."""
        self._lock.acquire()
        try:
            self._putconn(conn, key, close)
        finally:
            self._lock.release()

    def closeall(self):
        """Close all connections (even the one currently in use.)"""
        self._lock.acquire()
        try:
            self._closeall()
        finally:
            self._lock.release()

    def prune(self):
        """Drop all expired connections from the pool."""
        self._lock.acquire()
        try:
            self._prune()
        finally:
            self._lock.release()
