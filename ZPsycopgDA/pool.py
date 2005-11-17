# ZPsycopgDA/pool.py - ZPsycopgDA Zope product: connection pooling
#
# Copyright (C) 2004 Federico Di Gregorio <fog@initd.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2, or (at your option) any later
# version.
#
# Or, at your option this program (ZPsycopgDA) can be distributed under the
# Zope Public License (ZPL) Version 1.0, as published on the Zope web site,
# http://www.zope.org/Resources/ZPL.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTIBILITY
# or FITNESS FOR A PARTICULAR PURPOSE.
#
# See the LICENSE file for details.

# all the connections are held in a pool of pools, directly accessible by the
# ZPsycopgDA code in db.py

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
