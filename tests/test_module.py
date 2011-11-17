#!/usr/bin/env python

# test_module.py - unit test for the module interface
#
# Copyright (C) 2011 Daniele Varrazzo <daniele.varrazzo@gmail.com>
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

from testutils import unittest

import psycopg2

class ConnectTestCase(unittest.TestCase):
    def setUp(self):
        self.args = None
        def conect_stub(dsn, connection_factory=None, async=False):
            self.args = (dsn, connection_factory, async)

        self._connect_orig = psycopg2._connect
        psycopg2._connect = conect_stub

    def tearDown(self):
        psycopg2._connect = self._connect_orig

    def test_there_has_to_be_something(self):
        self.assertRaises(psycopg2.InterfaceError, psycopg2.connect)
        self.assertRaises(psycopg2.InterfaceError, psycopg2.connect,
            connection_factory=lambda dsn, async=False: None)
        self.assertRaises(psycopg2.InterfaceError, psycopg2.connect,
            async=True)

    def test_no_keywords(self):
        psycopg2.connect('')
        self.assertEqual(self.args[0], '')
        self.assertEqual(self.args[1], None)
        self.assertEqual(self.args[2], False)

    def test_dsn(self):
        psycopg2.connect('dbname=blah x=y')
        self.assertEqual(self.args[0], 'dbname=blah x=y')
        self.assertEqual(self.args[1], None)
        self.assertEqual(self.args[2], False)

    def test_supported_keywords(self):
        psycopg2.connect(database='foo')
        self.assertEqual(self.args[0], 'dbname=foo')
        psycopg2.connect(user='postgres')
        self.assertEqual(self.args[0], 'user=postgres')
        psycopg2.connect(password='secret')
        self.assertEqual(self.args[0], 'password=secret')
        psycopg2.connect(port=5432)
        self.assertEqual(self.args[0], 'port=5432')
        psycopg2.connect(sslmode='require')
        self.assertEqual(self.args[0], 'sslmode=require')

        psycopg2.connect(database='foo',
            user='postgres', password='secret', port=5432)
        self.assert_('dbname=foo' in self.args[0])
        self.assert_('user=postgres' in self.args[0])
        self.assert_('password=secret' in self.args[0])
        self.assert_('port=5432' in self.args[0])
        self.assertEqual(len(self.args[0].split()), 4)

    def test_generic_keywords(self):
        psycopg2.connect(foo='bar')
        self.assertEqual(self.args[0], 'foo=bar')

    def test_factory(self):
        def f(dsn, async=False):
            pass

        psycopg2.connect(database='foo', bar='baz', connection_factory=f)
        self.assertEqual(self.args[0], 'dbname=foo bar=baz')
        self.assertEqual(self.args[1], f)
        self.assertEqual(self.args[2], False)

        psycopg2.connect("dbname=foo bar=baz", connection_factory=f)
        self.assertEqual(self.args[0], 'dbname=foo bar=baz')
        self.assertEqual(self.args[1], f)
        self.assertEqual(self.args[2], False)

    def test_async(self):
        psycopg2.connect(database='foo', bar='baz', async=1)
        self.assertEqual(self.args[0], 'dbname=foo bar=baz')
        self.assertEqual(self.args[1], None)
        self.assert_(self.args[2])

        psycopg2.connect("dbname=foo bar=baz", async=True)
        self.assertEqual(self.args[0], 'dbname=foo bar=baz')
        self.assertEqual(self.args[1], None)
        self.assert_(self.args[2])

    def test_empty_param(self):
        psycopg2.connect(database='sony', password='')
        self.assertEqual(self.args[0], "dbname=sony password=''")

    def test_escape(self):
        psycopg2.connect(database='hello world')
        self.assertEqual(self.args[0], "dbname='hello world'")

        psycopg2.connect(database=r'back\slash')
        self.assertEqual(self.args[0], r"dbname=back\\slash")

        psycopg2.connect(database="quo'te")
        self.assertEqual(self.args[0], r"dbname=quo\'te")

        psycopg2.connect(database="with\ttab")
        self.assertEqual(self.args[0], "dbname='with\ttab'")

        psycopg2.connect(database=r"\every thing'")
        self.assertEqual(self.args[0], r"dbname='\\every thing\''")


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()
