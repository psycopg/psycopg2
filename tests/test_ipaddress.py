#!/usr/bin/env python
#
# test_ipaddress.py - tests for ipaddress support
#
# Copyright (C) 2016-2019 Daniele Varrazzo  <daniele.varrazzo@gmail.com>
# Copyright (C) 2020 The Psycopg Team
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

from __future__ import unicode_literals

from . import testutils
import unittest

import psycopg2
import psycopg2.extras

try:
    import ipaddress as ip
except ImportError:
    # Python 2
    ip = None


@unittest.skipIf(ip is None, "'ipaddress' module not available")
class NetworkingTestCase(testutils.ConnectingTestCase):
    def test_inet_cast(self):
        cur = self.conn.cursor()
        psycopg2.extras.register_ipaddress(cur)

        cur.execute("select null::inet")
        self.assert_(cur.fetchone()[0] is None)

        cur.execute("select '127.0.0.1/24'::inet")
        obj = cur.fetchone()[0]
        self.assert_(isinstance(obj, ip.IPv4Interface), repr(obj))
        self.assertEquals(obj, ip.ip_interface('127.0.0.1/24'))

        cur.execute("select '::ffff:102:300/128'::inet")
        obj = cur.fetchone()[0]
        self.assert_(isinstance(obj, ip.IPv6Interface), repr(obj))
        self.assertEquals(obj, ip.ip_interface('::ffff:102:300/128'))

    @testutils.skip_before_postgres(8, 2)
    def test_inet_array_cast(self):
        cur = self.conn.cursor()
        psycopg2.extras.register_ipaddress(cur)
        cur.execute("select '{NULL,127.0.0.1,::ffff:102:300/128}'::inet[]")
        l = cur.fetchone()[0]
        self.assert_(l[0] is None)
        self.assertEquals(l[1], ip.ip_interface('127.0.0.1'))
        self.assertEquals(l[2], ip.ip_interface('::ffff:102:300/128'))
        self.assert_(isinstance(l[1], ip.IPv4Interface), l)
        self.assert_(isinstance(l[2], ip.IPv6Interface), l)

    def test_inet_adapt(self):
        cur = self.conn.cursor()
        psycopg2.extras.register_ipaddress(cur)

        cur.execute("select %s", [ip.ip_interface('127.0.0.1/24')])
        self.assertEquals(cur.fetchone()[0], '127.0.0.1/24')

        cur.execute("select %s", [ip.ip_interface('::ffff:102:300/128')])
        self.assertEquals(cur.fetchone()[0], '::ffff:102:300/128')

    def test_cidr_cast(self):
        cur = self.conn.cursor()
        psycopg2.extras.register_ipaddress(cur)

        cur.execute("select null::cidr")
        self.assert_(cur.fetchone()[0] is None)

        cur.execute("select '127.0.0.0/24'::cidr")
        obj = cur.fetchone()[0]
        self.assert_(isinstance(obj, ip.IPv4Network), repr(obj))
        self.assertEquals(obj, ip.ip_network('127.0.0.0/24'))

        cur.execute("select '::ffff:102:300/128'::cidr")
        obj = cur.fetchone()[0]
        self.assert_(isinstance(obj, ip.IPv6Network), repr(obj))
        self.assertEquals(obj, ip.ip_network('::ffff:102:300/128'))

    @testutils.skip_before_postgres(8, 2)
    def test_cidr_array_cast(self):
        cur = self.conn.cursor()
        psycopg2.extras.register_ipaddress(cur)
        cur.execute("select '{NULL,127.0.0.1,::ffff:102:300/128}'::cidr[]")
        l = cur.fetchone()[0]
        self.assert_(l[0] is None)
        self.assertEquals(l[1], ip.ip_network('127.0.0.1'))
        self.assertEquals(l[2], ip.ip_network('::ffff:102:300/128'))
        self.assert_(isinstance(l[1], ip.IPv4Network), l)
        self.assert_(isinstance(l[2], ip.IPv6Network), l)

    def test_cidr_adapt(self):
        cur = self.conn.cursor()
        psycopg2.extras.register_ipaddress(cur)

        cur.execute("select %s", [ip.ip_network('127.0.0.0/24')])
        self.assertEquals(cur.fetchone()[0], '127.0.0.0/24')

        cur.execute("select %s", [ip.ip_network('::ffff:102:300/128')])
        self.assertEquals(cur.fetchone()[0], '::ffff:102:300/128')


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)


if __name__ == "__main__":
    unittest.main()
