#!/usr/bin/env python

import unittest
import psycopg2
import psycopg2.extensions
import tests

class CursorTests(unittest.TestCase):

    def setUp(self):
        self.conn = psycopg2.connect(tests.dsn)

    def tearDown(self):
        self.conn.close()

    def test_executemany_propagate_exceptions(self):
        conn = self.conn
        cur = conn.cursor()
        cur.execute("create temp table test_exc (data int);")
        def buggygen():
            yield 1/0
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
        self.assertEqual("SELECT 'foo';", cur.mogrify(u"SELECT 'foo';"))

        conn.set_client_encoding('UTF8')
        snowman = u"\u2603"

        # unicode query with non-ascii data
        cur.execute(u"SELECT '%s';" % snowman)
        self.assertEqual(snowman.encode('utf8'), cur.fetchone()[0])
        self.assertEqual("SELECT '%s';" % snowman.encode('utf8'),
            cur.mogrify(u"SELECT '%s';" % snowman).replace("E'", "'"))

        # unicode args
        cur.execute("SELECT %s;", (snowman,))
        self.assertEqual(snowman.encode("utf-8"), cur.fetchone()[0])
        self.assertEqual("SELECT '%s';" % snowman.encode('utf8'),
            cur.mogrify("SELECT %s;", (snowman,)).replace("E'", "'"))

        # unicode query and args
        cur.execute(u"SELECT %s;", (snowman,))
        self.assertEqual(snowman.encode("utf-8"), cur.fetchone()[0])
        self.assertEqual("SELECT '%s';" % snowman.encode('utf8'),
            cur.mogrify(u"SELECT %s;", (snowman,)).replace("E'", "'"))

    def test_mogrify_decimal_explodes(self):
        # issue #7: explodes on windows with python 2.5 and psycopg 2.2.2
        try:
            from decimal import Decimal
        except:
            return

        conn = self.conn
        cur = conn.cursor()
        self.assertEqual('SELECT 10.3;',
            cur.mogrify("SELECT %s;", (Decimal("10.3"),)))


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()
