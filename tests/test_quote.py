#!/usr/bin/env python

# test_quote.py - unit test for strings quoting
#
# Copyright (C) 2007-2011 Daniele Varrazzo  <daniele.varrazzo@gmail.com>
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

import sys
from testutils import unittest
from testconfig import dsn

import psycopg2
import psycopg2.extensions
from psycopg2.extensions import b

class QuotingTestCase(unittest.TestCase):
    r"""Checks the correct quoting of strings and binary objects.

    Since ver. 8.1, PostgreSQL is moving towards SQL standard conforming
    strings, where the backslash (\) is treated as literal character,
    not as escape. To treat the backslash as a C-style escapes, PG supports
    the E'' quotes.

    This test case checks that the E'' quotes are used whenever they are
    needed. The tests are expected to pass with all PostgreSQL server versions
    (currently tested with 7.4 <= PG <= 8.3beta) and with any
    'standard_conforming_strings' server parameter value.
    The tests also check that no warning is raised ('escape_string_warning'
    should be on).

    http://www.postgresql.org/docs/current/static/sql-syntax-lexical.html#SQL-SYNTAX-STRINGS
    http://www.postgresql.org/docs/current/static/runtime-config-compatible.html
    """
    def setUp(self):
        self.conn = psycopg2.connect(dsn)

    def tearDown(self):
        self.conn.close()

    def test_string(self):
        data = """some data with \t chars
        to escape into, 'quotes' and \\ a backslash too.
        """
        data += "".join(map(chr, range(1,127)))

        curs = self.conn.cursor()
        curs.execute("SELECT %s;", (data,))
        res = curs.fetchone()[0]

        self.assertEqual(res, data)
        self.assert_(not self.conn.notices)

    def test_binary(self):
        data = b("""some data with \000\013 binary
        stuff into, 'quotes' and \\ a backslash too.
        """)
        if sys.version_info[0] < 3:
            data += "".join(map(chr, range(256)))
        else:
            data += bytes(range(256))

        curs = self.conn.cursor()
        curs.execute("SELECT %s::bytea;", (psycopg2.Binary(data),))
        if sys.version_info[0] < 3:
            res = str(curs.fetchone()[0])
        else:
            res = curs.fetchone()[0].tobytes()

        if res[0] in (b('x'), ord(b('x'))) and self.conn.server_version >= 90000:
            return self.skipTest(
                "bytea broken with server >= 9.0, libpq < 9")

        self.assertEqual(res, data)
        self.assert_(not self.conn.notices)

    def test_unicode(self):
        curs = self.conn.cursor()
        curs.execute("SHOW server_encoding")
        server_encoding = curs.fetchone()[0]
        if server_encoding != "UTF8":
            return self.skipTest(
                "Unicode test skipped since server encoding is %s"
                    % server_encoding)

        data = u"""some data with \t chars
        to escape into, 'quotes', \u20ac euro sign and \\ a backslash too.
        """
        data += u"".join(map(unichr, [ u for u in range(1,65536)
            if not 0xD800 <= u <= 0xDFFF ]))    # surrogate area
        self.conn.set_client_encoding('UNICODE')

        psycopg2.extensions.register_type(psycopg2.extensions.UNICODE, self.conn)
        curs.execute("SELECT %s::text;", (data,))
        res = curs.fetchone()[0]

        self.assertEqual(res, data)
        self.assert_(not self.conn.notices)

    def test_latin1(self):
        self.conn.set_client_encoding('LATIN1')
        curs = self.conn.cursor()
        if sys.version_info[0] < 3:
            data = ''.join(map(chr, range(32, 127) + range(160, 256)))
        else:
            data = bytes(range(32, 127) + range(160, 256)).decode('latin1')

        # as string
        curs.execute("SELECT %s::text;", (data,))
        res = curs.fetchone()[0]
        self.assertEqual(res, data)
        self.assert_(not self.conn.notices)

        # as unicode
        if sys.version_info[0] < 3:
            psycopg2.extensions.register_type(psycopg2.extensions.UNICODE, self.conn)
            data = data.decode('latin1')

            curs.execute("SELECT %s::text;", (data,))
            res = curs.fetchone()[0]
            self.assertEqual(res, data)
            self.assert_(not self.conn.notices)

    def test_koi8(self):
        self.conn.set_client_encoding('KOI8')
        curs = self.conn.cursor()
        if sys.version_info[0] < 3:
            data = ''.join(map(chr, range(32, 127) + range(128, 256)))
        else:
            data = bytes(range(32, 127) + range(128, 256)).decode('koi8_r')

        # as string
        curs.execute("SELECT %s::text;", (data,))
        res = curs.fetchone()[0]
        self.assertEqual(res, data)
        self.assert_(not self.conn.notices)

        # as unicode
        if sys.version_info[0] < 3:
            psycopg2.extensions.register_type(psycopg2.extensions.UNICODE, self.conn)
            data = data.decode('koi8_r')

            curs.execute("SELECT %s::text;", (data,))
            res = curs.fetchone()[0]
            self.assertEqual(res, data)
            self.assert_(not self.conn.notices)


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()

