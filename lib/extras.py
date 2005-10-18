"""Miscellaneous goodies for psycopg2

This module is a generic place used to hold little helper functions
and classes untill a better place in the distribution is found.
"""
# psycopg/extras.py - miscellaneous extra goodies for psycopg
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

from psycopg2.extensions import cursor as _cursor
from psycopg2.extensions import connection as _connection
from psycopg2.extensions import register_adapter as _RA
from psycopg2.extensions import adapt as _A

class DictConnection(_connection):
    """A connection that uses DictCursor automatically."""
    def cursor(self):
        return _connection.cursor(self, cursor_factory=DictCursor)

class DictCursor(_cursor):
    """A cursor that keeps a list of column name -> index mappings."""

    __query_executed = 0
    
    def execute(self, query, vars=None, async=0):
        self.row_factory = DictRow
        self.index = {}
        self.__query_executed = 1
        return _cursor.execute(self, query, vars, async)

    def _build_index(self):
        if self.__query_executed == 1 and self.description:
            for i in range(len(self.description)):
                self.index[self.description[i][0]] = i
            self.__query_executed = 0
            
    def fetchone(self):
        res = _cursor.fetchone(self)
        if self.__query_executed:
            self._build_index()
        return res

    def fetchmany(self, size=None):
        res = _cursor.fetchmany(self, size)
        if self.__query_executed:
            self._build_index()
        return res

    def fetchall(self):
        res = _cursor.fetchall(self)
        if self.__query_executed:
            self._build_index()
        return res
        

class DictRow(list):
    """A row object that allow by-colun-name access to data."""

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



class SQL_IN(object):
    """Adapt any iterable to an SQL quotable object."""
    
    def __init__(self, seq):
	self._seq = seq
	
    def getquoted(self):
        # this is the important line: note how every object in the
        # list is adapted and then how getquoted() is called on it
	qobjs = [str(_A(o).getquoted()) for o in self._seq]

	return '(' + ', '.join(qobjs) + ')'

    __str__ = getquoted
    
_RA(tuple, SQL_IN)
