import os
import shutil
import tempfile
import unittest

import psycopg2
import psycopg2.extensions
import tests


class LargeObjectTests(unittest.TestCase):

    def setUp(self):
        self.conn = psycopg2.connect(tests.dsn)
        self.lo_oid = None
        self.tmpdir = None

    def tearDown(self):
        if self.tmpdir:
            shutil.rmtree(self.tmpdir, ignore_errors=True)
        if self.lo_oid is not None:
            self.conn.rollback()
            try:
                lo = self.conn.lobject(self.lo_oid, "n")
            except psycopg2.OperationalError:
                pass
            else:
                lo.unlink()

    def test_create(self):
        lo = self.conn.lobject()
        self.assertNotEqual(lo, None)
        self.assertEqual(lo.mode, "w")

    def test_open_non_existent(self):
        # By creating then removing a large object, we get an Oid that
        # should be unused.
        lo = self.conn.lobject()
        lo.unlink()
        self.assertRaises(psycopg2.OperationalError, self.conn.lobject, lo.oid)

    def test_open_existing(self):
        lo = self.conn.lobject()
        lo2 = self.conn.lobject(lo.oid)
        self.assertNotEqual(lo2, None)
        self.assertEqual(lo2.oid, lo.oid)
        self.assertEqual(lo2.mode, "r")

    def test_open_for_write(self):
        lo = self.conn.lobject()
        lo2 = self.conn.lobject(lo.oid, "w")
        self.assertEqual(lo2.mode, "w")
        lo2.write("some data")

    def test_open_mode_n(self):
        # Openning an object in mode "n" gives us a closed lobject.
        lo = self.conn.lobject()
        lo.close()

        lo2 = self.conn.lobject(lo.oid, "n")
        self.assertEqual(lo2.oid, lo.oid)
        self.assertEqual(lo2.closed, True)

    def test_create_with_oid(self):
        # Create and delete a large object to get an unused Oid.
        lo = self.conn.lobject()
        oid = lo.oid
        lo.unlink()

        lo = self.conn.lobject(0, "w", oid)
        self.assertEqual(lo.oid, oid)

    def test_create_with_existing_oid(self):
        lo = self.conn.lobject()
        lo.close()

        self.assertRaises(psycopg2.OperationalError,
                          self.conn.lobject, 0, "w", lo.oid)

    def test_import(self):
        self.tmpdir = tempfile.mkdtemp()
        filename = os.path.join(self.tmpdir, "data.txt")
        fp = open(filename, "wb")
        fp.write("some data")
        fp.close()

        lo = self.conn.lobject(0, "r", 0, filename)
        self.assertEqual(lo.read(), "some data")

    def test_close(self):
        lo = self.conn.lobject()
        self.assertEqual(lo.closed, False)
        lo.close()
        self.assertEqual(lo.closed, True)

    def test_write(self):
        lo = self.conn.lobject()
        self.assertEqual(lo.write("some data"), len("some data"))

    def test_read(self):
        lo = self.conn.lobject()
        length = lo.write("some data")
        lo.close()

        lo = self.conn.lobject(lo.oid)
        self.assertEqual(lo.read(4), "some")
        self.assertEqual(lo.read(), " data")

    def test_seek_tell(self):
        lo = self.conn.lobject()
        length = lo.write("some data")
        self.assertEqual(lo.tell(), length)
        lo.close()
        lo = self.conn.lobject(lo.oid)

        self.assertEqual(lo.seek(5, 0), 5)
        self.assertEqual(lo.tell(), 5)
        self.assertEqual(lo.read(), "data")

        # SEEK_CUR: relative current location
        lo.seek(5)
        self.assertEqual(lo.seek(2, 1), 7)
        self.assertEqual(lo.tell(), 7)
        self.assertEqual(lo.read(), "ta")

        # SEEK_END: relative to end of file
        self.assertEqual(lo.seek(-2, 2), length - 2)
        self.assertEqual(lo.read(), "ta")

    def test_unlink(self):
        lo = self.conn.lobject()
        lo.unlink()

        # the object doesn't exist now, so we can't reopen it.
        self.assertRaises(psycopg2.OperationalError, self.conn.lobject, lo.oid)
        # And the object has been closed.
        self.assertEquals(lo.closed, True)

    def test_export(self):
        lo = self.conn.lobject()
        lo.write("some data")

        self.tmpdir = tempfile.mkdtemp()
        filename = os.path.join(self.tmpdir, "data.txt")
        lo.export(filename)
        self.assertTrue(os.path.exists(filename))
        self.assertEqual(open(filename, "rb").read(), "some data")

    def test_close_twice(self):
        lo = self.conn.lobject()
        lo.close()
        lo.close()

    def test_write_after_close(self):
        lo = self.conn.lobject()
        lo.close()
        self.assertRaises(psycopg2.InterfaceError, lo.write, "some data")

    def test_read_after_close(self):
        lo = self.conn.lobject()
        lo.close()
        self.assertRaises(psycopg2.InterfaceError, lo.read, 5)

    def test_seek_after_close(self):
        lo = self.conn.lobject()
        lo.close()
        self.assertRaises(psycopg2.InterfaceError, lo.seek, 0)

    def test_tell_after_close(self):
        lo = self.conn.lobject()
        lo.close()
        self.assertRaises(psycopg2.InterfaceError, lo.tell)

    def test_unlink_after_close(self):
        lo = self.conn.lobject()
        lo.close()
        # Unlink works on closed files.
        lo.unlink()

    def test_export_after_close(self):
        lo = self.conn.lobject()
        lo.write("some data")
        lo.close()

        self.tmpdir = tempfile.mkdtemp()
        filename = os.path.join(self.tmpdir, "data.txt")
        lo.export(filename)
        self.assertTrue(os.path.exists(filename))
        self.assertEqual(open(filename, "rb").read(), "some data")

    def test_close_after_commit(self):
        lo = self.conn.lobject()
        self.lo_oid = lo.oid
        self.conn.commit()

        # Closing outside of the transaction is okay.
        lo.close()

    def test_write_after_commit(self):
        lo = self.conn.lobject()
        self.lo_oid = lo.oid
        self.conn.commit()

        self.assertRaises(psycopg2.ProgrammingError, lo.write, "some data")

    def test_read_after_commit(self):
        lo = self.conn.lobject()
        self.lo_oid = lo.oid
        self.conn.commit()

        self.assertRaises(psycopg2.ProgrammingError, lo.read, 5)

    def test_seek_after_commit(self):
        lo = self.conn.lobject()
        self.lo_oid = lo.oid
        self.conn.commit()

        self.assertRaises(psycopg2.ProgrammingError, lo.seek, 0)

    def test_tell_after_commit(self):
        lo = self.conn.lobject()
        self.lo_oid = lo.oid
        self.conn.commit()

        self.assertRaises(psycopg2.ProgrammingError, lo.tell)

    def test_unlink_after_commit(self):
        lo = self.conn.lobject()
        self.lo_oid = lo.oid
        self.conn.commit()

        # Unlink of stale lobject is okay
        lo.unlink()

    def test_export_after_commit(self):
        lo = self.conn.lobject()
        lo.write("some data")
        self.conn.commit()

        self.tmpdir = tempfile.mkdtemp()
        filename = os.path.join(self.tmpdir, "data.txt")
        lo.export(filename)
        self.assertTrue(os.path.exists(filename))
        self.assertEqual(open(filename, "rb").read(), "some data")


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)
