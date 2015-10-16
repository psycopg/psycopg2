#!/usr/bin/env python

# test_replication.py - unit test for replication protocol
#
# Copyright (C) 2015 Daniele Varrazzo  <daniele.varrazzo@gmail.com>
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

import psycopg2
import psycopg2.extensions
from psycopg2.extras import PhysicalReplicationConnection, LogicalReplicationConnection

from testutils import unittest
from testutils import skip_before_postgres
from testutils import ConnectingTestCase


class ReplicationTestCase(ConnectingTestCase):
    def setUp(self):
        from testconfig import repl_dsn
        if not repl_dsn:
            self.skipTest("replication tests disabled by default")
        super(ReplicationTestCase, self).setUp()
        self._slots = []

    def tearDown(self):
        # first close all connections, as they might keep the slot(s) active
        super(ReplicationTestCase, self).tearDown()

        if self._slots:
            kill_conn = self.repl_connect(connection_factory=PhysicalReplicationConnection)
            if kill_conn:
                kill_cur = kill_conn.cursor()
                for slot in self._slots:
                    kill_cur.drop_replication_slot(slot)
                kill_conn.close()

    def create_replication_slot(self, cur, slot_name, **kwargs):
        cur.create_replication_slot(slot_name, **kwargs)
        self._slots.append(slot_name)

    def drop_replication_slot(self, cur, slot_name):
        cur.drop_replication_slot(slot_name)
        self._slots.remove(slot_name)


class ReplicationTest(ReplicationTestCase):
    @skip_before_postgres(9, 0)
    def test_physical_replication_connection(self):
        conn = self.repl_connect(connection_factory=PhysicalReplicationConnection)
        if conn is None: return
        cur = conn.cursor()
        cur.execute("IDENTIFY_SYSTEM")
        cur.fetchall()

    @skip_before_postgres(9, 4)
    def test_logical_replication_connection(self):
        conn = self.repl_connect(connection_factory=LogicalReplicationConnection)
        if conn is None: return
        cur = conn.cursor()
        cur.execute("IDENTIFY_SYSTEM")
        cur.fetchall()

    @skip_before_postgres(9, 0)
    def test_stop_replication_raises(self):
        conn = self.repl_connect(connection_factory=PhysicalReplicationConnection)
        if conn is None: return
        cur = conn.cursor()
        self.assertRaises(psycopg2.ProgrammingError, cur.stop_replication)

        cur.start_replication()
        cur.stop_replication() # doesn't raise now

        def consume(msg):
            pass
        cur.consume_replication_stream(consume) # should return at once

    @skip_before_postgres(9, 4) # slots require 9.4
    def test_create_replication_slot(self):
        conn = self.repl_connect(connection_factory=PhysicalReplicationConnection)
        if conn is None: return
        cur = conn.cursor()

        slot = "test_slot1"

        self.create_replication_slot(cur, slot)
        self.assertRaises(psycopg2.ProgrammingError, self.create_replication_slot, cur, slot)

    @skip_before_postgres(9, 4) # slots require 9.4
    def test_start_on_missing_replication_slot(self):
        conn = self.repl_connect(connection_factory=PhysicalReplicationConnection)
        if conn is None: return
        cur = conn.cursor()

        slot = "test_slot1"

        self.assertRaises(psycopg2.ProgrammingError, cur.start_replication, slot)

        self.create_replication_slot(cur, slot)
        cur.start_replication(slot)


class AsyncReplicationTest(ReplicationTestCase):
    @skip_before_postgres(9, 4)
    def test_async_replication(self):
        conn = self.repl_connect(connection_factory=LogicalReplicationConnection, async=1)
        if conn is None: return
        self.wait(conn)
        cur = conn.cursor()

        slot = "test_slot1"
        self.create_replication_slot(cur, slot, output_plugin='test_decoding')
        self.wait(cur)

        cur.start_replication(slot)
        self.wait(cur)


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()
