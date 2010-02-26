"""Miscellaneous goodies for psycopg2

This module is a generic place used to hold little helper functions
and classes untill a better place in the distribution is found.
"""
# psycopg/extras.py - miscellaneous extra goodies for psycopg
#
# Copyright (C) 2003-2010 Federico Di Gregorio  <fog@debian.org>
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

import os
import time
import re as regex

try:
    import logging
except:
    logging = None

from psycopg2 import DATETIME, DataError
from psycopg2 import extensions as _ext
from psycopg2.extensions import cursor as _cursor
from psycopg2.extensions import connection as _connection
from psycopg2.extensions import adapt as _A


class DictCursorBase(_cursor):
    """Base class for all dict-like cursors."""

    def __init__(self, *args, **kwargs):
        if kwargs.has_key('row_factory'):
            row_factory = kwargs['row_factory']
            del kwargs['row_factory']
        else:
            raise NotImplementedError(
                "DictCursorBase can't be instantiated without a row factory.")
        _cursor.__init__(self, *args, **kwargs)
        self._query_executed = 0
        self._prefetch = 0
        self.row_factory = row_factory

    def fetchone(self):
        if self._prefetch:
            res = _cursor.fetchone(self)
        if self._query_executed:
            self._build_index()
        if not self._prefetch:
            res = _cursor.fetchone(self)
        return res

    def fetchmany(self, size=None):
        if self._prefetch:
            res = _cursor.fetchmany(self, size)
        if self._query_executed:
            self._build_index()
        if not self._prefetch:
            res = _cursor.fetchmany(self, size)
        return res

    def fetchall(self):
        if self._prefetch:
            res = _cursor.fetchall(self)
        if self._query_executed:
            self._build_index()
        if not self._prefetch:
            res = _cursor.fetchall(self)
        return res

    def next(self):
        if self._prefetch:
            res = _cursor.fetchone(self)
            if res is None:
                raise StopIteration()
        if self._query_executed:
            self._build_index()
        if not self._prefetch:
            res = _cursor.fetchone(self)
            if res is None:
                raise StopIteration()
        return res

class DictConnection(_connection):
    """A connection that uses `DictCursor` automatically."""
    def cursor(self, name=None):
        if name is None:
            return _connection.cursor(self, cursor_factory=DictCursor)
        else:
            return _connection.cursor(self, name, cursor_factory=DictCursor)

class DictCursor(DictCursorBase):
    """A cursor that keeps a list of column name -> index mappings."""

    def __init__(self, *args, **kwargs):
        kwargs['row_factory'] = DictRow
        DictCursorBase.__init__(self, *args, **kwargs)
        self._prefetch = 1

    def execute(self, query, vars=None, async=0):
        self.index = {}
        self._query_executed = 1
        return _cursor.execute(self, query, vars, async)

    def callproc(self, procname, vars=None):
        self.index = {}
        self._query_executed = 1
        return _cursor.callproc(self, procname, vars)

    def _build_index(self):
        if self._query_executed == 1 and self.description:
            for i in range(len(self.description)):
                self.index[self.description[i][0]] = i
            self._query_executed = 0

class DictRow(list):
    """A row object that allow by-colmun-name access to data."""

    __slots__ = ('_index',)

    def __init__(self, cursor):
        self._index = cursor.index
        self[:] = [None] * len(cursor.description)

    def __getitem__(self, x):
        if type(x) != int:
            x = self._index[x]
        return list.__getitem__(self, x)

    def items(self):
        res = []
        for n, v in self._index.items():
            res.append((n, list.__getitem__(self, v)))
        return res

    def keys(self):
        return self._index.keys()

    def values(self):
        return tuple(self[:])

    def has_key(self, x):
        return self._index.has_key(x)

    def get(self, x, default=None):
        try:
            return self[x]
        except:
            return default

    def iteritems(self):
        for n, v in self._index.items():
            yield n, list.__getitem__(self, v)

    def iterkeys(self):
        return self._index.iterkeys()

    def itervalues(self):
        return list.__iter__(self)

    def copy(self):
        return dict(self.items())

    def __contains__(self, x):
        return self._index.__contains__(x)

class RealDictConnection(_connection):
    """A connection that uses `RealDictCursor` automatically."""
    def cursor(self, name=None):
        if name is None:
            return _connection.cursor(self, cursor_factory=RealDictCursor)
        else:
            return _connection.cursor(self, name, cursor_factory=RealDictCursor)

class RealDictCursor(DictCursorBase):
    """A cursor that uses a real dict as the base type for rows.

    Note that this cursor is extremely specialized and does not allow
    the normal access (using integer indices) to fetched data. If you need
    to access database rows both as a dictionary and a list, then use
    the generic `DictCursor` instead of `!RealDictCursor`.
    """

    def __init__(self, *args, **kwargs):
        kwargs['row_factory'] = RealDictRow
        DictCursorBase.__init__(self, *args, **kwargs)
        self._prefetch = 0

    def execute(self, query, vars=None, async=0):
        self.column_mapping = []
        self._query_executed = 1
        return _cursor.execute(self, query, vars, async)

    def callproc(self, procname, vars=None):
        self.column_mapping = []
        self._query_executed = 1
        return _cursor.callproc(self, procname, vars)

    def _build_index(self):
        if self._query_executed == 1 and self.description:
            for i in range(len(self.description)):
                self.column_mapping.append(self.description[i][0])
            self._query_executed = 0

class RealDictRow(dict):
    """A ``dict`` subclass representing a data record."""

    __slots__ = ('_column_mapping')

    def __init__(self, cursor):
        dict.__init__(self)
        self._column_mapping = cursor.column_mapping

    def __setitem__(self, name, value):
        if type(name) == int:
            name = self._column_mapping[name]
        return dict.__setitem__(self, name, value)


class LoggingConnection(_connection):
    """A connection that logs all queries to a file or logger__ object.

    .. __: http://docs.python.org/library/logging.html
    """

    def initialize(self, logobj):
        """Initialize the connection to log to ``logobj``.

        The ``logobj`` parameter can be an open file object or a Logger
        instance from the standard logging module.
        """
        self._logobj = logobj
        if logging and isinstance(logobj, logging.Logger):
            self.log = self._logtologger
        else:
            self.log = self._logtofile
    
    def filter(self, msg, curs):
        """Filter the query before logging it.

        This is the method to overwrite to filter unwanted queries out of the
        log or to add some extra data to the output. The default implementation
        just does nothing.
        """
        return msg
    
    def _logtofile(self, msg, curs):
        msg = self.filter(msg, curs)
        if msg: self._logobj.write(msg + os.linesep)
        
    def _logtologger(self, msg, curs):
        msg = self.filter(msg, curs)
        if msg: self._logobj.debug(msg)
    
    def _check(self):
        if not hasattr(self, '_logobj'):
            raise self.ProgrammingError(
                "LoggingConnection object has not been initialize()d")
            
    def cursor(self, name=None):
        self._check()
        if name is None:
            return _connection.cursor(self, cursor_factory=LoggingCursor)
        else:
            return _connection.cursor(self, name, cursor_factory=LoggingCursor)

class LoggingCursor(_cursor):
    """A cursor that logs queries using its connection logging facilities."""

    def execute(self, query, vars=None, async=0):
        try:
            return _cursor.execute(self, query, vars, async)
        finally:
            self.connection.log(self.query, self)

    def callproc(self, procname, vars=None):
        try:
            return _cursor.callproc(self, procname, vars)  
        finally:
            self.connection.log(self.query, self)


class MinTimeLoggingConnection(LoggingConnection):
    """A connection that logs queries based on execution time.
    
    This is just an example of how to sub-class `LoggingConnection` to
    provide some extra filtering for the logged queries. Both the
    `inizialize()` and `filter()` methods are overwritten to make sure
    that only queries executing for more than ``mintime`` ms are logged.
    
    Note that this connection uses the specialized cursor
    `MinTimeLoggingCursor`.
    """
    def initialize(self, logobj, mintime=0):
        LoggingConnection.initialize(self, logobj)
        self._mintime = mintime

    def filter(self, msg, curs):
        t = (time.time() - curs.timestamp) * 1000
        if t > self._mintime:
            return msg + os.linesep + "  (execution time: %d ms)" % t

    def cursor(self, name=None):
        self._check()
        if name is None:
            return _connection.cursor(self, cursor_factory=MinTimeLoggingCursor)
        else:
            return _connection.cursor(self, name, cursor_factory=MinTimeLoggingCursor)
    
class MinTimeLoggingCursor(LoggingCursor):
    """The cursor sub-class companion to `MinTimeLoggingConnection`."""

    def execute(self, query, vars=None, async=0):
        self.timestamp = time.time()
        return LoggingCursor.execute(self, query, vars, async)
    
    def callproc(self, procname, vars=None):
        self.timestamp = time.time()
        return LoggingCursor.execute(self, procname, vars)


# a dbtype and adapter for Python UUID type

try:
    import uuid

    class UUID_adapter(object):
        """Adapt Python's uuid.UUID__ type to PostgreSQL's uuid__.

        .. __: http://docs.python.org/library/uuid.html
        .. __: http://www.postgresql.org/docs/8.4/static/datatype-uuid.html
        """
        
        def __init__(self, uuid):
            self._uuid = uuid
    
        def prepare(self, conn):
            pass
        
        def getquoted(self):
            return "'"+str(self._uuid)+"'::uuid"
            
        __str__ = getquoted

    def register_uuid(oids=None, conn_or_curs=None):
        """Create the UUID type and an uuid.UUID adapter."""
        if not oids:
            oid1 = 2950
            oid2 = 2951
        elif type(oids) == list:
            oid1, oid2 = oids
        else:
            oid1 = oids
            oid2 = 2951

        def parseUUIDARRAY(data, cursor):
            if data is None:
                return None
            elif data == '{}':
                return []
            else:
                return [((len(x) > 0 and x != 'NULL') and uuid.UUID(x) or None)
                        for x in data[1:-1].split(',')]

        _ext.UUID = _ext.new_type((oid1, ), "UUID",
                lambda data, cursor: data and uuid.UUID(data) or None)
        _ext.UUIDARRAY = _ext.new_type((oid2,), "UUID[]", parseUUIDARRAY)

        _ext.register_type(_ext.UUID, conn_or_curs)
        _ext.register_type(_ext.UUIDARRAY, conn_or_curs)
        _ext.register_adapter(uuid.UUID, UUID_adapter)

        return _ext.UUID

except ImportError, e:
    def register_uuid(oid=None):
        """Create the UUID type and an uuid.UUID adapter.

        This is a fake function that will always raise an error because the
        import of the uuid module failed.
        """
        raise e


# a type, dbtype and adapter for PostgreSQL inet type

class Inet(object):
    """Wrap a string to allow for correct SQL-quoting of inet values.

    Note that this adapter does NOT check the passed value to make
    sure it really is an inet-compatible address but DOES call adapt()
    on it to make sure it is impossible to execute an SQL-injection
    by passing an evil value to the initializer.
    """
    def __init__(self, addr):
        self.addr = addr
    
    def __repr__(self):
        return "%s(%r)" % (self.__class__.__name__, self.addr)

    def prepare(self, conn):
        self._conn = conn
    
    def getquoted(self):
        obj = _A(self.addr)
        if hasattr(obj, 'prepare'):
            obj.prepare(self._conn)
        return obj.getquoted()+"::inet"

    def __str__(self):
        return str(self.addr)
        
def register_inet(oid=None, conn_or_curs=None):
    """Create the INET type and an Inet adapter."""
    if not oid: oid = 869
    _ext.INET = _ext.new_type((oid, ), "INET",
            lambda data, cursor: data and Inet(data) or None)
    _ext.register_type(_ext.INET, conn_or_curs)
    _ext.register_adapter(Inet, lambda x: x)
    return _ext.INET


# safe management of times with a non-standard time zone

def _convert_tstz_w_secs(s, cursor):
    try:
        return DATETIME(s, cursor)
        
    except (DataError,), exc:
        if exc.message != "unable to parse time":
                raise

        if regex.match('(\+|-)\d\d:\d\d:\d\d', s[-9:]) is None:
                raise

        # parsing doesn't succeed even if seconds are ":00" so truncate in
        # any case
        return DATETIME(s[:-3], cursor)

def register_tstz_w_secs(oids=None, conn_or_curs=None):
    """Register alternate type caster for :sql:`TIMESTAMP WITH TIME ZONE`.

    The Python datetime module cannot handle time zones with
    seconds in the UTC offset. There are, however, historical
    "time zones" which contain such offsets, eg. "Asia/Calcutta".
    In many cases those offsets represent true local time.

    If you encounter "unable to parse time" on a perfectly valid
    timestamp you likely want to try this type caster. It truncates
    the seconds from the time zone data and retries casting
    the timestamp. Note that this will generate timestamps
    which are **inaccurate** by the number of seconds truncated
    (unless the seconds were 00).

    :param oids:
            which OIDs to use this type caster for,
            defaults to :sql:`TIMESTAMP WITH TIME ZONE`
    :param conn_or_curs:
            a cursor or connection if you want to attach
            this type caster to that only, defaults to
            ``None`` meaning all connections and cursors
    """
    if oids is None:
        oids = (1184,) # hardcoded from PostgreSQL headers

    _ext.TSTZ_W_SECS = _ext.new_type(oids, 'TSTZ_W_SECS', _convert_tstz_w_secs)
    _ext.register_type(_ext.TSTZ_W_SECS, conn_or_curs)

    return _ext.TSTZ_W_SECS


__all__ = filter(lambda k: not k.startswith('_'), locals().keys())
