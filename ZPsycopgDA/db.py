# ZPsycopgDA/db.py - query execution
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

from Shared.DC.ZRDB.TM import TM
from Shared.DC.ZRDB import dbi_db

from ZODB.POSException import ConflictError

import site
import pool

import psycopg2
from psycopg2.extensions import INTEGER, LONGINTEGER, FLOAT, BOOLEAN, DATE, TIME
from psycopg2.extensions import register_type
from psycopg2 import NUMBER, STRING, ROWID, DATETIME 


# the DB object, managing all the real query work

class DB(TM, dbi_db.DB):
    
    _p_oid = _p_changed = _registered = None

    def __init__(self, dsn, tilevel, typecasts, enc='utf-8'):
        self.dsn = dsn
        self.tilevel = tilevel
        self.typecasts = typecasts
        self.encoding = enc
        self.failures = 0
        self.calls = 0
        self.make_mappings()
                        
    def getconn(self, create=True):
        conn = pool.getconn(self.dsn)
        conn.set_isolation_level(int(self.tilevel))
        conn.set_client_encoding(self.encoding)
        for tc in self.typecasts:
            register_type(tc, conn)
        return conn

    def putconn(self, close=False):
        try:
            conn = pool.getconn(self.dsn, False)
        except AttributeError:
            pass
        pool.putconn(self.dsn, conn, close)
        
    def getcursor(self):
        conn = self.getconn()
        return conn.cursor()

    def _finish(self, *ignored):
        try:
            conn = self.getconn(False)
            conn.commit()
            self.putconn()
        except AttributeError:
            pass
            
    def _abort(self, *ignored):
        try:
            conn = self.getconn(False)
            conn.rollback()
            self.putconn()
        except AttributeError:
            pass

    def open(self):
        # this will create a new pool for our DSN if not already existing,
        # then get and immediately release a connection
        self.getconn()
        self.putconn()
        
    def close(self):
        # FIXME: if this connection is closed we flush all the pool associated
        # with the current DSN; does this makes sense?
        pool.flushpool(self.dsn)

    def sortKey(self):
        return 1

    def make_mappings(self):
        """Generate the mappings used later by self.convert_description()."""
        self.type_mappings = {}
	for t, s in [(INTEGER,'i'), (LONGINTEGER, 'i'), (NUMBER, 'n'),  
	             (BOOLEAN,'n'), (ROWID, 'i'),
	             (DATETIME, 'd'), (DATE, 'd'), (TIME, 'd')]:
            for v in t.values:
	        self.type_mappings[v] = (t, s)

    def convert_description(self, desc, use_psycopg_types=False):
        """Convert DBAPI-2.0 description field to Zope format."""
        items = []
        for name, typ, width, ds, p, scale, null_ok in desc:
	    m = self.type_mappings.get(typ, (STRING, 's'))
            items.append({
                'name': name,
                'type': use_psycopg_types and m[0] or m[1],
                'width': width,
                'precision': p,
                'scale': scale,
                'null': null_ok,
                })
        return items

    ## tables and rows ##

    def tables(self, rdb=0, _care=('TABLE', 'VIEW')):
        self._register()
        c = self.getcursor()
        c.execute(
            "SELECT t.tablename AS NAME, 'TABLE' AS TYPE "
            "  FROM pg_tables t WHERE tableowner <> 'postgres' "
            "UNION SELECT v.viewname AS NAME, 'VIEW' AS TYPE "
            "  FROM pg_views v WHERE viewowner <> 'postgres' "
            "UNION SELECT t.tablename AS NAME, 'SYSTEM_TABLE\' AS TYPE "
            "  FROM pg_tables t WHERE tableowner = 'postgres' "
            "UNION SELECT v.viewname AS NAME, 'SYSTEM_TABLE' AS TYPE "
            "FROM pg_views v WHERE viewowner = 'postgres'")
        res = []
        for name, typ in c.fetchall():
            if typ in _care:
                res.append({'TABLE_NAME': name, 'TABLE_TYPE': typ})
        self.putconn()
        return res

    def columns(self, table_name):
        self._register()
        c = self.getcursor()
        try:
            r = c.execute('SELECT * FROM "%s" WHERE 1=0' % table_name)
        except:
            return ()
        self.putconn()
        return self.convert_description(c.description, True)
    
    ## query execution ##

    def query(self, query_string, max_rows=None, query_data=None):
        self._register()
        self.calls = self.calls+1

        desc = ()
        res = []
        nselects = 0

        c = self.getcursor()

        try:
            for qs in [x for x in query_string.split('\0') if x]:
                try:
                    if query_data:
                        c.execute(qs, query_data)
                    else:
                        c.execute(qs)
                except psycopg2.OperationalError, e:
                    try:
                        self.close()
                    except:
                        pass
                    self.open()
                    try:
                        if   query_data:
                            c.execute(qs, query_data)
                        else:
                            c.execute(qs)
                    except (psycopg2.ProgrammingError,
                            psycopg2.IntegrityError), e:
                        if e.args[0].find("concurrent update") > -1:
                            raise ConflictError
                        raise e
                except (psycopg2.ProgrammingError, psycopg2.IntegrityError), e:
                    if e.args[0].find("concurrent update") > -1:
                        raise ConflictError
                    raise e
                if c.description is not None:
                    nselects += 1
                    if c.description != desc and nselects > 1:
                        raise psycopg2.ProgrammingError(
                            'multiple selects in single query not allowed')
                    if max_rows:
                        res = c.fetchmany(max_rows)
                    else:
                        res = c.fetchall()
                    desc = c.description
            self.failures = 0

        except StandardError, err:
            self._abort()
            raise err
        
        return self.convert_description(desc), res
