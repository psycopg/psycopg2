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


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)


if __name__ == "__main__":
    unittest.main()
