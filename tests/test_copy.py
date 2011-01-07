#!/usr/bin/env python
import os
import string
from testutils import unittest, decorate_all_tests
from cStringIO import StringIO
from itertools import cycle, izip

import psycopg2
import psycopg2.extensions
import tests

def skip_if_green(f):
    def skip_if_green_(self):
        if tests.green:
            return self.skipTest("copy in async mode currently not supported")
        else:
            return f(self)

    return skip_if_green_


class MinimalRead(object):
    """A file wrapper exposing the minimal interface to copy from."""
    def __init__(self, f):
        self.f = f

    def read(self, size):
        return self.f.read(size)

    def readline(self):
        return self.f.readline()

class MinimalWrite(object):
    """A file wrapper exposing the minimal interface to copy to."""
    def __init__(self, f):
        self.f = f

    def write(self, data):
        return self.f.write(data)


class CopyTests(unittest.TestCase):

    def setUp(self):
        self.conn = psycopg2.connect(tests.dsn)
        curs = self.conn.cursor()
        curs.execute('''
            CREATE TEMPORARY TABLE tcopy (
              id int PRIMARY KEY,
              data text
            )''')

    def tearDown(self):
        self.conn.close()

    def test_copy_from(self):
        curs = self.conn.cursor()
        try:
            self._copy_from(curs, nrecs=1024, srec=10*1024, copykw={})
        finally:
            curs.close()

    def test_copy_from_insane_size(self):
        # Trying to trigger a "would block" error
        curs = self.conn.cursor()
        try:
            self._copy_from(curs, nrecs=10*1024, srec=10*1024,
                copykw={'size': 20*1024*1024})
        finally:
            curs.close()

    def test_copy_from_cols(self):
        curs = self.conn.cursor()
        f = StringIO()
        for i in xrange(10):
            f.write("%s\n" % (i,))

        f.seek(0)
        curs.copy_from(MinimalRead(f), "tcopy", columns=['id'])

        curs.execute("select * from tcopy order by id")
        self.assertEqual([(i, None) for i in range(10)], curs.fetchall())

    def test_copy_from_cols_err(self):
        curs = self.conn.cursor()
        f = StringIO()
        for i in xrange(10):
            f.write("%s\n" % (i,))

        f.seek(0)
        def cols():
            raise ZeroDivisionError()
            yield 'id'

        self.assertRaises(ZeroDivisionError,
            curs.copy_from, MinimalRead(f), "tcopy", columns=cols())

    def test_copy_to(self):
        curs = self.conn.cursor()
        try:
            self._copy_from(curs, nrecs=1024, srec=10*1024, copykw={})
            self._copy_to(curs, srec=10*1024)
        finally:
            curs.close()

    def _copy_from(self, curs, nrecs, srec, copykw):
        f = StringIO()
        for i, c in izip(xrange(nrecs), cycle(string.letters)):
            l = c * srec
            f.write("%s\t%s\n" % (i,l))

        f.seek(0)
        curs.copy_from(MinimalRead(f), "tcopy", **copykw)

        curs.execute("select count(*) from tcopy")
        self.assertEqual(nrecs, curs.fetchone()[0])

        curs.execute("select data from tcopy where id < %s order by id",
                (len(string.letters),))
        for i, (l,) in enumerate(curs):
            self.assertEqual(l, string.letters[i] * srec)

    def _copy_to(self, curs, srec):
        f = StringIO()
        curs.copy_to(MinimalWrite(f), "tcopy")

        f.seek(0)
        ntests = 0
        for line in f:
            n, s = line.split()
            if int(n) < len(string.letters):
                self.assertEqual(s, string.letters[int(n)] * srec)
                ntests += 1

        self.assertEqual(ntests, len(string.letters))

decorate_all_tests(CopyTests, skip_if_green)


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()
