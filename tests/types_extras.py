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
from datetime import date

from testutils import unittest, skip_if_no_uuid, skip_before_postgres

import psycopg2
import psycopg2.extras
from psycopg2.extensions import b

from testconfig import dsn


def filter_scs(conn, s):
    if conn.get_parameter_status("standard_conforming_strings") == 'off':
        return s
    else:
        return s.replace(b("E'"), b("'"))

class TypesExtrasTests(unittest.TestCase):
    """Test that all type conversions are working."""

    def setUp(self):
        self.conn = psycopg2.connect(dsn)

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
            filter_scs(self.conn, b("E'192.168.1.0/24'::inet")),
            a.getquoted())

        # adapts ok with unicode too
        i = Inet(u"192.168.1.0/24")
        a = psycopg2.extensions.adapt(i)
        a.prepare(self.conn)
        self.assertEqual(
            filter_scs(self.conn, b("E'192.168.1.0/24'::inet")),
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
        if oids is None or not oids[0]:
            return self.skipTest("hstore not available in test database")
        return f(self)

    return skip_if_no_hstore_

class HstoreTestCase(unittest.TestCase):
    def setUp(self):
        self.conn = psycopg2.connect(dsn)

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

        self.assert_(q.startswith(b("((")), q)
        ii = q[1:-1].split(b("||"))
        ii.sort()

        self.assertEqual(len(ii), len(o))
        self.assertEqual(ii[0], filter_scs(self.conn, b("(E'a' => E'1')")))
        self.assertEqual(ii[1], filter_scs(self.conn, b("(E'b' => E'''')")))
        self.assertEqual(ii[2], filter_scs(self.conn, b("(E'c' => NULL)")))
        if 'd' in o:
            encc = u'\xe0'.encode(psycopg2.extensions.encodings[self.conn.encoding])
            self.assertEqual(ii[3], filter_scs(self.conn, b("(E'd' => E'") + encc + b("')")))

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

        m = re.match(b(r'hstore\(ARRAY\[([^\]]+)\], ARRAY\[([^\]]+)\]\)'), q)
        self.assert_(m, repr(q))

        kk = m.group(1).split(b(", "))
        vv = m.group(2).split(b(", "))
        ii = zip(kk, vv)
        ii.sort()

        def f(*args):
            return tuple([filter_scs(self.conn, s) for s in args])

        self.assertEqual(len(ii), len(o))
        self.assertEqual(ii[0], f(b("E'a'"), b("E'1'")))
        self.assertEqual(ii[1], f(b("E'b'"), b("E''''")))
        self.assertEqual(ii[2], f(b("E'c'"), b("NULL")))
        if 'd' in o:
            encc = u'\xe0'.encode(psycopg2.extensions.encodings[self.conn.encoding])
            self.assertEqual(ii[3], f(b("E'd'"), b("E'") + encc + b("'")))

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
            conn2 = psycopg2.connect(dsn)
            try:
                cur2 = self.conn.cursor()
                cur2.execute("select 'a => b'::hstore")
                r = cur2.fetchone()
                self.assert_(isinstance(r[0], dict))
            finally:
                conn2.close()
        finally:
            psycopg2.extensions.string_types.pop(oids[0][0])

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
        if sys.version_info[0] < 3:
            ab = map(chr, range(32, 127) + range(160, 255))
        else:
            ab = bytes(range(32, 127) + range(160, 255)).decode('latin1')

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

    @skip_if_no_hstore
    def test_oid(self):
        cur = self.conn.cursor()
        cur.execute("select 'hstore'::regtype::oid")
        oid = cur.fetchone()[0]

        # Note: None as conn_or_cursor is just for testing: not public
        # interface and it may break in future.
        from psycopg2.extras import register_hstore
        register_hstore(None, globally=True, oid=oid)
        try:
            cur.execute("select null::hstore, ''::hstore, 'a => b'::hstore")
            t = cur.fetchone()
            self.assert_(t[0] is None)
            self.assertEqual(t[1], {})
            self.assertEqual(t[2], {'a': 'b'})

        finally:
            psycopg2.extensions.string_types.pop(oid)

    @skip_if_no_hstore
    @skip_before_postgres(8, 3)
    def test_roundtrip_array(self):
        from psycopg2.extras import register_hstore
        register_hstore(self.conn)

        ds = []
        ds.append({})
        ds.append({'a': 'b', 'c': None})

        ab = map(chr, range(32, 128))
        ds.append(dict(zip(ab, ab)))
        ds.append({''.join(ab): ''.join(ab)})

        self.conn.set_client_encoding('latin1')
        if sys.version_info[0] < 3:
            ab = map(chr, range(32, 127) + range(160, 255))
        else:
            ab = bytes(range(32, 127) + range(160, 255)).decode('latin1')

        ds.append({''.join(ab): ''.join(ab)})
        ds.append(dict(zip(ab, ab)))

        cur = self.conn.cursor()
        cur.execute("select %s", (ds,))
        ds1 = cur.fetchone()[0]
        self.assertEqual(ds, ds1)

    @skip_if_no_hstore
    @skip_before_postgres(8, 3)
    def test_array_cast(self):
        from psycopg2.extras import register_hstore
        register_hstore(self.conn)
        cur = self.conn.cursor()
        cur.execute("select array['a=>1'::hstore, 'b=>2'::hstore];")
        a = cur.fetchone()[0]
        self.assertEqual(a, [{'a': '1'}, {'b': '2'}])

    @skip_if_no_hstore
    def test_array_cast_oid(self):
        cur = self.conn.cursor()
        cur.execute("select 'hstore'::regtype::oid, 'hstore[]'::regtype::oid")
        oid, aoid = cur.fetchone()

        from psycopg2.extras import register_hstore
        register_hstore(None, globally=True, oid=oid, array_oid=aoid)
        try:
            cur.execute("select null::hstore, ''::hstore, 'a => b'::hstore, '{a=>b}'::hstore[]")
            t = cur.fetchone()
            self.assert_(t[0] is None)
            self.assertEqual(t[1], {})
            self.assertEqual(t[2], {'a': 'b'})
            self.assertEqual(t[3], [{'a': 'b'}])

        finally:
            psycopg2.extensions.string_types.pop(oid)
            psycopg2.extensions.string_types.pop(aoid)

def skip_if_no_composite(f):
    def skip_if_no_composite_(self):
        if self.conn.server_version < 80000:
            return self.skipTest(
                "server version %s doesn't support composite types"
                % self.conn.server_version)

        return f(self)

    skip_if_no_composite_.__name__ = f.__name__
    return skip_if_no_composite_

class AdaptTypeTestCase(unittest.TestCase):
    def setUp(self):
        self.conn = psycopg2.connect(dsn)

    def tearDown(self):
        self.conn.close()

    @skip_if_no_composite
    def test_none_in_record(self):
        curs = self.conn.cursor()
        s = curs.mogrify("SELECT %s;", [(42, None)])
        self.assertEqual(b("SELECT (42, NULL);"), s)
        curs.execute("SELECT %s;", [(42, None)])
        d = curs.fetchone()[0]
        self.assertEqual("(42,)", d)

    def test_none_fast_path(self):
        # the None adapter is not actually invoked in regular adaptation
        ext = psycopg2.extensions

        class WonkyAdapter(object):
            def __init__(self, obj): pass
            def getquoted(self): return "NOPE!"

        curs = self.conn.cursor()

        orig_adapter = ext.adapters[type(None), ext.ISQLQuote]
        try:
            ext.register_adapter(type(None), WonkyAdapter)
            self.assertEqual(ext.adapt(None).getquoted(), "NOPE!")

            s = curs.mogrify("SELECT %s;", (None,))
            self.assertEqual(b("SELECT NULL;"), s)

        finally:
            ext.register_adapter(type(None), orig_adapter)

    def test_tokenization(self):
        from psycopg2.extras import CompositeCaster
        def ok(s, v):
            self.assertEqual(CompositeCaster.tokenize(s), v)

        ok("(,)", [None, None])
        ok('(hello,,10.234,2010-11-11)', ['hello', None, '10.234', '2010-11-11'])
        ok('(10,"""")', ['10', '"'])
        ok('(10,",")', ['10', ','])
        ok(r'(10,"\\")', ['10', '\\'])
        ok(r'''(10,"\\',""")''', ['10', '''\\',"'''])
        ok('(10,"(20,""(30,40)"")")', ['10', '(20,"(30,40)")'])
        ok('(10,"(20,""(30,""""(40,50)"""")"")")', ['10', '(20,"(30,""(40,50)"")")'])
        ok('(,"(,""(a\nb\tc)"")")', [None, '(,"(a\nb\tc)")'])
        ok('(\x01,\x02,\x03,\x04,\x05,\x06,\x07,\x08,"\t","\n","\x0b",'
           '"\x0c","\r",\x0e,\x0f,\x10,\x11,\x12,\x13,\x14,\x15,\x16,'
           '\x17,\x18,\x19,\x1a,\x1b,\x1c,\x1d,\x1e,\x1f," ",!,"""",#,'
           '$,%,&,\',"(",")",*,+,",",-,.,/,0,1,2,3,4,5,6,7,8,9,:,;,<,=,>,?,'
           '@,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z,[,"\\\\",],'
           '^,_,`,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z,{,|,},'
           '~,\x7f)',
           map(chr, range(1, 128)))
        ok('(,"\x01\x02\x03\x04\x05\x06\x07\x08\t\n\x0b\x0c\r\x0e\x0f'
           '\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f !'
           '""#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\\\]'
           '^_`abcdefghijklmnopqrstuvwxyz{|}~\x7f")',
           [None, ''.join(map(chr, range(1, 128)))])

    @skip_if_no_composite
    def test_cast_composite(self):
        oid = self._create_type("type_isd",
            [('anint', 'integer'), ('astring', 'text'), ('adate', 'date')])

        t = psycopg2.extras.register_composite("type_isd", self.conn)
        self.assertEqual(t.name, 'type_isd')
        self.assertEqual(t.oid, oid)
        self.assert_(issubclass(t.type, tuple))
        self.assertEqual(t.attnames, ['anint', 'astring', 'adate'])
        self.assertEqual(t.atttypes, [23,25,1082])

        curs = self.conn.cursor()
        r = (10, 'hello', date(2011,1,2))
        curs.execute("select %s::type_isd;", (r,))
        v = curs.fetchone()[0]
        self.assert_(isinstance(v, t.type))
        self.assertEqual(v[0], 10)
        self.assertEqual(v[1], "hello")
        self.assertEqual(v[2], date(2011,1,2))

        try:
            from collections import namedtuple
        except ImportError:
            pass
        else:
            self.assert_(t.type is not tuple)
            self.assertEqual(v.anint, 10)
            self.assertEqual(v.astring, "hello")
            self.assertEqual(v.adate, date(2011,1,2))

    @skip_if_no_composite
    def test_cast_nested(self):
        self._create_type("type_is",
            [("anint", "integer"), ("astring", "text")])
        self._create_type("type_r_dt",
            [("adate", "date"), ("apair", "type_is")])
        self._create_type("type_r_ft",
            [("afloat", "float8"), ("anotherpair", "type_r_dt")])

        psycopg2.extras.register_composite("type_is", self.conn)
        psycopg2.extras.register_composite("type_r_dt", self.conn)
        psycopg2.extras.register_composite("type_r_ft", self.conn)

        curs = self.conn.cursor()
        r = (0.25, (date(2011,1,2), (42, "hello")))
        curs.execute("select %s::type_r_ft;", (r,))
        v = curs.fetchone()[0]

        self.assertEqual(r, v)

        try:
            from collections import namedtuple
        except ImportError:
            pass
        else:
            self.assertEqual(v.anotherpair.apair.astring, "hello")

    @skip_if_no_composite
    def test_register_on_cursor(self):
        self._create_type("type_ii", [("a", "integer"), ("b", "integer")])

        curs1 = self.conn.cursor()
        curs2 = self.conn.cursor()
        psycopg2.extras.register_composite("type_ii", curs1)
        curs1.execute("select (1,2)::type_ii")
        self.assertEqual(curs1.fetchone()[0], (1,2))
        curs2.execute("select (1,2)::type_ii")
        self.assertEqual(curs2.fetchone()[0], "(1,2)")

    @skip_if_no_composite
    def test_register_on_connection(self):
        self._create_type("type_ii", [("a", "integer"), ("b", "integer")])

        conn1 = psycopg2.connect(dsn)
        conn2 = psycopg2.connect(dsn)
        try:
            psycopg2.extras.register_composite("type_ii", conn1)
            curs1 = conn1.cursor()
            curs2 = conn2.cursor()
            curs1.execute("select (1,2)::type_ii")
            self.assertEqual(curs1.fetchone()[0], (1,2))
            curs2.execute("select (1,2)::type_ii")
            self.assertEqual(curs2.fetchone()[0], "(1,2)")
        finally:
            conn1.close()
            conn2.close()

    @skip_if_no_composite
    def test_register_globally(self):
        self._create_type("type_ii", [("a", "integer"), ("b", "integer")])

        conn1 = psycopg2.connect(dsn)
        conn2 = psycopg2.connect(dsn)
        try:
            t = psycopg2.extras.register_composite("type_ii", conn1, globally=True)
            try:
                curs1 = conn1.cursor()
                curs2 = conn2.cursor()
                curs1.execute("select (1,2)::type_ii")
                self.assertEqual(curs1.fetchone()[0], (1,2))
                curs2.execute("select (1,2)::type_ii")
                self.assertEqual(curs2.fetchone()[0], (1,2))
            finally:
                del psycopg2.extensions.string_types[t.oid]

        finally:
            conn1.close()
            conn2.close()

    @skip_if_no_composite
    def test_composite_namespace(self):
        curs = self.conn.cursor()
        curs.execute("""
            select nspname from pg_namespace
            where nspname = 'typens';
            """)
        if not curs.fetchone():
            curs.execute("create schema typens;")
            self.conn.commit()

        self._create_type("typens.typens_ii",
            [("a", "integer"), ("b", "integer")])
        t = psycopg2.extras.register_composite(
            "typens.typens_ii", self.conn)
        curs.execute("select (4,8)::typens.typens_ii")
        self.assertEqual(curs.fetchone()[0], (4,8))

    @skip_if_no_composite
    @skip_before_postgres(8, 4)
    def test_composite_array(self):
        oid = self._create_type("type_isd",
            [('anint', 'integer'), ('astring', 'text'), ('adate', 'date')])

        t = psycopg2.extras.register_composite("type_isd", self.conn)

        curs = self.conn.cursor()
        r1 = (10, 'hello', date(2011,1,2))
        r2 = (20, 'world', date(2011,1,3))
        curs.execute("select %s::type_isd[];", ([r1, r2],))
        v = curs.fetchone()[0]
        self.assertEqual(len(v), 2)
        self.assert_(isinstance(v[0], t.type))
        self.assertEqual(v[0][0], 10)
        self.assertEqual(v[0][1], "hello")
        self.assertEqual(v[0][2], date(2011,1,2))
        self.assert_(isinstance(v[1], t.type))
        self.assertEqual(v[1][0], 20)
        self.assertEqual(v[1][1], "world")
        self.assertEqual(v[1][2], date(2011,1,3))

    def _create_type(self, name, fields):
        curs = self.conn.cursor()
        try:
            curs.execute("drop type %s cascade;" % name)
        except psycopg2.ProgrammingError:
            self.conn.rollback()

        curs.execute("create type %s as (%s);" % (name,
            ", ".join(["%s %s" % p for p in fields])))
        if '.' in name:
            schema, name = name.split('.')
        else:
            schema = 'public'

        curs.execute("""\
            SELECT t.oid
            FROM pg_type t JOIN pg_namespace ns ON typnamespace = ns.oid
            WHERE typname = %s and nspname = %s;
            """, (name, schema))
        oid = curs.fetchone()[0]
        self.conn.commit()
        return oid


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()

