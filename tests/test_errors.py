#!/usr/bin/env python

# test_errors.py - unit test for psycopg2.errors module
#
# Copyright (C) 2018-2019 Daniele Varrazzo  <daniele.varrazzo@gmail.com>
# Copyright (C) 2020 The Psycopg Team
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
from psycopg2 import errors
from psycopg2._psycopg import sqlstate_errors
from psycopg2.errors import UndefinedTable


class ErrorsTests(ConnectingTestCase):
    def test_exception_class(self):
        cur = self.conn.cursor()
        try:
            cur.execute("select * from nonexist")
        except psycopg2.Error as exc:
            e = exc

        self.assert_(isinstance(e, UndefinedTable), type(e))
        self.assert_(isinstance(e, self.conn.ProgrammingError))

    def test_exception_class_fallback(self):
        cur = self.conn.cursor()

        x = sqlstate_errors.pop('42P01')
        try:
            cur.execute("select * from nonexist")
        except psycopg2.Error as exc:
            e = exc
        finally:
            sqlstate_errors['42P01'] = x

        self.assertEqual(type(e), self.conn.ProgrammingError)

    def test_lookup(self):
        self.assertIs(errors.lookup('42P01'), errors.UndefinedTable)

        with self.assertRaises(KeyError):
            errors.lookup('XXXXX')

    def test_has_base_exceptions(self):
        excs = []
        for n in dir(psycopg2):
            obj = getattr(psycopg2, n)
            if isinstance(obj, type) and issubclass(obj, Exception):
                excs.append(obj)

        self.assert_(len(excs) > 8, str(excs))

        excs.append(psycopg2.extensions.QueryCanceledError)
        excs.append(psycopg2.extensions.TransactionRollbackError)

        for exc in excs:
            self.assert_(hasattr(errors, exc.__name__))
            self.assert_(getattr(errors, exc.__name__) is exc)


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)


if __name__ == "__main__":
    unittest.main()
