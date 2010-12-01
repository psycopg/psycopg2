# Utility module for psycopg2 testing.
# 
# Copyright (C) 2010 Daniele Varrazzo <daniele.varrazzo@gmail.com>

# Use unittest2 if available. Otherwise mock a skip facility with warnings.

try:
    import unittest2
    unittest = unittest2
except ImportError:
    import unittest
    unittest2 = None

if hasattr(unittest, 'skipIf'):
    skip = unittest.skip
    skipIf = unittest.skipIf

else:
    import warnings

    def skipIf(cond, msg):
        def skipIf_(f):
            def skipIf__(self):
                if cond:
                    warnings.warn(msg)
                    return
                else:
                    return f(self)
            return skipIf__
        return skipIf_

    def skip(msg):
        return skipIf(True, msg)

    def skipTest(self, msg):
        warnings.warn(msg)
        return

    unittest.TestCase.skipTest = skipTest


def decorate_all_tests(cls, decorator):
    """Apply *decorator* to all the tests defined in the TestCase *cls*."""
    for n in dir(cls):
        if n.startswith('test'):
            setattr(cls, n, decorator(getattr(cls, n)))


def skip_if_no_pg_sleep(name):
    """Decorator to skip a test if pg_sleep is not supported by the server.

    Pass it the name of an attribute containing a connection or of a method
    returning a connection.
    """
    def skip_if_no_pg_sleep_(f):
        def skip_if_no_pg_sleep__(self):
            cnn = getattr(self, name)
            if callable(cnn):
                cnn = cnn()

            if cnn.server_version < 80100:
                return self.skipTest(
                    "server version %s doesn't support pg_sleep"
                    % cnn.server_version)

            return f(self)

        skip_if_no_pg_sleep__.__name__ = f.__name__
        return skip_if_no_pg_sleep__

    return skip_if_no_pg_sleep_
