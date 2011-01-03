#!/usr/bin/env python

import unittest
import psycopg2
import psycopg2.extensions
from psycopg2.extensions import b
from testconfig import dsn

class CursorTests(unittest.TestCase):

    def setUp(self):
        self.conn = psycopg2.connect(dsn)

    def tearDown(self):
        self.conn.close()

    def test_executemany_propagate_exceptions(self):
        conn = self.conn
        cur = conn.cursor()
        cur.execute("create temp table test_exc (data int);")
        def buggygen():
            yield 1//0
        self.assertRaises(ZeroDivisionError,
            cur.executemany, "insert into test_exc values (%s)", buggygen())
        cur.close()

    def test_mogrify_unicode(self):
        conn = self.conn
        cur = conn.cursor()

        # test consistency between execute and mogrify.

        # unicode query containing only ascii data
        cur.execute(u"SELECT 'foo';")
        self.assertEqual('foo', cur.fetchone()[0])
        self.assertEqual(b("SELECT 'foo';"), cur.mogrify(u"SELECT 'foo';"))

        conn.set_client_encoding('UTF8')
        snowman = u"\u2603"

        # unicode query with non-ascii data
        cur.execute(u"SELECT '%s';" % snowman)
        self.assertEqual(snowman.encode('utf8'), b(cur.fetchone()[0]))
        self.assertEqual(("SELECT '%s';" % snowman).encode('utf8'),
            cur.mogrify(u"SELECT '%s';" % snowman).replace(b("E'"), b("'")))

        # unicode args
        cur.execute("SELECT %s;", (snowman,))
        self.assertEqual(snowman.encode("utf-8"), b(cur.fetchone()[0]))
        self.assertEqual(("SELECT '%s';" % snowman).encode('utf8'),
            cur.mogrify("SELECT %s;", (snowman,)).replace(b("E'"), b("'")))

        # unicode query and args
        cur.execute(u"SELECT %s;", (snowman,))
        self.assertEqual(snowman.encode("utf-8"), b(cur.fetchone()[0]))
        self.assertEqual(("SELECT '%s';" % snowman).encode('utf8'),
            cur.mogrify(u"SELECT %s;", (snowman,)).replace(b("E'"), b("'")))

    def test_mogrify_decimal_explodes(self):
        # issue #7: explodes on windows with python 2.5 and psycopg 2.2.2
        try:
            from decimal import Decimal
        except:
            return

        conn = self.conn
        cur = conn.cursor()
        self.assertEqual(b('SELECT 10.3;'),
            cur.mogrify("SELECT %s;", (Decimal("10.3"),)))

    def test_cast(self):
        curs = self.conn.cursor()

        self.assertEqual(42, curs.cast(20, '42'))
        self.assertAlmostEqual(3.14, curs.cast(700, '3.14'))

        try:
            from decimal import Decimal
        except ImportError:
            self.assertAlmostEqual(123.45, curs.cast(1700, '123.45'))
        else:
            self.assertEqual(Decimal('123.45'), curs.cast(1700, '123.45'))

        from datetime import date
        self.assertEqual(date(2011,1,2), curs.cast(1082, '2011-01-02'))
        self.assertEqual("who am i?", curs.cast(705, 'who am i?'))  # unknown

    def test_cast_specificity(self):
        curs = self.conn.cursor()
        self.assertEqual("foo", curs.cast(705, 'foo'))

        D = psycopg2.extensions.new_type((705,), "DOUBLING", lambda v, c: v * 2)
        psycopg2.extensions.register_type(D, self.conn)
        self.assertEqual("foofoo", curs.cast(705, 'foo'))

        T = psycopg2.extensions.new_type((705,), "TREBLING", lambda v, c: v * 3)
        psycopg2.extensions.register_type(T, curs)
        self.assertEqual("foofoofoo", curs.cast(705, 'foo'))

        curs2 = self.conn.cursor()
        self.assertEqual("foofoo", curs2.cast(705, 'foo'))

    def test_weakref(self):
        from weakref import ref
        curs = self.conn.cursor()
        w = ref(curs)
        del curs
        self.assert_(w() is None)


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()
