#!/usr/bin/env python
import unittest

import psycopg2
from psycopg2 import extensions

import time
import select
import signal
from subprocess import Popen

import sys
if sys.version_info < (3,):
    import tests
else:
    import py3tests as tests


class NotifiesTests(unittest.TestCase):

    def setUp(self):
        self.conn = psycopg2.connect(tests.dsn)

    def tearDown(self):
        self.conn.close()

    def autocommit(self, conn):
        """Set a connection in autocommit mode."""
        conn.set_isolation_level(extensions.ISOLATION_LEVEL_AUTOCOMMIT)

    def listen(self, name):
        """Start listening for a name on self.conn."""
        curs = self.conn.cursor()
        curs.execute("LISTEN " + name)
        curs.close()

    def notify(self, name, sec=0):
        """Send a notification to the database, eventually after some time."""
        script = ("""\
import time
time.sleep(%(sec)s)
import psycopg2
import psycopg2.extensions
conn = psycopg2.connect(%(dsn)r)
conn.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT)
curs = conn.cursor()
curs.execute("NOTIFY " %(name)r)
curs.close()
conn.close()
"""
            % { 'dsn': tests.dsn, 'sec': sec, 'name': name})

        return Popen([sys.executable, '-c', script])

    def test_notifies_received_on_poll(self):
        self.autocommit(self.conn)
        self.listen('foo')

        self.notify('foo', 1);

        t0 = time.time()
        ready = select.select([self.conn], [], [], 5)
        t1 = time.time()
        self.assert_(0.99 < t1 - t0 < 4, t1 - t0)

        self.assertEqual(0, len(self.conn.notifies))
        self.assertEqual(extensions.POLL_OK, self.conn.poll())
        self.assertEqual(1, len(self.conn.notifies))
        self.assertEqual('foo', self.conn.notifies[0][1])

    def test_many_notifies(self):
        self.autocommit(self.conn)
        for name in ['foo', 'bar', 'baz']:
            self.listen(name)

        for name in ['foo', 'bar', 'baz', 'qux']:
            self.notify(name).wait()

        self.assertEqual(0, len(self.conn.notifies))
        self.assertEqual(extensions.POLL_OK, self.conn.poll())
        self.assertEqual(3, len(self.conn.notifies))
        names = [n[1] for n in self.conn.notifies]
        for name in ['foo', 'bar', 'baz']:
            self.assert_(name in names, name)

    def test_notifies_received_on_execute(self):
        self.autocommit(self.conn)
        self.listen('foo')
        self.notify('foo').wait()
        self.assertEqual(0, len(self.conn.notifies))
        self.conn.cursor().execute('select 1;')
        self.assertEqual(1, len(self.conn.notifies))
        self.assertEqual('foo', self.conn.notifies[0][1])

def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()

