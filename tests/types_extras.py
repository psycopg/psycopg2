#!/usr/bin/env python
#
# types_extras.py - tests for extras types conversions
#
# Copyright (C) 2008-2010 Federico Di Gregorio  <fog@debian.org>
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

try:
    import decimal
except:
    pass
import sys
import unittest

import psycopg2
import psycopg2.extras
import tests


class TypesExtrasTests(unittest.TestCase):
    """Test that all type conversions are working."""

    def setUp(self):
        self.conn = psycopg2.connect(tests.dsn)

    def execute(self, *args):
        curs = self.conn.cursor()
        curs.execute(*args)
        return curs.fetchone()[0]

    def testUUID(self):
        try:
            import uuid
            psycopg2.extras.register_uuid()
        except:
            return
        u = uuid.UUID('9c6d5a77-7256-457e-9461-347b4358e350')
        s = self.execute("SELECT %s AS foo", (u,))
        self.failUnless(u == s)
        # must survive NULL cast to a uuid
        s = self.execute("SELECT NULL::uuid AS foo")
        self.failUnless(s is None)

    def testUUIDARRAY(self):
        try:
            import uuid
            psycopg2.extras.register_uuid()
        except:
            return
        u = [uuid.UUID('9c6d5a77-7256-457e-9461-347b4358e350'), uuid.UUID('9c6d5a77-7256-457e-9461-347b4358e352')]
        s = self.execute("SELECT %s AS foo", (u,))
        self.failUnless(u == s)
        # array with a NULL element
        u = [uuid.UUID('9c6d5a77-7256-457e-9461-347b4358e350'), None]
        s = self.execute("SELECT %s AS foo", (u,))
        self.failUnless(u == s)
        # must survive NULL cast to a uuid[]
        s = self.execute("SELECT NULL::uuid[] AS foo")
        self.failUnless(s is None)
        # what about empty arrays?
        s = self.execute("SELECT '{}'::uuid[] AS foo")
        self.failUnless(type(s) == list and len(s) == 0)

    def testINET(self):
        psycopg2.extras.register_inet()
        i = "192.168.1.0/24";
        s = self.execute("SELECT %s AS foo", (i,))
        self.failUnless(i == s)
        # must survive NULL cast to inet
        s = self.execute("SELECT NULL::inet AS foo")
        self.failUnless(s is None)

    def test_adapt_fail(self):
        class Foo(object): pass
        self.assertRaises(psycopg2.ProgrammingError,
            psycopg2.extensions.adapt, Foo(), psycopg2.extensions.ISQLQuote, None)
        try:
            psycopg2.extensions.adapt(Foo(), psycopg2.extensions.ISQLQuote, None)
        except psycopg2.ProgrammingError, err:
            self.failUnless(str(err) == "can't adapt type 'Foo'")

def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()

