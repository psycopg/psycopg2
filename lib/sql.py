"""SQL composition utility module
"""

# psycopg/sql.py - Implementation of the JSON adaptation objects
#
# Copyright (C) 2016 Daniele Varrazzo  <daniele.varrazzo@gmail.com>
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

from psycopg2 import extensions as ext


class Composible(object):
    """Base class for objects that can be used to compose an SQL string."""
    def as_string(self, conn_or_curs):
        raise NotImplementedError

    def __add__(self, other):
        if isinstance(other, Composed):
            return Composed([self]) + other
        if isinstance(other, Composible):
            return Composed([self]) + Composed([other])
        else:
            return NotImplemented


class Composed(Composible):
    def __init__(self, seq):
        self._seq = []
        for i in seq:
            if not isinstance(i, Composible):
                raise TypeError(
                    "Composed elements must be Composible, got %r instead" % i)
            self._seq.append(i)

    def __repr__(self):
        return "sql.Composed(%r)" % (self.seq,)

    def as_string(self, conn_or_curs):
        rv = []
        for i in self._seq:
            rv.append(i.as_string(conn_or_curs))
        return ''.join(rv)

    def __add__(self, other):
        if isinstance(other, Composed):
            return Composed(self._seq + other._seq)
        if isinstance(other, Composible):
            return Composed(self._seq + [other])
        else:
            return NotImplemented

    def __mul__(self, n):
        return Composed(self._seq * n)

    def join(self, joiner):
        if isinstance(joiner, basestring):
            joiner = SQL(joiner)
        elif not isinstance(joiner, SQL):
            raise TypeError(
                "Composed.join() argument must be a string or an SQL")

        if len(self._seq) <= 1:
            return self

        it = iter(self._seq)
        rv = [it.next()]
        for i in it:
            rv.append(joiner)
            rv.append(i)

        return Composed(rv)


class SQL(Composible):
    def __init__(self, wrapped):
        if not isinstance(wrapped, basestring):
            raise TypeError("SQL values must be strings")
        self._wrapped = wrapped

    def __repr__(self):
        return "sql.SQL(%r)" % (self._wrapped,)

    def as_string(self, conn_or_curs):
        return self._wrapped

    def __mul__(self, n):
        return Composed([self] * n)

    def join(self, seq):
        rv = []
        it = iter(seq)
        try:
            rv.append(it.next())
        except StopIteration:
            pass
        else:
            for i in it:
                rv.append(self)
                rv.append(i)

        return Composed(rv)


class Identifier(Composible):
    def __init__(self, wrapped):
        if not isinstance(wrapped, basestring):
            raise TypeError("SQL identifiers must be strings")

        self._wrapped = wrapped

    @property
    def wrapped(self):
        return self._wrapped

    def __repr__(self):
        return "sql.Identifier(%r)" % (self._wrapped,)

    def as_string(self, conn_or_curs):
        return ext.quote_ident(self._wrapped, conn_or_curs)


class Literal(Composible):
    def __init__(self, wrapped):
        self._wrapped = wrapped

    def __repr__(self):
        return "sql.Literal(%r)" % (self._wrapped,)

    def as_string(self, conn_or_curs):
        a = ext.adapt(self._wrapped)
        if hasattr(a, 'prepare'):
            # is it a connection or cursor?
            if isinstance(conn_or_curs, ext.connection):
                conn = conn_or_curs
            elif isinstance(conn_or_curs, ext.cursor):
                conn = conn_or_curs.connection
            else:
                raise TypeError("conn_or_curs must be a connection or a cursor")

            a.prepare(conn)

        return a.getquoted()

    def __mul__(self, n):
        return Composed([self] * n)


class Placeholder(Composible):
    def __init__(self, name=None):
        if isinstance(name, basestring):
            if ')' in name:
                raise ValueError("invalid name: %r" % name)

        elif name is not None:
            raise TypeError("expected string or None as name, got %r" % name)

        self._name = name

    def __repr__(self):
        return "sql.Placeholder(%r)" % (
            self._name if self._name is not None else '',)

    def __mul__(self, n):
        return Composed([self] * n)

    def as_string(self, conn_or_curs):
        if self._name is not None:
            return "%%(%s)s" % self._name
        else:
            return "%s"


def compose(sql, args=()):
    raise NotImplementedError


# Alias
PH = Placeholder

# Literals
NULL = SQL("NULL")
DEFAULT = SQL("DEFAULT")
