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
import codecs
import warnings
import re as regex

try:
    import logging
except:
    logging = None

import psycopg2
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

    def execute(self, query, vars=None):
        self.index = {}
        self._query_executed = 1
        return _cursor.execute(self, query, vars)

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

    def __setitem__(self, x, v):
        if type(x) != int:
            x = self._index[x]
        list.__setitem__(self, x, v)

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

    def execute(self, query, vars=None):
        self.column_mapping = []
        self._query_executed = 1
        return _cursor.execute(self, query, vars)

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


class NamedTupleConnection(_connection):
    """A connection that uses `NamedTupleCursor` automatically."""
    def cursor(self, *args, **kwargs):
        kwargs['cursor_factory'] = NamedTupleCursor
        return _connection.cursor(self, *args, **kwargs)

class NamedTupleCursor(_cursor):
    """A cursor that generates results as |namedtuple|__.

    `!fetch*()` methods will return named tuples instead of regular tuples, so
    their elements can be accessed both as regular numeric items as well as
    attributes.

        >>> nt_cur = conn.cursor(cursor_factory=psycopg2.extras.NamedTupleCursor)
        >>> rec = nt_cur.fetchone()
        >>> rec
        Record(id=1, num=100, data="abc'def")
        >>> rec[1]
        100
        >>> rec.data
        "abc'def"

    .. |namedtuple| replace:: `!namedtuple`
    .. __: http://docs.python.org/release/2.6/library/collections.html#collections.namedtuple
    """
    Record = None

    def execute(self, query, vars=None):
        self.Record = None
        return _cursor.execute(self, query, vars)

    def executemany(self, query, vars):
        self.Record = None
        return _cursor.executemany(self, vars)

    def callproc(self, procname, vars=None):
        self.Record = None
        return _cursor.callproc(self, procname, vars)

    def fetchone(self):
        t = _cursor.fetchone(self)
        if t is not None:
            nt = self.Record
            if nt is None:
                nt = self.Record = self._make_nt()
            return nt(*t)

    def fetchmany(self, size=None):
        nt = self.Record
        if nt is None:
            nt = self.Record = self._make_nt()
        ts = _cursor.fetchmany(self, size)
        return [nt(*t) for t in ts]

    def fetchall(self):
        nt = self.Record
        if nt is None:
            nt = self.Record = self._make_nt()
        ts = _cursor.fetchall(self)
        return [nt(*t) for t in ts]

    def __iter__(self):
        return iter(self.fetchall())

    try:
        from collections import namedtuple
    except ImportError, _exc:
        def _make_nt(self):
            raise self._exc
    else:
        def _make_nt(self, namedtuple=namedtuple):
            return namedtuple("Record", [d[0] for d in self.description or ()])


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

    def execute(self, query, vars=None):
        try:
            return _cursor.execute(self, query, vars)
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

    def execute(self, query, vars=None):
        self.timestamp = time.time()
        return LoggingCursor.execute(self, query, vars)
    
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

    def __conform__(self, foo):
        if foo is _ext.ISQLQuote:
            return self

    def __str__(self):
        return str(self.addr)
        
def register_inet(oid=None, conn_or_curs=None):
    """Create the INET type and an Inet adapter."""
    if not oid: oid = 869
    _ext.INET = _ext.new_type((oid, ), "INET",
            lambda data, cursor: data and Inet(data) or None)
    _ext.register_type(_ext.INET, conn_or_curs)
    return _ext.INET


def register_tstz_w_secs(oids=None, conn_or_curs=None):
    """The function used to register an alternate type caster for
    :sql:`TIMESTAMP WITH TIME ZONE` to deal with historical time zones with
    seconds in the UTC offset.

    These are now correctly handled by the default type caster, so currently
    the function doesn't do anything.
    """
    warnings.warn("deprecated", DeprecationWarning)


import select
from psycopg2.extensions import POLL_OK, POLL_READ, POLL_WRITE
from psycopg2 import OperationalError

def wait_select(conn):
    """Wait until a connection or cursor has data available.

    The function is an example of a wait callback to be registered with
    `~psycopg2.extensions.set_wait_callback()`. This function uses `!select()`
    to wait for data available.
    """
    while 1:
        state = conn.poll()
        if state == POLL_OK:
            break
        elif state == POLL_READ:
            select.select([conn.fileno()], [], [])
        elif state == POLL_WRITE:
            select.select([], [conn.fileno()], [])
        else:
            raise OperationalError("bad state from poll: %s" % state)


class HstoreAdapter(object):
    """Adapt a Python dict to the hstore syntax."""
    def __init__(self, wrapped):
        self.wrapped = wrapped

    def prepare(self, conn):
        self.conn = conn

        # use an old-style getquoted implementation if required
        if conn.server_version < 90000:
            self.getquoted = self._getquoted_8

    def _getquoted_8(self):
        """Use the operators available in PG pre-9.0."""
        if not self.wrapped:
            return "''::hstore"

        adapt = _ext.adapt
        rv = []
        for k, v in self.wrapped.iteritems():
            k = adapt(k)
            k.prepare(self.conn)
            k = k.getquoted()

            if v is not None:
                v = adapt(v)
                v.prepare(self.conn)
                v = v.getquoted()
            else:
                v = 'NULL'

            rv.append("(%s => %s)" % (k, v))

        return "(" + '||'.join(rv) + ")"

    def _getquoted_9(self):
        """Use the hstore(text[], text[]) function."""
        if not self.wrapped:
            return "''::hstore"

        k = _ext.adapt(self.wrapped.keys())
        k.prepare(self.conn)
        v = _ext.adapt(self.wrapped.values())
        v.prepare(self.conn)
        return "hstore(%s, %s)" % (k.getquoted(), v.getquoted())

    getquoted = _getquoted_9

    _re_hstore = regex.compile(r"""
        # hstore key:
        # a string of normal or escaped chars
        "((?: [^"\\] | \\. )*)"
        \s*=>\s* # hstore value
        (?:
            NULL # the value can be null - not catched
            # or a quoted string like the key
            | "((?: [^"\\] | \\. )*)"
        )
        (?:\s*,\s*|$) # pairs separated by comma or end of string.
    """, regex.VERBOSE)

    # backslash decoder
    _bsdec = codecs.getdecoder("string_escape")

    def parse(self, s, cur, _decoder=_bsdec):
        """Parse an hstore representation in a Python string.

        The hstore is represented as something like::

            "a"=>"1", "b"=>"2"

        with backslash-escaped strings.
        """
        if s is None:
            return None

        rv = {}
        start = 0
        for m in self._re_hstore.finditer(s):
            if m is None or m.start() != start:
                raise psycopg2.InterfaceError(
                    "error parsing hstore pair at char %d" % start)
            k = _decoder(m.group(1))[0]
            v = m.group(2)
            if v is not None:
                v = _decoder(v)[0]

            rv[k] = v
            start = m.end()

        if start < len(s):
            raise psycopg2.InterfaceError(
                "error parsing hstore: unparsed data after char %d" % start)

        return rv

    parse = classmethod(parse)

    def parse_unicode(self, s, cur):
        """Parse an hstore returning unicode keys and values."""
        codec = codecs.getdecoder(_ext.encodings[cur.connection.encoding])
        bsdec = self._bsdec
        decoder = lambda s: codec(bsdec(s)[0])
        return self.parse(s, cur, _decoder=decoder)

    parse_unicode = classmethod(parse_unicode)

    @classmethod
    def get_oids(self, conn_or_curs):
        """Return the oid of the hstore and hstore[] types.

        Return None if hstore is not available.
        """
        if hasattr(conn_or_curs, 'execute'):
            conn = conn_or_curs.connection
            curs = conn_or_curs
        else:
            conn = conn_or_curs
            curs = conn_or_curs.cursor()

        # Store the transaction status of the connection to revert it after use
        conn_status = conn.status

        # column typarray not available before PG 8.3
        typarray = conn.server_version >= 80300 and "typarray" or "NULL"

        # get the oid for the hstore
        curs.execute("""\
SELECT t.oid, %s
FROM pg_type t JOIN pg_namespace ns
    ON typnamespace = ns.oid
WHERE typname = 'hstore' and nspname = 'public';
""" % typarray)
        oids = curs.fetchone()

        # revert the status of the connection as before the command
        if (conn_status != _ext.STATUS_IN_TRANSACTION
        and conn.isolation_level != _ext.ISOLATION_LEVEL_AUTOCOMMIT):
            conn.rollback()

        return oids

def register_hstore(conn_or_curs, globally=False, unicode=False):
    """Register adapter and typecaster for `dict`\-\ |hstore| conversions.

    The function must receive a connection or cursor as the |hstore| oid is
    different in each database. The typecaster will normally be registered
    only on the connection or cursor passed as argument. If your application
    uses a single database you can pass *globally*\=True to have the typecaster
    registered on all the connections.

    By default the returned dicts will have `str` objects as keys and values:
    use *unicode*\=True to return `unicode` objects instead.  When adapting a
    dictionary both `str` and `unicode` keys and values are handled (the
    `unicode` values will be converted according to the current
    `~connection.encoding`).

    The |hstore| contrib module must be already installed in the database
    (executing the ``hstore.sql`` script in your ``contrib`` directory).
    Raise `~psycopg2.ProgrammingError` if the type is not found.
    """
    oids = HstoreAdapter.get_oids(conn_or_curs)
    if oids is None:
        raise psycopg2.ProgrammingError(
            "hstore type not found in the database. "
            "please install it from your 'contrib/hstore.sql' file")

    # create and register the typecaster
    if unicode:
        cast = HstoreAdapter.parse_unicode
    else:
        cast = HstoreAdapter.parse

    HSTORE = _ext.new_type((oids[0],), "HSTORE", cast)
    _ext.register_type(HSTORE, not globally and conn_or_curs or None)
    _ext.register_adapter(dict, HstoreAdapter)


__all__ = filter(lambda k: not k.startswith('_'), locals().keys())
