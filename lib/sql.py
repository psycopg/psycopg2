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

import re
import sys
import collections

from psycopg2 import extensions as ext


class Composable(object):
    """Base class for objects that can be used to compose an SQL string."""
    def as_string(self, conn_or_curs):
        raise NotImplementedError

    def __add__(self, other):
        if isinstance(other, Composed):
            return Composed([self]) + other
        if isinstance(other, Composable):
            return Composed([self]) + Composed([other])
        else:
            return NotImplemented


class Composed(Composable):
    def __init__(self, seq):
        self._seq = []
        for i in seq:
            if not isinstance(i, Composable):
                raise TypeError(
                    "Composed elements must be Composable, got %r instead" % i)
            self._seq.append(i)

    def __repr__(self):
        return "sql.Composed(%r)" % (self._seq,)

    def as_string(self, conn_or_curs):
        rv = []
        for i in self._seq:
            rv.append(i.as_string(conn_or_curs))
        return ''.join(rv)

    def __add__(self, other):
        if isinstance(other, Composed):
            return Composed(self._seq + other._seq)
        if isinstance(other, Composable):
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


class SQL(Composable):
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


class Identifier(Composable):
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


class Literal(Composable):
    def __init__(self, wrapped):
        self._wrapped = wrapped

    def __repr__(self):
        return "sql.Literal(%r)" % (self._wrapped,)

    def as_string(self, conn_or_curs):
        # is it a connection or cursor?
        if isinstance(conn_or_curs, ext.connection):
            conn = conn_or_curs
        elif isinstance(conn_or_curs, ext.cursor):
            conn = conn_or_curs.connection
        else:
            raise TypeError("conn_or_curs must be a connection or a cursor")

        a = ext.adapt(self._wrapped)
        if hasattr(a, 'prepare'):
            a.prepare(conn)

        rv = a.getquoted()
        if sys.version_info[0] >= 3 and isinstance(rv, bytes):
            rv = rv.decode(ext.encodings[conn.encoding])

        return rv

    def __mul__(self, n):
        return Composed([self] * n)


class Placeholder(Composable):
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


re_compose = re.compile("""
    %                           # percent sign
    (?:
        ([%s])                  # either % or s
        | \( ([^\)]+) \) s      # or a (named)s placeholder (named captured)
    )
    """, re.VERBOSE)


def compose(sql, args=()):
    phs = list(re_compose.finditer(sql))

    # check placeholders consistent
    counts = {'%': 0, 's': 0, None: 0}
    for ph in phs:
        counts[ph.group(1)] += 1

    npos = counts['s']
    nnamed = counts[None]

    if npos and nnamed:
        raise ValueError(
            "the sql string contains both named and positional placeholders")

    elif npos:
        if not isinstance(args, collections.Sequence):
            raise TypeError(
                "the sql string expects values in a sequence, got %s instead"
                % type(args).__name__)

        if len(args) != npos:
            raise ValueError(
                "the sql string expects %s values, got %s" % (npos, len(args)))

        return _compose_seq(sql, phs, args)

    elif nnamed:
        if not isinstance(args, collections.Mapping):
            raise TypeError(
                "the sql string expects values in a mapping, got %s instead"
                % type(args))

        return _compose_map(sql, phs, args)

    else:
        if isinstance(args, collections.Sequence) and args:
            raise ValueError(
                "the sql string expects no value, got %s instead" % len(args))
        # If args are a mapping, no placeholder is an acceptable case

        # Convert %% into %
        return _compose_seq(sql, phs, ())


def _compose_seq(sql, phs, args):
    rv = []
    j = 0
    for i, ph in enumerate(phs):
        if i:
            rv.append(SQL(sql[phs[i - 1].end():ph.start()]))
        else:
            rv.append(SQL(sql[0:ph.start()]))

        if ph.group(1) == 's':
            rv.append(args[j])
            j += 1
        else:
            rv.append(SQL('%'))

    if phs:
        rv.append(SQL(sql[phs[-1].end():]))
    else:
        rv.append(SQL(sql))

    return Composed(rv)


def _compose_map(sql, phs, args):
    rv = []
    for i, ph in enumerate(phs):
        if i:
            rv.append(SQL(sql[phs[i - 1].end():ph.start()]))
        else:
            rv.append(SQL(sql[0:ph.start()]))

        if ph.group(2):
            rv.append(args[ph.group(2)])
        else:
            rv.append(SQL('%'))

    if phs:
        rv.append(SQL(sql[phs[-1].end():]))
    else:
        rv.append(sql)

    return Composed(rv)


# Alias
PH = Placeholder

# Literals
NULL = SQL("NULL")
DEFAULT = SQL("DEFAULT")
