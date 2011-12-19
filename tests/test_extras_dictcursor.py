#!/usr/bin/env python
#
# extras_dictcursor - test if DictCursor extension class works
#
# Copyright (C) 2004-2010 Federico Di Gregorio  <fog@debian.org>
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

import time
from datetime import timedelta
import psycopg2
import psycopg2.extras
from testutils import unittest, skip_before_postgres, skip_if_no_namedtuple
from testconfig import dsn


class ExtrasDictCursorTests(unittest.TestCase):
    """Test if DictCursor extension class works."""

    def setUp(self):
        self.conn = psycopg2.connect(dsn)
        curs = self.conn.cursor()
        curs.execute("CREATE TEMPORARY TABLE ExtrasDictCursorTests (foo text)")
        curs.execute("INSERT INTO ExtrasDictCursorTests VALUES ('bar')")
        self.conn.commit()

    def tearDown(self):
        self.conn.close()

    def testDictCursorWithPlainCursorFetchOne(self):
        self._testWithPlainCursor(lambda curs: curs.fetchone())

    def testDictCursorWithPlainCursorFetchMany(self):
        self._testWithPlainCursor(lambda curs: curs.fetchmany(100)[0])

    def testDictCursorWithPlainCursorFetchAll(self):
        self._testWithPlainCursor(lambda curs: curs.fetchall()[0])

    def testDictCursorWithPlainCursorIter(self):
        def getter(curs):
            for row in curs:
                return row
        self._testWithPlainCursor(getter)

    def testDictCursorWithPlainCursorRealFetchOne(self):
        self._testWithPlainCursorReal(lambda curs: curs.fetchone())

    def testDictCursorWithPlainCursorRealFetchMany(self):
        self._testWithPlainCursorReal(lambda curs: curs.fetchmany(100)[0])

    def testDictCursorWithPlainCursorRealFetchAll(self):
        self._testWithPlainCursorReal(lambda curs: curs.fetchall()[0])

    def testDictCursorWithPlainCursorRealIter(self):
        def getter(curs):
            for row in curs:
                return row
        self._testWithPlainCursorReal(getter)


    def testDictCursorWithNamedCursorFetchOne(self):
        self._testWithNamedCursor(lambda curs: curs.fetchone())

    def testDictCursorWithNamedCursorFetchMany(self):
        self._testWithNamedCursor(lambda curs: curs.fetchmany(100)[0])

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


    def testDictCursorRealWithNamedCursorFetchOne(self):
        self._testWithNamedCursorReal(lambda curs: curs.fetchone())

    def testDictCursorRealWithNamedCursorFetchMany(self):
        self._testWithNamedCursorReal(lambda curs: curs.fetchmany(100)[0])

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


    def _testWithPlainCursor(self, getter):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.DictCursor)
        curs.execute("SELECT * FROM ExtrasDictCursorTests")
        row = getter(curs)
        self.failUnless(row['foo'] == 'bar')
        self.failUnless(row[0] == 'bar')
        return row

    def _testWithNamedCursor(self, getter):
        curs = self.conn.cursor('aname', cursor_factory=psycopg2.extras.DictCursor)
        curs.execute("SELECT * FROM ExtrasDictCursorTests")
        row = getter(curs)
        self.failUnless(row['foo'] == 'bar')
        self.failUnless(row[0] == 'bar')

    def _testWithPlainCursorReal(self, getter):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
        curs.execute("SELECT * FROM ExtrasDictCursorTests")
        row = getter(curs)
        self.failUnless(row['foo'] == 'bar')

    def _testWithNamedCursorReal(self, getter):
        curs = self.conn.cursor('aname', cursor_factory=psycopg2.extras.RealDictCursor)
        curs.execute("SELECT * FROM ExtrasDictCursorTests")
        row = getter(curs)
        self.failUnless(row['foo'] == 'bar')

    def testUpdateRow(self):
        row = self._testWithPlainCursor(lambda curs: curs.fetchone())
        row['foo'] = 'qux'
        self.failUnless(row['foo'] == 'qux')
        self.failUnless(row[0] == 'qux')

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


class NamedTupleCursorTest(unittest.TestCase):
    def setUp(self):
        from psycopg2.extras import NamedTupleConnection

        try:
            from collections import namedtuple
        except ImportError:
            self.conn = None
            return

        self.conn = psycopg2.connect(dsn,
            connection_factory=NamedTupleConnection)
        curs = self.conn.cursor()
        curs.execute("CREATE TEMPORARY TABLE nttest (i int, s text)")
        curs.execute("INSERT INTO nttest VALUES (1, 'foo')")
        curs.execute("INSERT INTO nttest VALUES (2, 'bar')")
        curs.execute("INSERT INTO nttest VALUES (3, 'baz')")
        self.conn.commit()

    def tearDown(self):
        if self.conn is not None:
            self.conn.close()

    @skip_if_no_namedtuple
    def test_fetchone(self):
        curs = self.conn.cursor()
        curs.execute("select * from nttest where i = 1")
        t = curs.fetchone()
        self.assertEqual(t[0], 1)
        self.assertEqual(t.i, 1)
        self.assertEqual(t[1], 'foo')
        self.assertEqual(t.s, 'foo')

    @skip_if_no_namedtuple
    def test_fetchmany(self):
        curs = self.conn.cursor()
        curs.execute("select * from nttest order by 1")
        res = curs.fetchmany(2)
        self.assertEqual(2, len(res))
        self.assertEqual(res[0].i, 1)
        self.assertEqual(res[0].s, 'foo')
        self.assertEqual(res[1].i, 2)
        self.assertEqual(res[1].s, 'bar')

    @skip_if_no_namedtuple
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

    @skip_if_no_namedtuple
    def test_executemany(self):
        curs = self.conn.cursor()
        curs.executemany("delete from nttest where i = %s",
            [(1,), (2,)])
        curs.execute("select * from nttest order by 1")
        res = curs.fetchall()
        self.assertEqual(1, len(res))
        self.assertEqual(res[0].i, 3)
        self.assertEqual(res[0].s, 'baz')

    @skip_if_no_namedtuple
    def test_iter(self):
        curs = self.conn.cursor()
        curs.execute("select * from nttest order by 1")
        i = iter(curs)
        t = i.next()
        self.assertEqual(t.i, 1)
        self.assertEqual(t.s, 'foo')
        t = i.next()
        self.assertEqual(t.i, 2)
        self.assertEqual(t.s, 'bar')
        t = i.next()
        self.assertEqual(t.i, 3)
        self.assertEqual(t.s, 'baz')
        self.assertRaises(StopIteration, i.next)

    def test_error_message(self):
        try:
            from collections import namedtuple
        except ImportError:
            # an import error somewhere
            from psycopg2.extras import NamedTupleConnection
            try:
                if self.conn is not None:
                    self.conn.close()
                self.conn = psycopg2.connect(dsn,
                    connection_factory=NamedTupleConnection)
                curs = self.conn.cursor()
                curs.execute("select 1")
                curs.fetchone()
            except ImportError:
                pass
            else:
                self.fail("expecting ImportError")
        else:
            # skip the test
            pass

    @skip_if_no_namedtuple
    def test_record_updated(self):
        curs = self.conn.cursor()
        curs.execute("select 1 as foo;")
        r = curs.fetchone()
        self.assertEqual(r.foo, 1)

        curs.execute("select 2 as bar;")
        r = curs.fetchone()
        self.assertEqual(r.bar, 2)
        self.assertRaises(AttributeError, getattr, r, 'foo')

    @skip_if_no_namedtuple
    def test_no_result_no_surprise(self):
        curs = self.conn.cursor()
        curs.execute("update nttest set s = s")
        self.assertRaises(psycopg2.ProgrammingError, curs.fetchone)

        curs.execute("update nttest set s = s")
        self.assertRaises(psycopg2.ProgrammingError, curs.fetchall)

    @skip_if_no_namedtuple
    def test_minimal_generation(self):
        # Instrument the class to verify it gets called the minimum number of times.
        from psycopg2.extras import NamedTupleCursor
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

    @skip_if_no_namedtuple
    @skip_before_postgres(8, 0)
    def test_named(self):
        curs = self.conn.cursor('tmp')
        curs.execute("""select i from generate_series(0,9) i""")
        recs = []
        recs.extend(curs.fetchmany(5))
        recs.append(curs.fetchone())
        recs.extend(curs.fetchall())
        self.assertEqual(range(10), [t.i for t in recs])

    @skip_if_no_namedtuple
    def test_named_fetchone(self):
        curs = self.conn.cursor('tmp')
        curs.execute("""select 42 as i""")
        t = curs.fetchone()
        self.assertEqual(t.i, 42)

    @skip_if_no_namedtuple
    def test_named_fetchmany(self):
        curs = self.conn.cursor('tmp')
        curs.execute("""select 42 as i""")
        recs = curs.fetchmany(10)
        self.assertEqual(recs[0].i, 42)

    @skip_if_no_namedtuple
    def test_named_fetchall(self):
        curs = self.conn.cursor('tmp')
        curs.execute("""select 42 as i""")
        recs = curs.fetchall()
        self.assertEqual(recs[0].i, 42)

    @skip_if_no_namedtuple
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


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()
