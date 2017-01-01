#!/usr/bin/env python

# test_sql.py - tests for the psycopg2.sql module
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

import datetime as dt
from testutils import unittest, ConnectingTestCase

from psycopg2 import sql


class ComposeTests(ConnectingTestCase):
    def test_pos(self):
        s = sql.compose("select %s from %s",
            (sql.Identifier('field'), sql.Identifier('table')))
        s1 = s.as_string(self.conn)
        self.assert_(isinstance(s1, str))
        self.assertEqual(s1, 'select "field" from "table"')

    def test_dict(self):
        s = sql.compose("select %(f)s from %(t)s",
            {'f': sql.Identifier('field'), 't': sql.Identifier('table')})
        s1 = s.as_string(self.conn)
        self.assert_(isinstance(s1, str))
        self.assertEqual(s1, 'select "field" from "table"')

    def test_unicode(self):
        s = sql.compose(u"select %s from %s",
            (sql.Identifier(u'field'), sql.Identifier('table')))
        s1 = s.as_string(self.conn)
        self.assert_(isinstance(s1, unicode))
        self.assertEqual(s1, u'select "field" from "table"')

    def test_compose_literal(self):
        s = sql.compose("select %s;", [sql.Literal(dt.date(2016, 12, 31))])
        s1 = s.as_string(self.conn)
        self.assertEqual(s1, "select '2016-12-31'::date;")

    def test_compose_empty(self):
        s = sql.compose("select foo;")
        s1 = s.as_string(self.conn)
        self.assertEqual(s1, "select foo;")

    def test_compose_badnargs(self):
        self.assertRaises(ValueError, sql.compose, "select foo;", [10])
        self.assertRaises(ValueError, sql.compose, "select %s;")
        self.assertRaises(ValueError, sql.compose, "select %s;", [])
        self.assertRaises(ValueError, sql.compose, "select %s;", [10, 20])

    def test_compose_bad_args_type(self):
        self.assertRaises(TypeError, sql.compose, "select %s;", {'a': 10})
        self.assertRaises(TypeError, sql.compose, "select %(x)s;", [10])

    def test_must_be_adaptable(self):
        class Foo(object):
            pass

        self.assertRaises(TypeError,
            sql.compose, "select %s;", [Foo()])

    def test_execute(self):
        cur = self.conn.cursor()
        cur.execute("""
            create table test_compose (
                id serial primary key,
                foo text, bar text, "ba'z" text)
            """)
        cur.execute(
            sql.compose("insert into %s (id, %s) values (%%s, %s)", [
                sql.Identifier('test_compose'),
                sql.SQL(', ').join(map(sql.Identifier, ['foo', 'bar', "ba'z"])),
                (sql.PH() * 3).join(', '),
            ]),
            (10, 'a', 'b', 'c'))

        cur.execute("select * from test_compose")
        self.assertEqual(cur.fetchall(), [(10, 'a', 'b', 'c')])

    def test_executemany(self):
        cur = self.conn.cursor()
        cur.execute("""
            create table test_compose (
                id serial primary key,
                foo text, bar text, "ba'z" text)
            """)
        cur.executemany(
            sql.compose("insert into %s (id, %s) values (%%s, %s)", [
                sql.Identifier('test_compose'),
                sql.SQL(', ').join(map(sql.Identifier, ['foo', 'bar', "ba'z"])),
                (sql.PH() * 3).join(', '),
            ]),
            [(10, 'a', 'b', 'c'), (20, 'd', 'e', 'f')])

        cur.execute("select * from test_compose")
        self.assertEqual(cur.fetchall(),
            [(10, 'a', 'b', 'c'), (20, 'd', 'e', 'f')])


class IdentifierTests(ConnectingTestCase):
    def test_class(self):
        self.assert_(issubclass(sql.Identifier, sql.Composable))

    def test_init(self):
        self.assert_(isinstance(sql.Identifier('foo'), sql.Identifier))
        self.assert_(isinstance(sql.Identifier(u'foo'), sql.Identifier))
        self.assertRaises(TypeError, sql.Identifier, 10)
        self.assertRaises(TypeError, sql.Identifier, dt.date(2016, 12, 31))

    def test_repr(self):
        obj = sql.Identifier("fo'o")
        self.assertEqual(repr(obj), 'sql.Identifier("fo\'o")')
        self.assertEqual(repr(obj), str(obj))

    def test_as_str(self):
        self.assertEqual(sql.Identifier('foo').as_string(self.conn), '"foo"')
        self.assertEqual(sql.Identifier("fo'o").as_string(self.conn), '"fo\'o"')

    def test_join(self):
        self.assert_(not hasattr(sql.Identifier('foo'), 'join'))


class LiteralTests(ConnectingTestCase):
    def test_class(self):
        self.assert_(issubclass(sql.Literal, sql.Composable))

    def test_init(self):
        self.assert_(isinstance(sql.Literal('foo'), sql.Literal))
        self.assert_(isinstance(sql.Literal(u'foo'), sql.Literal))
        self.assert_(isinstance(sql.Literal(b'foo'), sql.Literal))
        self.assert_(isinstance(sql.Literal(42), sql.Literal))
        self.assert_(isinstance(
            sql.Literal(dt.date(2016, 12, 31)), sql.Literal))

    def test_repr(self):
        self.assertEqual(repr(sql.Literal("foo")), "sql.Literal('foo')")
        self.assertEqual(str(sql.Literal("foo")), "sql.Literal('foo')")
        self.assertEqual(
            sql.Literal("foo").as_string(self.conn).replace("E'", "'"),
            "'foo'")
        self.assertEqual(sql.Literal(42).as_string(self.conn), "42")
        self.assertEqual(
            sql.Literal(dt.date(2017, 1, 1)).as_string(self.conn),
            "'2017-01-01'::date")


class SQLTests(ConnectingTestCase):
    def test_class(self):
        self.assert_(issubclass(sql.SQL, sql.Composable))

    def test_init(self):
        self.assert_(isinstance(sql.SQL('foo'), sql.SQL))
        self.assert_(isinstance(sql.SQL(u'foo'), sql.SQL))
        self.assertRaises(TypeError, sql.SQL, 10)
        self.assertRaises(TypeError, sql.SQL, dt.date(2016, 12, 31))

    def test_str(self):
        self.assertEqual(repr(sql.SQL("foo")), "sql.SQL('foo')")
        self.assertEqual(str(sql.SQL("foo")), "sql.SQL('foo')")
        self.assertEqual(sql.SQL("foo").as_string(self.conn), "foo")

    def test_sum(self):
        obj = sql.SQL("foo") + sql.SQL("bar")
        self.assert_(isinstance(obj, sql.Composed))
        self.assertEqual(obj.as_string(self.conn), "foobar")

    def test_sum_inplace(self):
        obj = sql.SQL("foo")
        obj += sql.SQL("bar")
        self.assert_(isinstance(obj, sql.Composed))
        self.assertEqual(obj.as_string(self.conn), "foobar")

    def test_multiply(self):
        obj = sql.SQL("foo") * 3
        self.assert_(isinstance(obj, sql.Composed))
        self.assertEqual(obj.as_string(self.conn), "foofoofoo")

    def test_join(self):
        obj = sql.SQL(", ").join(
            [sql.Identifier('foo'), sql.SQL('bar'), sql.Literal(42)])
        self.assert_(isinstance(obj, sql.Composed))
        self.assertEqual(obj.as_string(self.conn), '"foo", bar, 42')


class ComposedTest(ConnectingTestCase):
    def test_class(self):
        self.assert_(issubclass(sql.Composed, sql.Composable))

    def test_repr(self):
        obj = sql.Composed([sql.Literal("foo"), sql.Identifier("b'ar")])
        self.assertEqual(repr(obj),
            """sql.Composed([sql.Literal('foo'), sql.Identifier("b'ar")])""")
        self.assertEqual(str(obj), repr(obj))

    def test_join(self):
        obj = sql.Composed([sql.Literal("foo"), sql.Identifier("b'ar")])
        obj = obj.join(", ")
        self.assert_(isinstance(obj, sql.Composed))
        self.assertEqual(obj.as_string(self.conn), "'foo', \"b'ar\"")

    def test_sum(self):
        obj = sql.Composed([sql.SQL("foo ")])
        obj = obj + sql.Literal("bar")
        self.assert_(isinstance(obj, sql.Composed))
        self.assertEqual(obj.as_string(self.conn), "foo 'bar'")

    def test_sum_inplace(self):
        obj = sql.Composed([sql.SQL("foo ")])
        obj += sql.Literal("bar")
        self.assert_(isinstance(obj, sql.Composed))
        self.assertEqual(obj.as_string(self.conn), "foo 'bar'")

        obj = sql.Composed([sql.SQL("foo ")])
        obj += sql.Composed([sql.Literal("bar")])
        self.assert_(isinstance(obj, sql.Composed))
        self.assertEqual(obj.as_string(self.conn), "foo 'bar'")


class PlaceholderTest(ConnectingTestCase):
    def test_class(self):
        self.assert_(issubclass(sql.Placeholder, sql.Composable))

    def test_alias(self):
        self.assert_(sql.Placeholder is sql.PH)

    def test_repr(self):
        self.assert_(str(sql.Placeholder()), 'sql.Placeholder()')
        self.assert_(repr(sql.Placeholder()), 'sql.Placeholder()')
        self.assert_(sql.Placeholder().as_string(self.conn), '%s')

    def test_repr_name(self):
        self.assert_(str(sql.Placeholder('foo')), "sql.Placeholder('foo')")
        self.assert_(repr(sql.Placeholder('foo')), "sql.Placeholder('foo')")
        self.assert_(sql.Placeholder('foo').as_string(self.conn), '%(foo)s')

    def test_bad_name(self):
        self.assertRaises(ValueError, sql.Placeholder, ')')


class ValuesTest(ConnectingTestCase):
    def test_null(self):
        self.assert_(isinstance(sql.NULL, sql.SQL))
        self.assertEqual(sql.NULL.as_string(self.conn), "NULL")

    def test_default(self):
        self.assert_(isinstance(sql.DEFAULT, sql.SQL))
        self.assertEqual(sql.DEFAULT.as_string(self.conn), "DEFAULT")


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()
