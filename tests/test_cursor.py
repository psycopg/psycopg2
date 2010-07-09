#!/usr/bin/env python

import unittest
import psycopg2
import psycopg2.extensions
import tests

class CursorTests(unittest.TestCase):

    def connect(self):
        return psycopg2.connect(tests.dsn)

    def test_executemany_propagate_exceptions(self):
        conn = self.connect()
        cur = conn.cursor()
        cur.execute("create temp table test_exc (data int);")
        def buggygen():
            yield 1/0
        self.assertRaises(ZeroDivisionError,
            cur.executemany, "insert into test_exc values (%s)", buggygen())
        cur.close()
        conn.close()


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()
