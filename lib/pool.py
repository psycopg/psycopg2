"""Connection pooling for psycopg

This module implements thread-safe (and not) connection pools.
"""
# psycopg/pool.py - pooling code for psycopg
#
# Copyright (C) 2003-2004 Federico Di Gregorio  <fog@debian.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTIBILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.

import psycopg

try:
    from zLOG import LOG, DEBUG, INFO
    def dbg(*args):
        LOG('ZPsycopgDA',  DEBUG, "",
            ' '.join([str(x) for x in args])+'\n')
    LOG('ZPsycopgDA', INFO, "Installed", "Logging using Zope's zLOG\n") 
except:
    import sys
    def dbg(*args):
        sys.stderr.write(' '.join(args)+'\n')



class PoolError(psycopg.Error):
    pass



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
        self._keys = 0

        for i in range(self.minconn):
            self._connect()

    def _connect(self, key=None):
        """Create a new connection and assign it to 'key' if not None."""
        conn = psycopg.connect(*self._args, **self._kwargs)
        if key is not None:
            self._used[key] = conn
        else:
            self._pool.append(conn)
        return conn

    def _getkey(self):
        """Return a new unique key."""
        self._keys += 1
        return self._keys

    def _findkey(self, conn):
        """Return the key associated with a connection or None."""
        for o, k in self._used.items():
            if o == conn:
                return k
            
    def _getconn(self, key=None):
        """Get a free connection and assign it to 'key' if not None."""
        if self.closed: raise PoolError("connection pool is closed")
        if key is None: key = self._getkey()

        if not self._used.has_key(key):
            if not self._pool:
                if len(self._used) == self.maxconn:
                    raise PoolError("connection pool exausted")
                return self._connect(key)
            else:
                self._used[key] = self._pool.pop()
        return self._used[key]

    def _putconn(self, conn, key=None, close=False):
        """Put away a connection."""
        if self.closed: raise PoolError("connection pool is closed")
        if key is None: key = self._findkey(conn)

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

    def _closeall(self):
        """Close all connections.

        Note that this can lead to some code fail badly when trying to use
        an already closed connection. If you call .closeall() make sure
        your code can deal with it.
        """
        if self.closed: raise PoolError("connection pool is closed")
        for conn in self._pool + list(self._used.values()):
            try:
                print "Closing connection", conn
                conn.close()
            except:
                pass
        self.closed = True
        


class SimpleConnectionPool(AbstractConnectionPool):
    """A connection pool that can't be shared across different threads."""

    getconn = AbstractConnectionPool._getconn
    putconn = AbstractConnectionPool._putconn
    closeall   = AbstractConnectionPool._closeall



class ThreadedConnectionPool(AbstractConnectionPool):
    """A connection pool that works with the threading module.

    Note that this connection pool generates by itself the required keys
    using the current thread id.  This means that untill a thread put away
    a connection it will always get the same connection object by successive
    .getconn() calls.
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
        """Close all connections (even the one currently in use."""
        self._lock.acquire()
        try:
            self._closeall()
        finally:
            self._lock.release()
