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

from psycopg.extensions import cursor as _cursor

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
        self._cursor = cursor
        self[:] = [None] * len(cursor.description)

    def __getitem__(self, x):
        if type(x) != int:
            x = self._cursor.index[x]
        return list.__getitem__(self, x)
