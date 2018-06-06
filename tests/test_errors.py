#!/usr/bin/env python

# test_errors.py - unit test for psycopg2.errors module
#
# Copyright (C) 2018 Daniele Varrazzo  <daniele.varrazzo@gmail.com>
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

import unittest
from .testutils import ConnectingTestCase

import psycopg2


class ErrorsTests(ConnectingTestCase):
    def test_exception_class(self):
        cur = self.conn.cursor()
        try:
            cur.execute("select * from nonexist")
        except psycopg2.Error as exc:
            e = exc

        from psycopg2.errors import UndefinedTable
        self.assert_(isinstance(e, UndefinedTable), type(e))
        self.assert_(isinstance(e, self.conn.ProgrammingError))

    def test_exception_class_fallback(self):
        cur = self.conn.cursor()

        from psycopg2 import errors
        x = errors._by_sqlstate.pop('42P01')
        try:
            cur.execute("select * from nonexist")
        except psycopg2.Error as exc:
            e = exc
        finally:
            errors._by_sqlstate['42P01'] = x

        self.assertEqual(type(e), self.conn.ProgrammingError)


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()
