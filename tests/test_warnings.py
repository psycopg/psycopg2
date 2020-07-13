import re
import subprocess
import sys
import unittest
import warnings

import psycopg2

from .testconfig import dsn
from .testutils import skip_before_python


class WarningsTest(unittest.TestCase):
    @skip_before_python(3)
    def test_connection_not_closed(self):
        def f():
            psycopg2.connect(dsn)

        msg = (
            "^unclosed connection <connection object at 0x[0-9a-fA-F]+; dsn: '.*', "
            "closed: 0>$"
        )
        with self.assertWarnsRegex(ResourceWarning, msg):
            f()

    @skip_before_python(3)
    def test_cursor_not_closed(self):
        def f():
            conn = psycopg2.connect(dsn)
            try:
                conn.cursor()
            finally:
                conn.close()

        msg = (
            "^unclosed cursor <cursor object at 0x[0-9a-fA-F]+; closed: 0> for "
            "connection <connection object at 0x[0-9a-fA-F]+; dsn: '.*', closed: 0>$"
        )
        with self.assertWarnsRegex(ResourceWarning, msg):
            f()

    def test_cursor_factory_returns_non_cursor(self):
        def bad_factory(*args, **kwargs):
            return object()

        def f():
            conn = psycopg2.connect(dsn)
            try:
                conn.cursor(cursor_factory=bad_factory)
            finally:
                conn.close()

        with warnings.catch_warnings(record=True) as cm:
            with self.assertRaises(TypeError):
                f()

        # No warning as no cursor was instantiated.
        self.assertEquals(cm, [])

    @skip_before_python(3)
    def test_broken_close(self):
        script = """
import psycopg2

class MyException(Exception):
    pass

class MyCurs(psycopg2.extensions.cursor):
    def close(self):
        raise MyException

def f():
    conn = psycopg2.connect(%(dsn)r)
    try:
        conn.cursor(cursor_factory=MyCurs, scrollable=True)
    finally:
        conn.close()

f()
""" % {"dsn": dsn}
        p = subprocess.Popen(
            [sys.executable, "-Walways", "-c", script],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        output, _ = p.communicate()
        output = output.decode()
        # Normalize line endings.
        output = "\n".join(output.splitlines())
        self.assertRegex(
            output,
            re.compile(
                r"^Exception ignored in: "
                r"<cursor object at 0x[0-9a-fA-F]+; closed: 0>$",
                re.M,
            ),
        )
        self.assertIn("\n__main__.MyException: \n", output)
        self.assertRegex(
            output,
            re.compile(
                r"ResourceWarning: unclosed cursor "
                r"<cursor object at 0x[0-9a-fA-F]+; closed: 0> "
                r"for connection "
                r"<connection object at 0x[0-9a-fA-F]+; dsn: '.*', closed: 0>$",
                re.M,
            ),
        )


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)


if __name__ == "__main__":
    unittest.main()
