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
    """
    Abstract base class for objects that can be used to compose an SQL string.

    Composables can be passed directly to `~cursor.execute()` and
    `~cursor.executemany()`.

    Composables can be joined using the ``+`` operator: the result will be
    a `Composed` instance containing the objects joined. The operator ``*`` is
    also supported with an integer argument: the result is a `!Composed`
    instance containing the left argument repeated as many times as requested.

    .. automethod:: as_string
    """
    def as_string(self, conn_or_curs):
        """
        Return the string value of the object.

        The object is evaluated in the context of the *conn_or_curs* argument.

        The function is automatically invoked by `~cursor.execute()` and
        `~cursor.executemany()` if a `!Composable` is passed instead of the
        query string.
        """
        raise NotImplementedError

    def __add__(self, other):
        if isinstance(other, Composed):
            return Composed([self]) + other
        if isinstance(other, Composable):
            return Composed([self]) + Composed([other])
        else:
            return NotImplemented

    def __mul__(self, n):
        return Composed([self] * n)


class Composed(Composable):
    """
    A `Composable` object obtained concatenating a sequence of `Composable`.

    The object is usually created using `compose()` and the `Composable`
    operators. However it is possible to create a `!Composed` directly
    specifying a sequence of `Composable` as arguments.

    Example::

        >>> sql.Composed([sql.SQL("insert into "), sql.Identifier("table")]) \\
        ...     .as_string(conn)
        'insert into "table"'

    .. automethod:: join
    """
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

    def join(self, joiner):
        """
        Return a new `!Composed` interposing the *joiner* with the `!Composed` items.

        The *joiner* must be a `SQL` or a string which will be interpreted as
        an `SQL`.

        Example::

            >>> fields = sql.Identifier('foo') + sql.Identifier('bar')  # a Composed
            >>> fields.join(', ').as_string(conn)
            '"foo", "bar"'

        """
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
    """
    A `Composable` representing a snippet of SQL string to be included verbatim.

    `!SQL` supports the ``%`` operator to incorporate variable parts of a query
    into a template: the operator takes a sequence or mapping of `Composable`
    (according to the style of the placeholders in the *string*) and returning
    a `Composed` object.

    Example::

        >>> query = sql.SQL("select %s from %s") % [
        ...    sql.SQL(', ').join([sql.Identifier('foo'), sql.Identifier('bar')]),
        ...    sql.Identifier('table')]
        >>> query.as_string(conn)
        select "foo", "bar" from "table"'


    .. automethod:: join
    """
    def __init__(self, string):
        if not isinstance(string, basestring):
            raise TypeError("SQL values must be strings")
        self._wrapped = string

    def __repr__(self):
        return "sql.SQL(%r)" % (self._wrapped,)

    def __mod__(self, args):
        return _compose(self._wrapped, args)

    def as_string(self, conn_or_curs):
        return self._wrapped

    def join(self, seq):
        """
        Join a sequence of `Composable` or a `Composed` and return a `!Composed`.

        Use the object *string* to separate the *seq* elements.

        Example::

            >>> snip - sql.SQL(', ').join(map(sql.Identifier, ['foo', 'bar', 'baz']))
            >>> snip.as_string(conn)
            '"foo", "bar", "baz"'
        """
        if isinstance(seq, Composed):
            seq = seq._seq

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
    """
    A `Composable` representing an SQL identifer.

    Identifiers usually represent names of database objects, such as tables
    or fields. They follow `different rules`__ than SQL string literals for
    escaping (e.g. they use double quotes).

    .. __: https://www.postgresql.org/docs/current/static/sql-syntax-lexical.html# \
        SQL-SYNTAX-IDENTIFIERS
    """
    def __init__(self, string):
        if not isinstance(string, basestring):
            raise TypeError("SQL identifiers must be strings")

        self._wrapped = string

    def __repr__(self):
        return "sql.Identifier(%r)" % (self._wrapped,)

    def as_string(self, conn_or_curs):
        return ext.quote_ident(self._wrapped, conn_or_curs)


class Literal(Composable):
    """
    Represent an SQL value to be included in a query.

    Usually you will want to include placeholders in the query and pass values
    as `~cursor.execute()` arguments. If however you really really need to
    include a literal value in the query you can use this object.

    The string returned by `!as_string()` follows the normal :ref:`adaptation
    rules <python-types-adaptation>` for Python objects.

    """
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


class Placeholder(Composable):
    """A `Composable` representing a placeholder for query parameters.

    If the name is specified, generate a named placeholder (e.g. ``%(name)s``),
    otherwise generate a positional placeholder (e.g. ``%s``).

    The object is useful to generate SQL queries with a variable number of
    arguments.

    Examples::

        >>> (sql.SQL("insert into table (%s) values (%s)") % [
        ...     sql.SQL(', ').join(map(sql.Identifier, names)),
        ...     sql.SQL(', ').join(sql.Placeholder() * 3)
        ... ]).as_string(conn)
        'insert into table ("foo", "bar", "baz") values (%s, %s, %s)'

        >>> (sql.SQL("insert into table (%s) values (%s)") % [
        ...     sql.SQL(', ').join(map(sql.Identifier, names)),
        ...     sql.SQL(', ').join(map(sql.Placeholder, names))
        ... ]).as_string(conn)
        'insert into table ("foo", "bar", "baz") values (%(foo)s, %(bar)s, %(baz)s)'
    """

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


def _compose(sql, args=None):
    """
    Merge an SQL string with some variable parts.

    The *sql* string can contain placeholders such as `%s` or `%(name)s`.
    If the string must contain a literal ``%`` symbol use ``%%``. Note that,
    unlike `~cursor.execute()`, the replacement ``%%`` |=>| ``%`` is *always*
    performed, even if there is no argument.

    .. |=>| unicode:: 0x21D2 .. double right arrow

    *args* must be a sequence or mapping (according to the placeholder style)
    of `Composable` instances.

    The value returned is a `Composed` instance obtained replacing the
    arguments to the query placeholders.
    """
    if args is None:
        args = ()

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
