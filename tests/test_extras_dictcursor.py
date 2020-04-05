#!/usr/bin/env python
#
# extras_dictcursor - test if DictCursor extension class works
#
# Copyright (C) 2004-2019 Federico Di Gregorio  <fog@debian.org>
# Copyright (C) 2020 The Psycopg Team
#
# psycopg2 is free software: you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# psycopg2 is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.

import copy
import time
import pickle
import unittest
from datetime import timedelta

import psycopg2
from psycopg2.compat import lru_cache
import psycopg2.extras
from psycopg2.extras import NamedTupleConnection, NamedTupleCursor

from .testutils import ConnectingTestCase, skip_before_postgres, \
    skip_before_python, skip_from_python


class _DictCursorBase(ConnectingTestCase):
    def setUp(self):
        ConnectingTestCase.setUp(self)
        curs = self.conn.cursor()
        curs.execute("CREATE TEMPORARY TABLE ExtrasDictCursorTests (foo text)")
        curs.execute("INSERT INTO ExtrasDictCursorTests VALUES ('bar')")
        self.conn.commit()

    def _testIterRowNumber(self, curs):
        # Only checking for dataset < itersize:
        # see CursorTests.test_iter_named_cursor_rownumber
        curs.itersize = 20
        curs.execute("""select * from generate_series(1,10)""")
        for i, r in enumerate(curs):
            self.assertEqual(i + 1, curs.rownumber)

    def _testNamedCursorNotGreedy(self, curs):
        curs.itersize = 2
        curs.execute("""select clock_timestamp() as ts from generate_series(1,3)""")
        recs = []
        for t in curs:
            time.sleep(0.01)
            recs.append(t)

        # check that the dataset was not fetched in a single gulp
        self.assert_(recs[1]['ts'] - recs[0]['ts'] < timedelta(seconds=0.005))
        self.assert_(recs[2]['ts'] - recs[1]['ts'] > timedelta(seconds=0.0099))


class ExtrasDictCursorTests(_DictCursorBase):
    """Test if DictCursor extension class works."""

    def testDictConnCursorArgs(self):
        self.conn.close()
        self.conn = self.connect(connection_factory=psycopg2.extras.DictConnection)
        cur = self.conn.cursor()
        self.assert_(isinstance(cur, psycopg2.extras.DictCursor))
        self.assertEqual(cur.name, None)
        # overridable
        cur = self.conn.cursor('foo',
            cursor_factory=psycopg2.extras.NamedTupleCursor)
        self.assertEqual(cur.name, 'foo')
        self.assert_(isinstance(cur, psycopg2.extras.NamedTupleCursor))

    def testDictCursorWithPlainCursorFetchOne(self):
        self._testWithPlainCursor(lambda curs: curs.fetchone())

    def testDictCursorWithPlainCursorFetchMany(self):
        self._testWithPlainCursor(lambda curs: curs.fetchmany(100)[0])

    def testDictCursorWithPlainCursorFetchManyNoarg(self):
        self._testWithPlainCursor(lambda curs: curs.fetchmany()[0])

    def testDictCursorWithPlainCursorFetchAll(self):
        self._testWithPlainCursor(lambda curs: curs.fetchall()[0])

    def testDictCursorWithPlainCursorIter(self):
        def getter(curs):
            for row in curs:
                return row
        self._testWithPlainCursor(getter)

    def testUpdateRow(self):
        row = self._testWithPlainCursor(lambda curs: curs.fetchone())
        row['foo'] = 'qux'
        self.failUnless(row['foo'] == 'qux')
        self.failUnless(row[0] == 'qux')

    @skip_before_postgres(8, 0)
    def testDictCursorWithPlainCursorIterRowNumber(self):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.DictCursor)
        self._testIterRowNumber(curs)

    def _testWithPlainCursor(self, getter):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.DictCursor)
        curs.execute("SELECT * FROM ExtrasDictCursorTests")
        row = getter(curs)
        self.failUnless(row['foo'] == 'bar')
        self.failUnless(row[0] == 'bar')
        return row

    def testDictCursorWithNamedCursorFetchOne(self):
        self._testWithNamedCursor(lambda curs: curs.fetchone())

    def testDictCursorWithNamedCursorFetchMany(self):
        self._testWithNamedCursor(lambda curs: curs.fetchmany(100)[0])

    def testDictCursorWithNamedCursorFetchManyNoarg(self):
        self._testWithNamedCursor(lambda curs: curs.fetchmany()[0])

    def testDictCursorWithNamedCursorFetchAll(self):
        self._testWithNamedCursor(lambda curs: curs.fetchall()[0])

    def testDictCursorWithNamedCursorIter(self):
        def getter(curs):
            for row in curs:
                return row
        self._testWithNamedCursor(getter)

    @skip_before_postgres(8, 2)
    def testDictCursorWithNamedCursorNotGreedy(self):
        curs = self.conn.cursor('tmp', cursor_factory=psycopg2.extras.DictCursor)
        self._testNamedCursorNotGreedy(curs)

    @skip_before_postgres(8, 0)
    def testDictCursorWithNamedCursorIterRowNumber(self):
        curs = self.conn.cursor('tmp', cursor_factory=psycopg2.extras.DictCursor)
        self._testIterRowNumber(curs)

    def _testWithNamedCursor(self, getter):
        curs = self.conn.cursor('aname', cursor_factory=psycopg2.extras.DictCursor)
        curs.execute("SELECT * FROM ExtrasDictCursorTests")
        row = getter(curs)
        self.failUnless(row['foo'] == 'bar')
        self.failUnless(row[0] == 'bar')

    def testPickleDictRow(self):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.DictCursor)
        curs.execute("select 10 as a, 20 as b")
        r = curs.fetchone()
        d = pickle.dumps(r)
        r1 = pickle.loads(d)
        self.assertEqual(r, r1)
        self.assertEqual(r[0], r1[0])
        self.assertEqual(r[1], r1[1])
        self.assertEqual(r['a'], r1['a'])
        self.assertEqual(r['b'], r1['b'])
        self.assertEqual(r._index, r1._index)

    def test_copy(self):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.DictCursor)
        curs.execute("select 10 as foo, 'hi' as bar")
        rv = curs.fetchone()
        self.assertEqual(len(rv), 2)

        rv2 = copy.copy(rv)
        self.assertEqual(len(rv2), 2)
        self.assertEqual(len(rv), 2)

        rv3 = copy.deepcopy(rv)
        self.assertEqual(len(rv3), 2)
        self.assertEqual(len(rv), 2)

    @skip_from_python(3)
    def test_iter_methods_2(self):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.DictCursor)
        curs.execute("select 10 as a, 20 as b")
        r = curs.fetchone()
        self.assert_(isinstance(r.keys(), list))
        self.assertEqual(len(r.keys()), 2)
        self.assert_(isinstance(r.values(), tuple))     # sic?
        self.assertEqual(len(r.values()), 2)
        self.assert_(isinstance(r.items(), list))
        self.assertEqual(len(r.items()), 2)

        self.assert_(not isinstance(r.iterkeys(), list))
        self.assertEqual(len(list(r.iterkeys())), 2)
        self.assert_(not isinstance(r.itervalues(), list))
        self.assertEqual(len(list(r.itervalues())), 2)
        self.assert_(not isinstance(r.iteritems(), list))
        self.assertEqual(len(list(r.iteritems())), 2)

    @skip_before_python(3)
    def test_iter_methods_3(self):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.DictCursor)
        curs.execute("select 10 as a, 20 as b")
        r = curs.fetchone()
        self.assert_(not isinstance(r.keys(), list))
        self.assertEqual(len(list(r.keys())), 2)
        self.assert_(not isinstance(r.values(), list))
        self.assertEqual(len(list(r.values())), 2)
        self.assert_(not isinstance(r.items(), list))
        self.assertEqual(len(list(r.items())), 2)

    def test_order(self):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.DictCursor)
        curs.execute("select 5 as foo, 4 as bar, 33 as baz, 2 as qux")
        r = curs.fetchone()
        self.assertEqual(list(r), [5, 4, 33, 2])
        self.assertEqual(list(r.keys()), ['foo', 'bar', 'baz', 'qux'])
        self.assertEqual(list(r.values()), [5, 4, 33, 2])
        self.assertEqual(list(r.items()),
            [('foo', 5), ('bar', 4), ('baz', 33), ('qux', 2)])

        r1 = pickle.loads(pickle.dumps(r))
        self.assertEqual(list(r1), list(r))
        self.assertEqual(list(r1.keys()), list(r.keys()))
        self.assertEqual(list(r1.values()), list(r.values()))
        self.assertEqual(list(r1.items()), list(r.items()))

    @skip_from_python(3)
    def test_order_iter(self):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.DictCursor)
        curs.execute("select 5 as foo, 4 as bar, 33 as baz, 2 as qux")
        r = curs.fetchone()
        self.assertEqual(list(r.iterkeys()), ['foo', 'bar', 'baz', 'qux'])
        self.assertEqual(list(r.itervalues()), [5, 4, 33, 2])
        self.assertEqual(list(r.iteritems()),
            [('foo', 5), ('bar', 4), ('baz', 33), ('qux', 2)])

        r1 = pickle.loads(pickle.dumps(r))
        self.assertEqual(list(r1.iterkeys()), list(r.iterkeys()))
        self.assertEqual(list(r1.itervalues()), list(r.itervalues()))
        self.assertEqual(list(r1.iteritems()), list(r.iteritems()))


class ExtrasDictCursorRealTests(_DictCursorBase):
    def testRealMeansReal(self):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
        curs.execute("SELECT * FROM ExtrasDictCursorTests")
        row = curs.fetchone()
        self.assert_(isinstance(row, dict))

    def testDictCursorWithPlainCursorRealFetchOne(self):
        self._testWithPlainCursorReal(lambda curs: curs.fetchone())

    def testDictCursorWithPlainCursorRealFetchMany(self):
        self._testWithPlainCursorReal(lambda curs: curs.fetchmany(100)[0])

    def testDictCursorWithPlainCursorRealFetchManyNoarg(self):
        self._testWithPlainCursorReal(lambda curs: curs.fetchmany()[0])

    def testDictCursorWithPlainCursorRealFetchAll(self):
        self._testWithPlainCursorReal(lambda curs: curs.fetchall()[0])

    def testDictCursorWithPlainCursorRealIter(self):
        def getter(curs):
            for row in curs:
                return row
        self._testWithPlainCursorReal(getter)

    @skip_before_postgres(8, 0)
    def testDictCursorWithPlainCursorRealIterRowNumber(self):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
        self._testIterRowNumber(curs)

    def _testWithPlainCursorReal(self, getter):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
        curs.execute("SELECT * FROM ExtrasDictCursorTests")
        row = getter(curs)
        self.failUnless(row['foo'] == 'bar')

    def testPickleRealDictRow(self):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
        curs.execute("select 10 as a, 20 as b")
        r = curs.fetchone()
        d = pickle.dumps(r)
        r1 = pickle.loads(d)
        self.assertEqual(r, r1)
        self.assertEqual(r['a'], r1['a'])
        self.assertEqual(r['b'], r1['b'])

    def test_copy(self):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
        curs.execute("select 10 as foo, 'hi' as bar")
        rv = curs.fetchone()
        self.assertEqual(len(rv), 2)

        rv2 = copy.copy(rv)
        self.assertEqual(len(rv2), 2)
        self.assertEqual(len(rv), 2)

        rv3 = copy.deepcopy(rv)
        self.assertEqual(len(rv3), 2)
        self.assertEqual(len(rv), 2)

    def testDictCursorRealWithNamedCursorFetchOne(self):
        self._testWithNamedCursorReal(lambda curs: curs.fetchone())

    def testDictCursorRealWithNamedCursorFetchMany(self):
        self._testWithNamedCursorReal(lambda curs: curs.fetchmany(100)[0])

    def testDictCursorRealWithNamedCursorFetchManyNoarg(self):
        self._testWithNamedCursorReal(lambda curs: curs.fetchmany()[0])

    def testDictCursorRealWithNamedCursorFetchAll(self):
        self._testWithNamedCursorReal(lambda curs: curs.fetchall()[0])

    def testDictCursorRealWithNamedCursorIter(self):
        def getter(curs):
            for row in curs:
                return row
        self._testWithNamedCursorReal(getter)

    @skip_before_postgres(8, 2)
    def testDictCursorRealWithNamedCursorNotGreedy(self):
        curs = self.conn.cursor('tmp', cursor_factory=psycopg2.extras.RealDictCursor)
        self._testNamedCursorNotGreedy(curs)

    @skip_before_postgres(8, 0)
    def testDictCursorRealWithNamedCursorIterRowNumber(self):
        curs = self.conn.cursor('tmp', cursor_factory=psycopg2.extras.RealDictCursor)
        self._testIterRowNumber(curs)

    def _testWithNamedCursorReal(self, getter):
        curs = self.conn.cursor('aname',
            cursor_factory=psycopg2.extras.RealDictCursor)
        curs.execute("SELECT * FROM ExtrasDictCursorTests")
        row = getter(curs)
        self.failUnless(row['foo'] == 'bar')

    @skip_from_python(3)
    def test_iter_methods_2(self):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
        curs.execute("select 10 as a, 20 as b")
        r = curs.fetchone()
        self.assert_(isinstance(r.keys(), list))
        self.assertEqual(len(r.keys()), 2)
        self.assert_(isinstance(r.values(), list))
        self.assertEqual(len(r.values()), 2)
        self.assert_(isinstance(r.items(), list))
        self.assertEqual(len(r.items()), 2)

        self.assert_(not isinstance(r.iterkeys(), list))
        self.assertEqual(len(list(r.iterkeys())), 2)
        self.assert_(not isinstance(r.itervalues(), list))
        self.assertEqual(len(list(r.itervalues())), 2)
        self.assert_(not isinstance(r.iteritems(), list))
        self.assertEqual(len(list(r.iteritems())), 2)

    @skip_before_python(3)
    def test_iter_methods_3(self):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
        curs.execute("select 10 as a, 20 as b")
        r = curs.fetchone()
        self.assert_(not isinstance(r.keys(), list))
        self.assertEqual(len(list(r.keys())), 2)
        self.assert_(not isinstance(r.values(), list))
        self.assertEqual(len(list(r.values())), 2)
        self.assert_(not isinstance(r.items(), list))
        self.assertEqual(len(list(r.items())), 2)

    def test_order(self):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
        curs.execute("select 5 as foo, 4 as bar, 33 as baz, 2 as qux")
        r = curs.fetchone()
        self.assertEqual(list(r), ['foo', 'bar', 'baz', 'qux'])
        self.assertEqual(list(r.keys()), ['foo', 'bar', 'baz', 'qux'])
        self.assertEqual(list(r.values()), [5, 4, 33, 2])
        self.assertEqual(list(r.items()),
            [('foo', 5), ('bar', 4), ('baz', 33), ('qux', 2)])

        r1 = pickle.loads(pickle.dumps(r))
        self.assertEqual(list(r1), list(r))
        self.assertEqual(list(r1.keys()), list(r.keys()))
        self.assertEqual(list(r1.values()), list(r.values()))
        self.assertEqual(list(r1.items()), list(r.items()))

    @skip_from_python(3)
    def test_order_iter(self):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
        curs.execute("select 5 as foo, 4 as bar, 33 as baz, 2 as qux")
        r = curs.fetchone()
        self.assertEqual(list(r.iterkeys()), ['foo', 'bar', 'baz', 'qux'])
        self.assertEqual(list(r.itervalues()), [5, 4, 33, 2])
        self.assertEqual(list(r.iteritems()),
            [('foo', 5), ('bar', 4), ('baz', 33), ('qux', 2)])

        r1 = pickle.loads(pickle.dumps(r))
        self.assertEqual(list(r1.iterkeys()), list(r.iterkeys()))
        self.assertEqual(list(r1.itervalues()), list(r.itervalues()))
        self.assertEqual(list(r1.iteritems()), list(r.iteritems()))

    def test_pop(self):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
        curs.execute("select 1 as a, 2 as b, 3 as c")
        r = curs.fetchone()
        self.assertEqual(r.pop('b'), 2)
        self.assertEqual(list(r), ['a', 'c'])
        self.assertEqual(list(r.keys()), ['a', 'c'])
        self.assertEqual(list(r.values()), [1, 3])
        self.assertEqual(list(r.items()), [('a', 1), ('c', 3)])

        self.assertEqual(r.pop('b', None), None)
        self.assertRaises(KeyError, r.pop, 'b')

    def test_mod(self):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
        curs.execute("select 1 as a, 2 as b, 3 as c")
        r = curs.fetchone()
        r['d'] = 4
        self.assertEqual(list(r), ['a', 'b', 'c', 'd'])
        self.assertEqual(list(r.keys()), ['a', 'b', 'c', 'd'])
        self.assertEqual(list(r.values()), [1, 2, 3, 4])
        self.assertEqual(list(
            r.items()), [('a', 1), ('b', 2), ('c', 3), ('d', 4)])

        assert r['a'] == 1
        assert r['b'] == 2
        assert r['c'] == 3
        assert r['d'] == 4


class NamedTupleCursorTest(ConnectingTestCase):
    def setUp(self):
        ConnectingTestCase.setUp(self)

        self.conn = self.connect(connection_factory=NamedTupleConnection)
        curs = self.conn.cursor()
        curs.execute("CREATE TEMPORARY TABLE nttest (i int, s text)")
        curs.execute("INSERT INTO nttest VALUES (1, 'foo')")
        curs.execute("INSERT INTO nttest VALUES (2, 'bar')")
        curs.execute("INSERT INTO nttest VALUES (3, 'baz')")
        self.conn.commit()

    def test_cursor_args(self):
        cur = self.conn.cursor('foo', cursor_factory=psycopg2.extras.DictCursor)
        self.assertEqual(cur.name, 'foo')
        self.assert_(isinstance(cur, psycopg2.extras.DictCursor))

    def test_fetchone(self):
        curs = self.conn.cursor()
        curs.execute("select * from nttest order by 1")
        t = curs.fetchone()
        self.assertEqual(t[0], 1)
        self.assertEqual(t.i, 1)
        self.assertEqual(t[1], 'foo')
        self.assertEqual(t.s, 'foo')
        self.assertEqual(curs.rownumber, 1)
        self.assertEqual(curs.rowcount, 3)

    def test_fetchmany_noarg(self):
        curs = self.conn.cursor()
        curs.arraysize = 2
        curs.execute("select * from nttest order by 1")
        res = curs.fetchmany()
        self.assertEqual(2, len(res))
        self.assertEqual(res[0].i, 1)
        self.assertEqual(res[0].s, 'foo')
        self.assertEqual(res[1].i, 2)
        self.assertEqual(res[1].s, 'bar')
        self.assertEqual(curs.rownumber, 2)
        self.assertEqual(curs.rowcount, 3)

    def test_fetchmany(self):
        curs = self.conn.cursor()
        curs.execute("select * from nttest order by 1")
        res = curs.fetchmany(2)
        self.assertEqual(2, len(res))
        self.assertEqual(res[0].i, 1)
        self.assertEqual(res[0].s, 'foo')
        self.assertEqual(res[1].i, 2)
        self.assertEqual(res[1].s, 'bar')
        self.assertEqual(curs.rownumber, 2)
        self.assertEqual(curs.rowcount, 3)

    def test_fetchall(self):
        curs = self.conn.cursor()
        curs.execute("select * from nttest order by 1")
        res = curs.fetchall()
        self.assertEqual(3, len(res))
        self.assertEqual(res[0].i, 1)
        self.assertEqual(res[0].s, 'foo')
        self.assertEqual(res[1].i, 2)
        self.assertEqual(res[1].s, 'bar')
        self.assertEqual(res[2].i, 3)
        self.assertEqual(res[2].s, 'baz')
        self.assertEqual(curs.rownumber, 3)
        self.assertEqual(curs.rowcount, 3)

    def test_executemany(self):
        curs = self.conn.cursor()
        curs.executemany("delete from nttest where i = %s",
            [(1,), (2,)])
        curs.execute("select * from nttest order by 1")
        res = curs.fetchall()
        self.assertEqual(1, len(res))
        self.assertEqual(res[0].i, 3)
        self.assertEqual(res[0].s, 'baz')

    def test_iter(self):
        curs = self.conn.cursor()
        curs.execute("select * from nttest order by 1")
        i = iter(curs)
        self.assertEqual(curs.rownumber, 0)

        t = next(i)
        self.assertEqual(t.i, 1)
        self.assertEqual(t.s, 'foo')
        self.assertEqual(curs.rownumber, 1)
        self.assertEqual(curs.rowcount, 3)

        t = next(i)
        self.assertEqual(t.i, 2)
        self.assertEqual(t.s, 'bar')
        self.assertEqual(curs.rownumber, 2)
        self.assertEqual(curs.rowcount, 3)

        t = next(i)
        self.assertEqual(t.i, 3)
        self.assertEqual(t.s, 'baz')
        self.assertRaises(StopIteration, next, i)
        self.assertEqual(curs.rownumber, 3)
        self.assertEqual(curs.rowcount, 3)

    def test_record_updated(self):
        curs = self.conn.cursor()
        curs.execute("select 1 as foo;")
        r = curs.fetchone()
        self.assertEqual(r.foo, 1)

        curs.execute("select 2 as bar;")
        r = curs.fetchone()
        self.assertEqual(r.bar, 2)
        self.assertRaises(AttributeError, getattr, r, 'foo')

    def test_no_result_no_surprise(self):
        curs = self.conn.cursor()
        curs.execute("update nttest set s = s")
        self.assertRaises(psycopg2.ProgrammingError, curs.fetchone)

        curs.execute("update nttest set s = s")
        self.assertRaises(psycopg2.ProgrammingError, curs.fetchall)

    def test_bad_col_names(self):
        curs = self.conn.cursor()
        curs.execute('select 1 as "foo.bar_baz", 2 as "?column?", 3 as "3"')
        rv = curs.fetchone()
        self.assertEqual(rv.foo_bar_baz, 1)
        self.assertEqual(rv.f_column_, 2)
        self.assertEqual(rv.f3, 3)

    @skip_before_python(3)
    @skip_before_postgres(8)
    def test_nonascii_name(self):
        curs = self.conn.cursor()
        curs.execute('select 1 as \xe5h\xe9')
        rv = curs.fetchone()
        self.assertEqual(getattr(rv, '\xe5h\xe9'), 1)

    def test_minimal_generation(self):
        # Instrument the class to verify it gets called the minimum number of times.
        f_orig = NamedTupleCursor._make_nt
        calls = [0]

        def f_patched(self_):
            calls[0] += 1
            return f_orig(self_)

        NamedTupleCursor._make_nt = f_patched

        try:
            curs = self.conn.cursor()
            curs.execute("select * from nttest order by 1")
            curs.fetchone()
            curs.fetchone()
            curs.fetchone()
            self.assertEqual(1, calls[0])

            curs.execute("select * from nttest order by 1")
            curs.fetchone()
            curs.fetchall()
            self.assertEqual(2, calls[0])

            curs.execute("select * from nttest order by 1")
            curs.fetchone()
            curs.fetchmany(1)
            self.assertEqual(3, calls[0])

        finally:
            NamedTupleCursor._make_nt = f_orig

    @skip_before_postgres(8, 0)
    def test_named(self):
        curs = self.conn.cursor('tmp')
        curs.execute("""select i from generate_series(0,9) i""")
        recs = []
        recs.extend(curs.fetchmany(5))
        recs.append(curs.fetchone())
        recs.extend(curs.fetchall())
        self.assertEqual(list(range(10)), [t.i for t in recs])

    def test_named_fetchone(self):
        curs = self.conn.cursor('tmp')
        curs.execute("""select 42 as i""")
        t = curs.fetchone()
        self.assertEqual(t.i, 42)

    def test_named_fetchmany(self):
        curs = self.conn.cursor('tmp')
        curs.execute("""select 42 as i""")
        recs = curs.fetchmany(10)
        self.assertEqual(recs[0].i, 42)

    def test_named_fetchall(self):
        curs = self.conn.cursor('tmp')
        curs.execute("""select 42 as i""")
        recs = curs.fetchall()
        self.assertEqual(recs[0].i, 42)

    @skip_before_postgres(8, 2)
    def test_not_greedy(self):
        curs = self.conn.cursor('tmp')
        curs.itersize = 2
        curs.execute("""select clock_timestamp() as ts from generate_series(1,3)""")
        recs = []
        for t in curs:
            time.sleep(0.01)
            recs.append(t)

        # check that the dataset was not fetched in a single gulp
        self.assert_(recs[1].ts - recs[0].ts < timedelta(seconds=0.005))
        self.assert_(recs[2].ts - recs[1].ts > timedelta(seconds=0.0099))

    @skip_before_postgres(8, 0)
    def test_named_rownumber(self):
        curs = self.conn.cursor('tmp')
        # Only checking for dataset < itersize:
        # see CursorTests.test_iter_named_cursor_rownumber
        curs.itersize = 4
        curs.execute("""select * from generate_series(1,3)""")
        for i, t in enumerate(curs):
            self.assertEqual(i + 1, curs.rownumber)

    def test_cache(self):
        NamedTupleCursor._cached_make_nt.cache_clear()

        curs = self.conn.cursor()
        curs.execute("select 10 as a, 20 as b")
        r1 = curs.fetchone()
        curs.execute("select 10 as a, 20 as c")
        r2 = curs.fetchone()

        # Get a new cursor to check that the cache works across multiple ones
        curs = self.conn.cursor()
        curs.execute("select 10 as a, 30 as b")
        r3 = curs.fetchone()

        self.assert_(type(r1) is type(r3))
        self.assert_(type(r1) is not type(r2))

        cache_info = NamedTupleCursor._cached_make_nt.cache_info()
        self.assertEqual(cache_info.hits, 1)
        self.assertEqual(cache_info.misses, 2)
        self.assertEqual(cache_info.currsize, 2)

    def test_max_cache(self):
        old_func = NamedTupleCursor._cached_make_nt
        NamedTupleCursor._cached_make_nt = \
            lru_cache(8)(NamedTupleCursor._cached_make_nt.__wrapped__)
        try:
            recs = []
            curs = self.conn.cursor()
            for i in range(10):
                curs.execute("select 1 as f%s" % i)
                recs.append(curs.fetchone())

            # Still in cache
            curs.execute("select 1 as f9")
            rec = curs.fetchone()
            self.assert_(any(type(r) is type(rec) for r in recs))

            # Gone from cache
            curs.execute("select 1 as f0")
            rec = curs.fetchone()
            self.assert_(all(type(r) is not type(rec) for r in recs))

        finally:
            NamedTupleCursor._cached_make_nt = old_func


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)


if __name__ == "__main__":
    unittest.main()
