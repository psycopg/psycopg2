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
import re
import sys
from testutils import unittest

import psycopg2
import psycopg2.extras
import tests


def skip_if_no_uuid(f):
    def skip_if_no_uuid_(self):
        try:
            import uuid
        except ImportError:
            return self.skipTest("uuid not available in this Python version")

        try:
            cur = self.conn.cursor()
            cur.execute("select typname from pg_type where typname = 'uuid'")
            has = cur.fetchone()
        finally:
            self.conn.rollback()

        if has:
            return f(self)
        else:
            return self.skipTest("uuid type not available on the server")

    return skip_if_no_uuid_

def filter_scs(conn, s):
    if conn.get_parameter_status("standard_conforming_strings") == 'off':
        return s
    else:
        return s.replace("E'", "'")

class TypesExtrasTests(unittest.TestCase):
    """Test that all type conversions are working."""

    def setUp(self):
        self.conn = psycopg2.connect(tests.dsn)

    def tearDown(self):
        self.conn.close()

    def execute(self, *args):
        curs = self.conn.cursor()
        curs.execute(*args)
        return curs.fetchone()[0]

    @skip_if_no_uuid
    def testUUID(self):
        import uuid
        psycopg2.extras.register_uuid()
        u = uuid.UUID('9c6d5a77-7256-457e-9461-347b4358e350')
        s = self.execute("SELECT %s AS foo", (u,))
        self.failUnless(u == s)
        # must survive NULL cast to a uuid
        s = self.execute("SELECT NULL::uuid AS foo")
        self.failUnless(s is None)

    @skip_if_no_uuid
    def testUUIDARRAY(self):
        import uuid
        psycopg2.extras.register_uuid()
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

    def test_inet_conform(self):
        from psycopg2.extras import Inet
        i = Inet("192.168.1.0/24")
        a = psycopg2.extensions.adapt(i)
        a.prepare(self.conn)
        self.assertEqual(
            filter_scs(self.conn, "E'192.168.1.0/24'::inet"),
            a.getquoted())

        # adapts ok with unicode too
        i = Inet(u"192.168.1.0/24")
        a = psycopg2.extensions.adapt(i)
        a.prepare(self.conn)
        self.assertEqual(
            filter_scs(self.conn, "E'192.168.1.0/24'::inet"),
            a.getquoted())

    def test_adapt_fail(self):
        class Foo(object): pass
        self.assertRaises(psycopg2.ProgrammingError,
            psycopg2.extensions.adapt, Foo(), psycopg2.extensions.ISQLQuote, None)
        try:
            psycopg2.extensions.adapt(Foo(), psycopg2.extensions.ISQLQuote, None)
        except psycopg2.ProgrammingError, err:
            self.failUnless(str(err) == "can't adapt type 'Foo'")


def skip_if_no_hstore(f):
    def skip_if_no_hstore_(self):
        from psycopg2.extras import HstoreAdapter
        oids = HstoreAdapter.get_oids(self.conn)
        if oids is None:
            return self.skipTest("hstore not available in test database")
        return f(self)

    return skip_if_no_hstore_

class HstoreTestCase(unittest.TestCase):
    def setUp(self):
        self.conn = psycopg2.connect(tests.dsn)

    def tearDown(self):
        self.conn.close()

    def test_adapt_8(self):
        if self.conn.server_version >= 90000:
            return self.skipTest("skipping dict adaptation with PG pre-9 syntax")

        from psycopg2.extras import HstoreAdapter

        o = {'a': '1', 'b': "'", 'c': None}
        if self.conn.encoding == 'UTF8':
            o['d'] = u'\xe0'

        a = HstoreAdapter(o)
        a.prepare(self.conn)
        q = a.getquoted()

        self.assert_(q.startswith("(("), q)
        self.assert_(q.endswith("))"), q)
        ii = q[1:-1].split("||")
        ii.sort()

        self.assertEqual(len(ii), len(o))
        self.assertEqual(ii[0], filter_scs(self.conn, "(E'a' => E'1')"))
        self.assertEqual(ii[1], filter_scs(self.conn, "(E'b' => E'''')"))
        self.assertEqual(ii[2], filter_scs(self.conn, "(E'c' => NULL)"))
        if 'd' in o:
            encc = u'\xe0'.encode(psycopg2.extensions.encodings[self.conn.encoding])
            self.assertEqual(ii[3], filter_scs(self.conn, "(E'd' => E'%s')" % encc))

    def test_adapt_9(self):
        if self.conn.server_version < 90000:
            return self.skipTest("skipping dict adaptation with PG 9 syntax")

        from psycopg2.extras import HstoreAdapter

        o = {'a': '1', 'b': "'", 'c': None}
        if self.conn.encoding == 'UTF8':
            o['d'] = u'\xe0'

        a = HstoreAdapter(o)
        a.prepare(self.conn)
        q = a.getquoted()

        m = re.match(r'hstore\(ARRAY\[([^\]]+)\], ARRAY\[([^\]]+)\]\)', q)
        self.assert_(m, repr(q))

        kk = m.group(1).split(", ")
        vv = m.group(2).split(", ")
        ii = zip(kk, vv)
        ii.sort()

        self.assertEqual(len(ii), len(o))
        self.assertEqual(ii[0], ("E'a'", "E'1'"))
        self.assertEqual(ii[1], ("E'b'", "E''''"))
        self.assertEqual(ii[2], ("E'c'", "NULL"))
        if 'd' in o:
            encc = u'\xe0'.encode(psycopg2.extensions.encodings[self.conn.encoding])
            self.assertEqual(ii[3], ("E'd'", "E'%s'" % encc))

    def test_parse(self):
        from psycopg2.extras import HstoreAdapter

        def ok(s, d):
            self.assertEqual(HstoreAdapter.parse(s, None), d)

        ok(None, None)
        ok('', {})
        ok('"a"=>"1", "b"=>"2"', {'a': '1', 'b': '2'})
        ok('"a"  => "1" ,"b"  =>  "2"', {'a': '1', 'b': '2'})
        ok('"a"=>NULL, "b"=>"2"', {'a': None, 'b': '2'})
        ok(r'"a"=>"\"", "\""=>"2"', {'a': '"', '"': '2'})
        ok('"a"=>"\'", "\'"=>"2"', {'a': "'", "'": '2'})
        ok('"a"=>"1", "b"=>NULL', {'a': '1', 'b': None})
        ok(r'"a\\"=>"1"', {'a\\': '1'})
        ok(r'"a\""=>"1"', {'a"': '1'})
        ok(r'"a\\\""=>"1"', {r'a\"': '1'})
        ok(r'"a\\\\\""=>"1"', {r'a\\"': '1'})

        def ko(s):
            self.assertRaises(psycopg2.InterfaceError,
                HstoreAdapter.parse, s, None)

        ko('a')
        ko('"a"')
        ko(r'"a\\""=>"1"')
        ko(r'"a\\\\""=>"1"')
        ko('"a=>"1"')
        ko('"a"=>"1", "b"=>NUL')

    @skip_if_no_hstore
    def test_register_conn(self):
        from psycopg2.extras import register_hstore

        register_hstore(self.conn)
        cur = self.conn.cursor()
        cur.execute("select null::hstore, ''::hstore, 'a => b'::hstore")
        t = cur.fetchone()
        self.assert_(t[0] is None)
        self.assertEqual(t[1], {})
        self.assertEqual(t[2], {'a': 'b'})

    @skip_if_no_hstore
    def test_register_curs(self):
        from psycopg2.extras import register_hstore

        cur = self.conn.cursor()
        register_hstore(cur)
        cur.execute("select null::hstore, ''::hstore, 'a => b'::hstore")
        t = cur.fetchone()
        self.assert_(t[0] is None)
        self.assertEqual(t[1], {})
        self.assertEqual(t[2], {'a': 'b'})

    @skip_if_no_hstore
    def test_register_unicode(self):
        from psycopg2.extras import register_hstore

        register_hstore(self.conn, unicode=True)
        cur = self.conn.cursor()
        cur.execute("select null::hstore, ''::hstore, 'a => b'::hstore")
        t = cur.fetchone()
        self.assert_(t[0] is None)
        self.assertEqual(t[1], {})
        self.assertEqual(t[2], {u'a': u'b'})
        self.assert_(isinstance(t[2].keys()[0], unicode))
        self.assert_(isinstance(t[2].values()[0], unicode))

    @skip_if_no_hstore
    def test_register_globally(self):
        from psycopg2.extras import register_hstore, HstoreAdapter

        oids = HstoreAdapter.get_oids(self.conn)
        try:
            register_hstore(self.conn, globally=True)
            conn2 = psycopg2.connect(self.conn.dsn)
            try:
                cur2 = self.conn.cursor()
                cur2.execute("select 'a => b'::hstore")
                r = cur2.fetchone()
                self.assert_(isinstance(r[0], dict))
            finally:
                conn2.close()
        finally:
            psycopg2.extensions.string_types.pop(oids[0])

        # verify the caster is not around anymore
        cur = self.conn.cursor()
        cur.execute("select 'a => b'::hstore")
        r = cur.fetchone()
        self.assert_(isinstance(r[0], str))

    @skip_if_no_hstore
    def test_roundtrip(self):
        from psycopg2.extras import register_hstore
        register_hstore(self.conn)
        cur = self.conn.cursor()

        def ok(d):
            cur.execute("select %s", (d,))
            d1 = cur.fetchone()[0]
            self.assertEqual(len(d), len(d1))
            for k in d:
                self.assert_(k in d1, k)
                self.assertEqual(d[k], d1[k])

        ok({})
        ok({'a': 'b', 'c': None})

        ab = map(chr, range(32, 128))
        ok(dict(zip(ab, ab)))
        ok({''.join(ab): ''.join(ab)})

        self.conn.set_client_encoding('latin1')
        ab = map(chr, range(1, 256))
        ok({''.join(ab): ''.join(ab)})
        ok(dict(zip(ab, ab)))

    @skip_if_no_hstore
    def test_roundtrip_unicode(self):
        from psycopg2.extras import register_hstore
        register_hstore(self.conn, unicode=True)
        cur = self.conn.cursor()

        def ok(d):
            cur.execute("select %s", (d,))
            d1 = cur.fetchone()[0]
            self.assertEqual(len(d), len(d1))
            for k, v in d1.iteritems():
                self.assert_(k in d, k)
                self.assertEqual(d[k], v)
                self.assert_(isinstance(k, unicode))
                self.assert_(v is None or isinstance(v, unicode))

        ok({})
        ok({'a': 'b', 'c': None, 'd': u'\u20ac', u'\u2603': 'e'})

        ab = map(unichr, range(1, 1024))
        ok({u''.join(ab): u''.join(ab)})
        ok(dict(zip(ab, ab)))


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()

