#!/usr/bin/env python
from testutils import unittest

import psycopg2
from psycopg2 import extensions

import time
import select
import signal
from subprocess import Popen, PIPE

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

    def notify(self, name, sec=0, payload=None):
        """Send a notification to the database, eventually after some time."""
        if payload is None:
            payload = ''
        else:
            payload = ", %r" % payload

        script = ("""\
import time
time.sleep(%(sec)s)
import psycopg2
import psycopg2.extensions
conn = psycopg2.connect(%(dsn)r)
conn.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT)
print conn.get_backend_pid()
curs = conn.cursor()
curs.execute("NOTIFY " %(name)r %(payload)r)
curs.close()
conn.close()
"""
            % { 'dsn': tests.dsn, 'sec': sec, 'name': name, 'payload': payload})

        return Popen([sys.executable, '-c', script], stdout=PIPE)

    def test_notifies_received_on_poll(self):
        self.autocommit(self.conn)
        self.listen('foo')

        proc = self.notify('foo', 1)

        t0 = time.time()
        ready = select.select([self.conn], [], [], 5)
        t1 = time.time()
        self.assert_(0.99 < t1 - t0 < 4, t1 - t0)

        pid = int(proc.communicate()[0])
        self.assertEqual(0, len(self.conn.notifies))
        self.assertEqual(extensions.POLL_OK, self.conn.poll())
        self.assertEqual(1, len(self.conn.notifies))
        self.assertEqual(pid, self.conn.notifies[0][0])
        self.assertEqual('foo', self.conn.notifies[0][1])

    def test_many_notifies(self):
        self.autocommit(self.conn)
        for name in ['foo', 'bar', 'baz']:
            self.listen(name)

        pids = {}
        for name in ['foo', 'bar', 'baz', 'qux']:
            pids[name] = int(self.notify(name).communicate()[0])

        self.assertEqual(0, len(self.conn.notifies))
        for i in range(10):
            self.assertEqual(extensions.POLL_OK, self.conn.poll())
        self.assertEqual(3, len(self.conn.notifies))

        names = dict.fromkeys(['foo', 'bar', 'baz'])
        for (pid, name) in self.conn.notifies:
            self.assertEqual(pids[name], pid)
            names.pop(name) # raise if name found twice

    def test_notifies_received_on_execute(self):
        self.autocommit(self.conn)
        self.listen('foo')
        pid = int(self.notify('foo').communicate()[0])
        self.assertEqual(0, len(self.conn.notifies))
        self.conn.cursor().execute('select 1;')
        self.assertEqual(1, len(self.conn.notifies))
        self.assertEqual(pid, self.conn.notifies[0][0])
        self.assertEqual('foo', self.conn.notifies[0][1])

    def test_notify_object(self):
        self.autocommit(self.conn)
        self.listen('foo')
        self.notify('foo').communicate()
        time.sleep(0.1)
        self.conn.poll()
        notify = self.conn.notifies[0]
        self.assert_(isinstance(notify, psycopg2.extensions.Notify))

    def test_notify_attributes(self):
        self.autocommit(self.conn)
        self.listen('foo')
        pid = int(self.notify('foo').communicate()[0])
        time.sleep(0.1)
        self.conn.poll()
        self.assertEqual(1, len(self.conn.notifies))
        notify = self.conn.notifies[0]
        self.assertEqual(pid, notify.pid)
        self.assertEqual('foo', notify.channel)
        self.assertEqual('', notify.payload)

    def test_notify_payload(self):
        if self.conn.server_version < 90000:
            return self.skipTest("server version %s doesn't support notify payload"
                % self.conn.server_version)
        self.autocommit(self.conn)
        self.listen('foo')
        pid = int(self.notify('foo', payload="Hello, world!").communicate()[0])
        time.sleep(0.1)
        self.conn.poll()
        self.assertEqual(1, len(self.conn.notifies))
        notify = self.conn.notifies[0]
        self.assertEqual(pid, notify.pid)
        self.assertEqual('foo', notify.channel)
        self.assertEqual('Hello, world!', notify.payload)

    def test_notify_init(self):
        n = psycopg2.extensions.Notify(10, 'foo')
        self.assertEqual(10, n.pid)
        self.assertEqual('foo', n.channel)
        self.assertEqual('', n.payload)
        (pid, channel) = n
        self.assertEqual((pid, channel), (10, 'foo'))

        n = psycopg2.extensions.Notify(42, 'bar', 'baz')
        self.assertEqual(42, n.pid)
        self.assertEqual('bar', n.channel)
        self.assertEqual('baz', n.payload)
        (pid, channel) = n
        self.assertEqual((pid, channel), (42, 'bar'))

    def test_compare(self):
        data = [(10, 'foo'), (20, 'foo'), (10, 'foo', 'bar'), (10, 'foo', 'baz')]
        for d1 in data:
            for d2 in data:
                n1 = psycopg2.extensions.Notify(*d1)
                n2 = psycopg2.extensions.Notify(*d2)
                self.assertEqual((n1 == n2), (d1 == d2))
                self.assertEqual((n1 != n2), (d1 != d2))

    def test_compare_tuple(self):
        from psycopg2.extensions import Notify
        self.assertEqual((10, 'foo'), Notify(10, 'foo'))
        self.assertEqual((10, 'foo'), Notify(10, 'foo', 'bar'))
        self.assertNotEqual((10, 'foo'), Notify(20, 'foo'))
        self.assertNotEqual((10, 'foo'), Notify(10, 'bar'))

    def test_hash(self):
        from psycopg2.extensions import Notify
        self.assertEqual(hash((10, 'foo')), hash(Notify(10, 'foo')))
        self.assertNotEqual(hash(Notify(10, 'foo', 'bar')),
            hash(Notify(10, 'foo')))

def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()

