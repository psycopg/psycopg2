# ZPsycopgDA/pool.py - ZPsycopgDA Zope product: connection pooling
#
# Copyright (C) 2004-2010 Federico Di Gregorio  <fog@debian.org>
#
# psycopg2 is free software: you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# psycopg2 is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.

# Import modules needed by _psycopg to allow tools like py2exe to do
# their work without bothering about the module dependencies.

# All the connections are held in a pool of pools, directly accessible by the
# ZPsycopgDA code in db.py.

import threading
import psycopg2
from psycopg2.pool import PoolError


class AbstractConnectionPool(object):
    """Generic key-based pooling code."""

    def __init__(self, minconn, maxconn, *args, **kwargs):
        """Initialize the connection pool.

        New 'minconn' connections are created immediately calling 'connfunc'
        with given parameters. The connection pool will support a maximum of
        about 'maxconn' connections.
        """
        self.minconn = minconn
        self.maxconn = maxconn
        self.closed = False

        self._args = args
        self._kwargs = kwargs

        self._pool = []
        self._used = {}
        self._rused = {} # id(conn) -> key map
        self._keys = 0

        for i in range(self.minconn):
            self._connect()

    def _connect(self, key=None):
        """Create a new connection and assign it to 'key' if not None."""
        conn = psycopg2.connect(*self._args, **self._kwargs)
        if key is not None:
            self._used[key] = conn
            self._rused[id(conn)] = key
        else:
            self._pool.append(conn)
        return conn

    def _getkey(self):
        """Return a new unique key."""
        self._keys += 1
        return self._keys

    def _getconn(self, key=None):
        """Get a free connection and assign it to 'key' if not None."""
        if self.closed: raise PoolError("connection pool is closed")
        if key is None: key = self._getkey()

        if key in self._used:
            return self._used[key]

        if self._pool:
            self._used[key] = conn = self._pool.pop()
            self._rused[id(conn)] = key
            return conn
        else:
            if len(self._used) == self.maxconn:
                raise PoolError("connection pool exausted")
            return self._connect(key)

    def _putconn(self, conn, key=None, close=False):
        """Put away a connection."""
        if self.closed: raise PoolError("connection pool is closed")
        if key is None: key = self._rused[id(conn)]

        if not key:
            raise PoolError("trying to put unkeyed connection")

        if len(self._pool) < self.minconn and not close:
            self._pool.append(conn)
        else:
            conn.close()

        # here we check for the presence of key because it can happen that a
        # thread tries to put back a connection after a call to close
        if not self.closed or key in self._used:
            del self._used[key]
            del self._rused[id(conn)]

    def _closeall(self):
        """Close all connections.

        Note that this can lead to some code fail badly when trying to use
        an already closed connection. If you call .closeall() make sure
        your code can deal with it.
        """
        if self.closed: raise PoolError("connection pool is closed")
        for conn in self._pool + list(self._used.values()):
            try:
                conn.close()
            except:
                pass
        self.closed = True


class PersistentConnectionPool(AbstractConnectionPool):
    """A pool that assigns persistent connections to different threads.

    Note that this connection pool generates by itself the required keys
    using the current thread id.  This means that until a thread puts away
    a connection it will always get the same connection object by successive
    `!getconn()` calls. This also means that a thread can't use more than one
    single connection from the pool.
    """

    def __init__(self, minconn, maxconn, *args, **kwargs):
        """Initialize the threading lock."""
        import threading
        AbstractConnectionPool.__init__(
            self, minconn, maxconn, *args, **kwargs)
        self._lock = threading.Lock()

        # we we'll need the thread module, to determine thread ids, so we
        # import it here and copy it in an instance variable
        import thread
        self.__thread = thread

    def getconn(self):
        """Generate thread id and return a connection."""
        key = self.__thread.get_ident()
        self._lock.acquire()
        try:
            return self._getconn(key)
        finally:
            self._lock.release()

    def putconn(self, conn=None, close=False):
        """Put away an unused connection."""
        key = self.__thread.get_ident()
        self._lock.acquire()
        try:
            if not conn: conn = self._used[key]
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


_connections_pool = {}
_connections_lock = threading.Lock()

def getpool(dsn, create=True):
    _connections_lock.acquire()
    try:
        if not _connections_pool.has_key(dsn) and create:
            _connections_pool[dsn] = \
                PersistentConnectionPool(4, 200, dsn)
    finally:
        _connections_lock.release()
    return _connections_pool[dsn]

def flushpool(dsn):
    _connections_lock.acquire()
    try:
        _connections_pool[dsn].closeall()
        del _connections_pool[dsn]
    finally:
        _connections_lock.release()

def getconn(dsn, create=True):
    return getpool(dsn, create=create).getconn()

def putconn(dsn, conn, close=False):
    getpool(dsn).putconn(conn, close=close)
