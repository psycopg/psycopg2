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
    from unittest2 import skip, skipIf

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
            
