import unittest

import psycopg2
import psycopg2.extensions
import tests


class LargeObjectTests(unittest.TestCase):

    def setUp(self):
        self.conn = psycopg2.connect(tests.dsn)
        self.lo_oid = None

    def tearDown(self):
        if self.lo_oid is not None:
            self.conn.lobject(self.lo_oid).unlink()

    def test_create(self):
        lo = self.conn.lobject()
        self.assertNotEqual(lo, None)
        self.assertEqual(lo.mode, "w")

    def test_open_non_existent(self):
        # This test will give a false negative if Oid 42 is a large object.
        self.assertRaises(psycopg2.OperationalError, self.conn.lobject, 42)

    def test_open_existing(self):
        lo = self.conn.lobject()
        lo2 = self.conn.lobject(lo.oid)
        self.assertNotEqual(lo2, None)
        self.assertEqual(lo2.oid, lo.oid)
        self.assertEqual(lo2.mode, "r")

    def test_close(self):
        lo = self.conn.lobject()
        self.assertEqual(lo.closed, False)
        lo.close()
        self.assertEqual(lo.closed, True)

    def test_write(self):
        lo = self.conn.lobject()
        self.assertEqual(lo.write("some data"), len("some data"))

    def test_seek_tell(self):
        lo = self.conn.lobject()
        length = lo.write("some data")
        self.assertEqual(lo.tell(), length)
        lo.close()
        lo = self.conn.lobject(lo.oid)

        self.assertEqual(lo.seek(5, 0), 5)
        self.assertEqual(lo.tell(), 5)

        # SEEK_CUR: relative current location
        self.assertEqual(lo.seek(2, 1), 7)
        self.assertEqual(lo.tell(), 7)

        # SEEK_END: relative to end of file
        self.assertEqual(lo.seek(-2, 2), length - 2)

    def test_read(self):
        lo = self.conn.lobject()
        length = lo.write("some data")
        lo.close()

        lo = self.conn.lobject(lo.oid)
        self.assertEqual(lo.read(4), "some")
        self.assertEqual(lo.read(), " data")

    def test_unlink(self):
        lo = self.conn.lobject()
        lo.close()
        lo.unlink()

        # the object doesn't exist now, so we can't reopen it.
        self.assertRaises(psycopg2.OperationalError, self.conn.lobject, lo.oid)


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)
