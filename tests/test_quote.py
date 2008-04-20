#!/usr/bin/env python
import unittest
import warnings

import psycopg2
import psycopg2.extensions
import tests

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

    http://www.postgresql.org/docs/8.1/static/sql-syntax.html#SQL-SYNTAX-STRINGS
    http://www.postgresql.org/docs/8.1/static/runtime-config-compatible.html
    """
    def setUp(self):
        self.conn = psycopg2.connect(tests.dsn)

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
        data = """some data with \000\013 binary
        stuff into, 'quotes' and \\ a backslash too.
        """
        data += "".join(map(chr, range(256)))

        curs = self.conn.cursor()
        curs.execute("SELECT %s::bytea;", (psycopg2.Binary(data),))
        res = str(curs.fetchone()[0])

        self.assertEqual(res, data)
        self.assert_(not self.conn.notices)

    def test_unicode(self):
        curs = self.conn.cursor()
        curs.execute("SHOW server_encoding")
        server_encoding = curs.fetchone()[0]
        if server_encoding != "UTF8":
            warnings.warn("Unicode test skipped since server encoding is %s"
                          % server_encoding)
            return

        data = u"""some data with \t chars
        to escape into, 'quotes', \u20ac euro sign and \\ a backslash too.
        """
        data += u"".join(map(unichr, [ u for u in range(1,65536)
            if not 0xD800 <= u <= 0xDFFF ]))    # surrogate area
        self.conn.set_client_encoding('UNICODE')

        psycopg2.extensions.register_type(psycopg2.extensions.UNICODE)
        curs.execute("SELECT %s::text;", (data,))
        res = curs.fetchone()[0]

        self.assertEqual(res, data)
        self.assert_(not self.conn.notices)

def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()

