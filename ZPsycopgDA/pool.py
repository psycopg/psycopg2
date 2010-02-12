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
import psycopg2.pool

_connections_pool = {}
_connections_lock = threading.Lock()

def getpool(dsn, create=True):
    _connections_lock.acquire()
    try:
        if not _connections_pool.has_key(dsn) and create:
            _connections_pool[dsn] = \
                psycopg2.pool.PersistentConnectionPool(4, 200, dsn)
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
